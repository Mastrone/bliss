#include "drift_search/integrate_drifts.hpp"
#include "kernels/drift_integration_bland.hpp"
#include "kernels/drift_integration_cpu.hpp"
#if BLISS_CUDA
#include "kernels/drift_integration_cuda.cuh"
#endif

#include <bland/bland.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <cmath>
#include <algorithm>

using namespace bliss;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions)
// ============================================================================
namespace {

    /**
     * @brief Computes the geometrical properties of a single drift path on the time-frequency grid.
     * @details Calculates the channel span, slope, and required desmearing width for a specific drift rate.
     * @param index The ordinal index of this drift rate in the search plane.
     * @param drift_rate The physical drift rate in Hz/sec.
     * @param maximum_drift_time_span The total number of time steps minus one.
     * @param tsamp The sampling interval in seconds.
     * @param foff_Hz The frequency resolution in Hz.
     * @param desmear_enabled Boolean flag to enable or disable desmearing compensation.
     * @return A populated frequency_drift_plane::drift_rate structure.
     */
    frequency_drift_plane::drift_rate compute_single_drift_geometry(
        int index, 
        double drift_rate, 
        int maximum_drift_time_span, 
        double tsamp, 
        double foff_Hz, 
        bool desmear_enabled) 
    {
        frequency_drift_plane::drift_rate rate;
        rate.index_in_plane = index;
        
        // Calculate total channel span for this drift rate over the full duration
        rate.drift_channels_span = std::lround(drift_rate * (maximum_drift_time_span * tsamp) / foff_Hz);

        // Calculate slope 'm' (channels per time step)
        auto m = static_cast<float>(rate.drift_channels_span) / static_cast<float>(maximum_drift_time_span);

        rate.drift_rate_slope = m;
        rate.drift_rate_Hz_per_sec = drift_rate;
        
        // Desmearing Logic:
        // If the slope > 1 (crosses >1 channel per time step), the signal is smeared.
        auto smeared_channels = std::round(std::abs(m));

        int desmear_binwidth = 1;
        if (desmear_enabled) {
            desmear_binwidth = std::max(1.0f, smeared_channels);
        }
        rate.desmeared_bins = desmear_binwidth;

        return rate;
    }
}

// ============================================================================
// DRIFT SEARCH GEOMETRY SETUP
// ============================================================================

/**
 * @brief Pre-calculates the geometry and metadata for all drift paths to be searched.
 * @details Converts physical boundaries into discrete grid steps and delegates individual
 * path construction to the geometry helper.
 * @param time_steps Number of time integrations in the input data.
 * @param foff Frequency resolution (MHz).
 * @param tsamp Time sampling interval (seconds).
 * @param options Configuration options (min/max drift).
 * @return A vector of `drift_rate` structs describing every path to be summed.
 */
std::vector<frequency_drift_plane::drift_rate> compute_drifts(int time_steps, double foff, double tsamp, integrate_drifts_options options) {
    auto maximum_drift_time_span = time_steps - 1;

    // Convert drift options to specific drift info units
    auto foff_Hz = std::abs(foff) * 1e6;
    double unit_drift_resolution = foff_Hz / (maximum_drift_time_span * tsamp);
    
    // Round the drift bounds to multiple of unit_drift_resolution for grid alignment
    auto search_resolution_Hz_sec = unit_drift_resolution * options.resolution;
    auto rounded_low_drift_Hz_sec = std::round(options.low_rate_Hz_per_sec / unit_drift_resolution) * unit_drift_resolution;
    auto rounded_high_drift_Hz_sec = std::round(options.high_rate_Hz_per_sec / unit_drift_resolution) * unit_drift_resolution;

    auto number_drifts = std::lround(std::abs(rounded_high_drift_Hz_sec - rounded_low_drift_Hz_sec) / std::abs(search_resolution_Hz_sec));
    
    fmt::print("INFO: Searching {} drift rates from {} Hz/sec to {} Hz/sec in increments of {} Hz/sec\n", 
               number_drifts, rounded_low_drift_Hz_sec, rounded_high_drift_Hz_sec, std::abs(search_resolution_Hz_sec));
    
    std::vector<frequency_drift_plane::drift_rate> drift_rate_info;
    drift_rate_info.reserve(number_drifts);
    
    for (int index = 0; index < number_drifts; ++index) {
        auto drift_rate = rounded_low_drift_Hz_sec + index * std::abs(search_resolution_Hz_sec);
        drift_rate_info.push_back(compute_single_drift_geometry(index, drift_rate, maximum_drift_time_span, tsamp, foff_Hz, options.desmear));
    }
    
    return drift_rate_info;
}

// ============================================================================
// INTEGRATION ROUTING & WRAPPERS
// ============================================================================

coarse_channel bliss::integrate_drifts(coarse_channel cc_data, integrate_drifts_options options) {
    auto compute_device = cc_data.device();

    // 1. Calculate the geometry of the search
    auto drifts = compute_drifts(cc_data.ntsteps(), cc_data.foff(), cc_data.tsamp(), options);

    // 2. Dispatch to the appropriate kernel (CPU vs CUDA)
    if (compute_device.device_type == kDLCPU) {
        auto integrated_dedrift = integrate_linear_rounded_bins_cpu(cc_data.data(), cc_data.mask(), drifts, options);
        cc_data.set_integrated_drift_plane(integrated_dedrift);
#if BLISS_CUDA
    } else if (compute_device.device_type == kDLCUDA) {
        auto integrated_dedrift = integrate_linear_rounded_bins_cuda(cc_data.data(), cc_data.mask(), drifts, options);
        cc_data.set_integrated_drift_plane(integrated_dedrift);
#endif
    } else {
        // Fallback or specific backend implementation
        auto integrated_dedrift = integrate_linear_rounded_bins_bland(cc_data.data(), cc_data.mask(), drifts, options);
        cc_data.set_integrated_drift_plane(integrated_dedrift);
    }

    return cc_data;
}

scan bliss::integrate_drifts(scan scan_data, integrate_drifts_options options) {
    // Pipeline pattern: Adds the transform to be executed lazily
    scan_data.add_coarse_channel_transform([options](coarse_channel cc) { return integrate_drifts(cc, options); }, "integrate_drifts");
    return scan_data;
}

observation_target bliss::integrate_drifts(observation_target target, integrate_drifts_options options) {
    for (auto &target_scan : target._scans) {
        target_scan = integrate_drifts(target_scan, options);
    }
    return target;
}

cadence bliss::integrate_drifts(cadence observation, integrate_drifts_options options) {
    for (auto &target : observation._observations) {
        target = integrate_drifts(target, options);
    }
    return observation;
}