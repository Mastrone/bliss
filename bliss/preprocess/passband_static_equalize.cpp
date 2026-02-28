#include <preprocess/passband_static_equalize.hpp>

#include <bland/ndarray.hpp>
#include <bland/ndarray_slice.hpp>
#include <bland/ops/ops.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cmath>

using namespace bliss;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions)
// ============================================================================
namespace {

    /**
     * @brief Validates the empirical data spectrum against the theoretical filter response.
     * @details Computes the mean power in the passband and the cutoff regions for both the 
     * empirical data and the theoretical filter response. It issues a warning if the 
     * observed power ratio deviates significantly from the expected theoretical ratio, 
     * which may indicate intrinsic recording quality issues leading to flared edges.
     * @param spectra The empirical data tensor from the coarse channel.
     * @param H The theoretical filter response tensor.
     */
    void validate_passband_ratios(bland::ndarray spectra, bland::ndarray H) {
        auto num_channels = spectra.size(1);
        
        bland::ndarray spectra_passband = bland::mean(spectra.slice(bland::slice_spec{1, num_channels/2 - 100, num_channels / 2 + 100}));
        bland::ndarray spectra_cutoff_lower = bland::mean(spectra.slice(bland::slice_spec{1, 0, 50}));
        bland::ndarray spectra_cutoff_upper = bland::mean(spectra.slice(bland::slice_spec{1, -50, -1}));
        
        auto spectra_passband_mean = spectra_passband.scalarize<float>();
        auto spectra_cutoff_upper_mean = spectra_cutoff_lower.scalarize<float>();
        auto spectra_cutoff_lower_mean = spectra_cutoff_upper.scalarize<float>();
        auto spectra_cutoff_mean = (spectra_cutoff_upper_mean + spectra_cutoff_lower_mean) / 2.0f;

        bland::ndarray H_passband = bland::mean(H.slice(bland::slice_spec{0, num_channels/2 - 100, num_channels / 2 + 100}));
        bland::ndarray H_cutoff_lower = bland::mean(H.slice(bland::slice_spec{0, 0, 50}));
        bland::ndarray H_cutoff_upper = bland::mean(H.slice(bland::slice_spec{0, -50, -1}));
        
        auto H_passband_mean = H_passband.scalarize<float>();
        auto H_cutoff_upper_mean = H_cutoff_lower.scalarize<float>();
        auto H_cutoff_lower_mean = H_cutoff_upper.scalarize<float>();
        auto H_ratio = H_passband_mean / ((H_cutoff_upper_mean + H_cutoff_lower_mean) / 2.0f);

        if (spectra_cutoff_mean > 1.05f * (spectra_passband_mean / H_ratio)) {
            fmt::print("WARN: the ratio ({}) of power between the passband ({}) and cutoff ({}) for data does not "
                       "match given ratio of power in the given filter response ({}). This is potentially a data "
                       "quality issue inherent to recording that will cause flared edges after correction which can "
                       "lead to false positives.\n",
                       spectra_passband_mean / spectra_cutoff_mean,
                       spectra_passband_mean,
                       spectra_cutoff_mean,
                       H_ratio);
        }
    }
}

// ============================================================================
// FILTER GENERATION IMPLEMENTATION
// ============================================================================

/**
 * @brief Generates a Hamming window of size N.
 * @param N The number of points in the window.
 * @return A vector containing the window coefficients.
 */
std::vector<float> hamming_window(int N) {
    auto N_float = static_cast<float>(N-1);
    constexpr float two_pi = 2 * M_PI;
    constexpr float a_0 = 0.54f;
    constexpr float a_1 = 0.46f;
    auto w = std::vector<float>();
    
    w.reserve(N);
    for (int n = 0; n < N; ++n) {
        w.push_back(a_0 - a_1 * std::cos((two_pi * n) / N_float));
    }

    return w;
}

/**
 * @brief Computes the normalized sinc function: sin(pi*x)/(pi*x).
 * @param t The input coordinate.
 * @return The evaluated sinc value.
 */
float sinc(float t) {
    auto x = static_cast<float>(M_PI)*t;
    // Taylor series approximation for small x to avoid division by zero instability
    if (std::fabs(x) < 0.01f) {
        return std::cos(x/2.0f) * std::cos(x/4.0f) * std::cos(x/8.0f);
    } else {
        return std::sin(x) / x;
    }
}

/**
 * @brief Generates a Sinc Low-Pass Filter kernel.
 * @param fc The cutoff frequency (normalized 0..1).
 * @param num_taps The number of filter taps.
 * @return A vector representing the filter kernel.
 */
std::vector<float> sinc_lpf(float fc, int num_taps) {
    std::vector<float> prototype_lpf;
    prototype_lpf.reserve(num_taps);
    auto half_taps = static_cast<float>(num_taps-1)/2.0f;
    
    for (int n=0; n < num_taps; ++n) {
        auto t = static_cast<float>(n) - half_taps;
        prototype_lpf.emplace_back(sinc(fc*t));
    }
    return prototype_lpf;
}

/**
 * @brief Designs a finite impulse response (FIR) filter.
 * @param num_taps Number of filter taps.
 * @param fc Cutoff frequency.
 * @param window String identifier for the windowing function.
 * @return A tensor containing the FIR filter coefficients.
 */
bland::ndarray bliss::firdes(int num_taps, float fc, std::string_view window) {
    // Hard code hamming for now as the default window
    auto h_prototype = sinc_lpf(fc, num_taps);
    auto w = hamming_window(num_taps); 

    auto h = bland::ndarray({num_taps}, bland::ndarray::datatype::float32, bland::ndarray::dev::cpu);
    auto h_ptr = h.data_ptr<float>();
    
    for (int n=0; n < num_taps; ++n) {
        h_ptr[n] = h_prototype[n] * w[n];
    }

    return h;
}

/**
 * @brief Generates the magnitude response for a coarse channelizer.
 * @param fine_per_coarse Number of fine channels per coarse channel.
 * @param num_coarse_channels Total number of coarse channels.
 * @param taps_per_channel Filter taps applied per channel.
 * @param window Windowing function identifier.
 * @param device_str Compute device identifier.
 * @return A tensor representing the normalized magnitude response.
 */
bland::ndarray bliss::gen_coarse_channel_response(int fine_per_coarse, int num_coarse_channels, int taps_per_channel, std::string window, std::string device_str) {
    int num_taps = taps_per_channel * num_coarse_channels;

    auto h = firdes(num_taps, 1.0f/static_cast<float>(num_coarse_channels), window);

    int64_t full_res_length = static_cast<int64_t>(num_coarse_channels * fine_per_coarse);
    auto h_padded = bland::zeros({full_res_length});

    h_padded.slice(bland::slice_spec{0, 0, h.size(0)}) = h;
    
    auto H = bland::fft_shift_mag_square(h_padded);

    auto number_coarse_channels_contributing = H.size(0)/fine_per_coarse - 1;
    auto slice_dims = bland::slice_spec{0, fine_per_coarse/2, full_res_length-fine_per_coarse/2};
    auto H_slice = H.slice(slice_dims);
    H = H_slice;

    H.reshape({number_coarse_channels_contributing, fine_per_coarse});
    H = bland::sum(H, {0}); 
    H = H/bland::max(H); 

    return H;
}

// ============================================================================
// PIPELINE TRANSFORM IMPLEMENTATIONS
// ============================================================================

coarse_channel bliss::equalize_passband_filter(coarse_channel cc, bland::ndarray H, bool validate) {
    if (validate) {
        validate_passband_ratios(cc.data(), H);
    }
    
    H = H.to(cc.device());
    cc.set_data(bland::divide(cc.data(), H));
    
    return cc;
}

coarse_channel bliss::equalize_passband_filter(coarse_channel cc, std::string_view h_resp_filepath, bland::ndarray::datatype dtype, bool validate) {
    auto h = bland::read_from_file(h_resp_filepath, dtype);
    return equalize_passband_filter(cc, h, validate);
}

scan bliss::equalize_passband_filter(scan sc, bland::ndarray h, bool validate) {
    sc.add_coarse_channel_transform([h, validate](coarse_channel cc) { return equalize_passband_filter(cc, h, validate); }, "equalize_passband");
    return sc;
}

scan bliss::equalize_passband_filter(scan sc, std::string_view h_resp_filepath, bland::ndarray::datatype dtype, bool validate) {
    auto h = bland::read_from_file(h_resp_filepath, dtype);
    return equalize_passband_filter(sc, h, validate);
}

observation_target bliss::equalize_passband_filter(observation_target ot, bland::ndarray h, bool validate) {
    for (auto &scan_data : ot._scans) {
        scan_data = equalize_passband_filter(scan_data, h, validate);
    }
    return ot;
}

observation_target bliss::equalize_passband_filter(observation_target ot, std::string_view h_resp_filepath, bland::ndarray::datatype dtype, bool validate) {
    auto h = bland::read_from_file(h_resp_filepath, dtype);
    
    bland::ndarray::dev target_device = bland::ndarray::dev::cpu;
    if (!ot._scans.empty()) {
        target_device = ot._scans[0].device();
    }
    
    h = h.to(target_device);
    return equalize_passband_filter(ot, h, validate);
}

cadence bliss::equalize_passband_filter(cadence ca, bland::ndarray h, bool validate) {
    for (auto &target : ca._observations) {
        target = equalize_passband_filter(target, h, validate);
    }
    return ca;
}

cadence bliss::equalize_passband_filter(cadence ca, std::string_view h_resp_filepath, bland::ndarray::datatype dtype, bool validate) {
    auto h = bland::read_from_file(h_resp_filepath, dtype);
    return equalize_passband_filter(ca, h, validate);
}