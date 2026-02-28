#include <core/scan.hpp>
#include <core/cadence.hpp>
#include <estimators/noise_estimate.hpp>
#include <preprocess/excise_dc.hpp>
#include <preprocess/normalize.hpp>
#include <preprocess/passband_static_equalize.hpp>
#include <drift_search/event_search.hpp>
#include <drift_search/filter_hits.hpp>
#include <drift_search/hit_search.hpp>
#include <drift_search/integrate_drifts.hpp>
#include <flaggers/filter_rolloff.hpp>
#include <flaggers/magnitude.hpp>
#include <flaggers/sigmaclip.hpp>
#include <flaggers/spectral_kurtosis.hpp>
#include <file_types/hits_file.hpp>
#include <file_types/scan_factory.hpp>

#include "fmt/core.h"
#include <fmt/ranges.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include <limits>

#include <chrono>
#include <iostream> 
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "Filesystem library not available"
#endif

#include "clipp.h"

using namespace bliss;

// ============================================================================
// CONFIGURATION STATE
// ============================================================================

/**
 * @brief Encapsulates all application configuration and CLI parameters.
 */
struct app_config {
    std::vector<std::string> pipeline_files;
    int coarse_channel = 0;
    int number_coarse_channels = 1;
    std::string channel_taps_path;
    bool validate_pfb_response = false;
    bool excise_dc = false;
    
    integrate_drifts_options dedrift_options{
        .desmear = true, 
        .low_rate_Hz_per_sec = -5, 
        .high_rate_Hz_per_sec = 5, 
        .resolution = 1
    };
    
    int low_rate = std::numeric_limits<int>::min();
    int high_rate = std::numeric_limits<int>::max();

    std::string device = "cuda:0";
    int nchan_per_coarse = 0;
    
    hit_search_options hit_search_opts{
        .method = hit_search_methods::CONNECTED_COMPONENTS, 
        .snr_threshold = 10.0f, 
        .neighbor_l1_dist = 7
    };
    
    filter_options hit_filter_opts{
        .filter_zero_drift = true,
        .filter_sigmaclip = true, 
        .minimum_percent_sigmaclip = 0.1,
        .filter_high_sk = false, 
        .minimum_percent_high_sk = 0.1,
        .filter_low_sk = false, 
        .maximum_percent_low_sk = 0.1
    };
    
    std::string output_path = "";
    std::string output_format = "";
    
    struct {
        float filter_rolloff = 0.25;
        float sk_low = 0.25;
        float sk_high = 50;
        float sk_d = 2;
        int sigmaclip_iters = 5;
        float sigmaclip_low = 3;
        float sigmaclip_high = 4;
    } flag_opts;
    
    bool help = false;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Calculates the precise maximum drift rate evaluated during the de-Doppler search.
 * @details Computes the exact maximum drift rate that falls on the physical grid 
 * by quantizing the requested search bounds against the unit drift resolution.
 * @param sc The scan containing the observation metadata.
 * @param dedrift_options The drift search configuration.
 * @return The precise maximum drift rate in Hz/sec.
 */
double calculate_precise_max_drift(scan& sc, integrate_drifts_options& dedrift_options) {
    double tsamp = sc.tsamp();
    int time_steps = sc.ntsteps();
    double foff_Hz = std::abs(sc.foff()) * 1e6; 

    double unit_drift_resolution = foff_Hz / ((time_steps - 1) * tsamp);

    double rounded_low = std::round(dedrift_options.low_rate_Hz_per_sec / unit_drift_resolution) * unit_drift_resolution;
    double rounded_high = std::round(dedrift_options.high_rate_Hz_per_sec / unit_drift_resolution) * unit_drift_resolution;

    double search_step = unit_drift_resolution * dedrift_options.resolution;

    long number_drifts = std::lround(std::abs(rounded_high - rounded_low) / std::abs(search_step));
    double precise_max_drift = rounded_low + (number_drifts - 1) * std::abs(search_step);
    
    return std::round(precise_max_drift * 1e7) / 1e7;
}

/**
 * @brief Defines and parses the command-line interface arguments.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @param config The application configuration struct to populate.
 * @return True if parsing succeeds and execution should continue, false otherwise.
 */
bool parse_cli_arguments(int argc, char *argv[], app_config& config) {
    auto cli = (
        (
            clipp::values("files").set(config.pipeline_files) % "input hdf5 filterbank files",

            (clipp::option("-c", "--coarse-channel") & clipp::value("coarse_channel").set(config.coarse_channel)) % fmt::format("Coarse channel to process (default: {})", config.coarse_channel),
            (clipp::option("--number-coarse") & clipp::value("number_coarse_channels").set(config.number_coarse_channels)) % fmt::format("Number of coarse channels to process (default: {})", config.number_coarse_channels),
            (clipp::option("--nchan-per-coarse") & clipp::value("nchan_per_coarse").set(config.nchan_per_coarse)) % fmt::format("number of fine channels per coarse to use (default: {} auto-detects)", config.nchan_per_coarse),

            (clipp::option("-e", "--equalizer-channel") & clipp::value("channel_taps").set(config.channel_taps_path)) % "the path to coarse channel response at fine frequency resolution",
            clipp::option("--validate-pfb").set(config.validate_pfb_response, true) % fmt::format("whether to validate the coarse channel has a similar PFB response to the given response (default: {})", config.validate_pfb_response),
            (clipp::option("--excise-dc") .set(config.excise_dc, true) |
             clipp::option("--noexcise-dc").set(config.excise_dc, false)) % fmt::format("Excise DC offset from the data (default: {})", config.excise_dc),

            (clipp::option("-d", "--device") & clipp::value("device").set(config.device)) % fmt::format("Compute device to use (default: {})", config.device),

            (clipp::option("--desmear") .set(config.dedrift_options.desmear, true) |
             clipp::option("--nodesmear").set(config.dedrift_options.desmear, false)) % "Desmear the drift plane to compensate for drift rate crossing channels",
            (clipp::option("-md", "--min-drift") & clipp::value("min-rate").set(config.dedrift_options.low_rate_Hz_per_sec)) % fmt::format("Minimum drift rate (default: {})", config.dedrift_options.low_rate_Hz_per_sec),
            (clipp::option("-MD", "--max-drift") & clipp::value("max-rate").set(config.dedrift_options.high_rate_Hz_per_sec)) % fmt::format("Maximum drift rate (default: {})", config.dedrift_options.high_rate_Hz_per_sec),
            
            (clipp::option("-rs", "--rate-step") & clipp::value("rate-step").set(config.dedrift_options.resolution)) % fmt::format("Multiple of unit drift resolution to step in search (default: {})", config.dedrift_options.resolution),
            
            (clipp::option("-m", "--min-rate") & clipp::value("min-rate").set(config.low_rate)) % "(DEPRECATED: use -md) Minimum drift rate (fourier bins)",
            (clipp::option("-M", "--max-rate") & clipp::value("max-rate").set(config.high_rate)) % "(DEPRECATED: use -MD) Maximum drift rate (fourier bins)",

            (clipp::option("--sk-low") & clipp::value("spectral kurtosis lower").set(config.flag_opts.sk_low)) % "Flagging lower threshold for spectral kurtosis",
            (clipp::option("--sk-high") & clipp::value("spectral kurtsosis high").set(config.flag_opts.sk_high)) % "Flagging high threshold for spectral kurtosis",
            (clipp::option("--sk-d") & clipp::value("spectral kurtosis d").set(config.flag_opts.sk_d)) % "Flagging shape parameter for spectral kurtosis",

            (clipp::option("--local-maxima") .set(config.hit_search_opts.method, hit_search_methods::LOCAL_MAXIMA) |
             clipp::option("--connected-components").set(config.hit_search_opts.method, hit_search_methods::CONNECTED_COMPONENTS)) % "select the hit search method",
            (clipp::option("-s", "--snr") & clipp::value("snr_threshold").set(config.hit_search_opts.snr_threshold)) % fmt::format("SNR threshold (default: {})", config.hit_search_opts.snr_threshold),
            (clipp::option("--distance") & clipp::value("l1_distance").set(config.hit_search_opts.neighbor_l1_dist)) % fmt::format("L1 distance to consider hits connected (default: {})", config.hit_search_opts.neighbor_l1_dist),

            (clipp::option("--filter-zero-drift") .set(config.hit_filter_opts.filter_zero_drift, true) |
             clipp::option("--nofilter-zero-drift").set(config.hit_filter_opts.filter_zero_drift, false)) % fmt::format("Filter out hits with zero drift rate (default: {})", config.hit_filter_opts.filter_zero_drift),

            (clipp::option("--filter-sigmaclip") .set(config.hit_filter_opts.filter_sigmaclip, true) |
             clipp::option("--nofilter-sigmaclip").set(config.hit_filter_opts.filter_sigmaclip, false)) % fmt::format("Filter out hits with low sigmaclip (default: {})", config.hit_filter_opts.filter_sigmaclip),
            (clipp::option("--min-sigmaclip") & clipp::value("min_sigmaclip").set(config.hit_filter_opts.minimum_percent_sigmaclip)) % fmt::format("Minimum percent of sigmaclip to keep (default: {})", config.hit_filter_opts.minimum_percent_sigmaclip),

            (clipp::option("--filter-high-sk") .set(config.hit_filter_opts.filter_high_sk, true) |
             clipp::option("--nofilter-high-sk").set(config.hit_filter_opts.filter_high_sk, false)) % fmt::format("Filter out hits with high spectral kurtosis (default: {})", config.hit_filter_opts.filter_high_sk),
            (clipp::option("--min-high-sk") & clipp::value("min_high_sk").set(config.hit_filter_opts.minimum_percent_high_sk)) % fmt::format("Minimum percent of high spectral kurtosis to keep (default: {})", config.hit_filter_opts.minimum_percent_high_sk),

            (clipp::option("--filter-low-sk") .set(config.hit_filter_opts.filter_low_sk, true) |
             clipp::option("--nofilter-low-sk").set(config.hit_filter_opts.filter_low_sk, false)) % fmt::format("Filter out hits with low spectral kurtosis (default: {})", config.hit_filter_opts.filter_low_sk),
            (clipp::option("--max-low-sk") & clipp::value("max_low_sk").set(config.hit_filter_opts.maximum_percent_low_sk)) % fmt::format("Maximum percent of low spectral kurtosis to keep (default: {})", config.hit_filter_opts.maximum_percent_low_sk),

            (clipp::option("-o", "--output") & (clipp::value("output_file").set(config.output_path), clipp::opt_value("format").set(config.output_format))) % "Filename to store output"
        )
        |
        clipp::option("-h", "--help").set(config.help) % "Show this screen."
    );

    auto parse_result = clipp::parse(argc, argv, cli);

    if (!parse_result || config.help) {
        std::cout << clipp::make_man_page(cli, "bliss_find_hits");
        return false;
    }    
    return true;
}

/**
 * @brief Translates deprecated arguments into the modern drift resolution scale.
 * @param obs The observation target providing necessary metadata.
 * @param config The application configuration struct to update.
 */
void handle_legacy_arguments(const observation_target& obs, app_config& config) {
    auto foff = std::fabs(1e6 * obs._scans[0].foff());
    auto tsamp = obs._scans[0].tsamp();
    auto ntsteps = obs._scans[0].ntsteps();
    auto drift_resolution = foff / (tsamp * (ntsteps - 1));
    
    if (config.low_rate != std::numeric_limits<int>::min()) {
        config.dedrift_options.low_rate_Hz_per_sec = config.low_rate * drift_resolution;
        fmt::print("WARN: deprecated use of -m ...\n");
    }
    if (config.high_rate != std::numeric_limits<int>::max()) {
        config.dedrift_options.high_rate_Hz_per_sec = config.high_rate * drift_resolution;        
        fmt::print("WARN: deprecated use of -M ...\n");
    }
}

/**
 * @brief Initializes files and prepares the data slices.
 * @param config The application configuration struct.
 * @return An observation target representing the data subset.
 */
observation_target load_and_slice_data(const app_config& config) {
    std::vector<scan> scans;
    scans.reserve(config.pipeline_files.size());
    
    for (const auto &file_path : config.pipeline_files) {
        auto new_scan = ScanFactory::create_from_file(file_path, config.nchan_per_coarse);
        new_scan.set_device(config.device);
        scans.push_back(new_scan);
    }

    auto pipeline_object = observation_target(scans);
    return pipeline_object.slice_observation_channels(config.coarse_channel, config.number_coarse_channels);
}

/**
 * @brief Executes the signal normalization and RFI flagging steps.
 * @param obs The raw observation target.
 * @param config The application configuration struct.
 * @return A flagged and normalized observation target.
 */
observation_target preprocess_and_flag(observation_target obs, const app_config& config) {
    obs = normalize(obs);
    
    if (config.excise_dc) {
        obs = excise_dc(obs);
    }
    
    if (!config.channel_taps_path.empty()) {
        obs = equalize_passband_filter(obs, config.channel_taps_path, bland::ndarray::datatype::float32, config.validate_pfb_response);
    } else {
        obs = flag_filter_rolloff(obs, config.flag_opts.filter_rolloff);
    }
    
    obs = flag_spectral_kurtosis(obs, config.flag_opts.sk_low, config.flag_opts.sk_high, config.flag_opts.sk_d);
    obs = flag_sigmaclip(obs, config.flag_opts.sigmaclip_iters, config.flag_opts.sigmaclip_low, config.flag_opts.sigmaclip_high);
    
    return obs;
}

/**
 * @brief Exports the detected hits to the console or disk.
 * @param obs The observation target containing the detected hits.
 * @param config The application configuration struct.
 */
void export_results(observation_target& obs, const app_config& config) {
    for (int scan_index = 0; scan_index < obs._scans.size(); ++scan_index) {
        auto &sc = obs._scans[scan_index];

        std::string out_path = config.output_path;
        std::string out_format = config.output_format;

        if (out_path.empty()) {
            auto path = fs::path(config.pipeline_files[scan_index]);
            out_path = path.filename().replace_extension("capnp").string();
            out_format = "capnp";
        }

        if (out_path == "-" || out_path == "stdout") {
            auto hits = sc.hits();
            fmt::print("scan has {} hits\n", hits.size());
            for (auto &h : hits) {
                fmt::print("{}\n", h.repr());
            }
        } else {
            // Note: dedrift_options is strictly casted to match exact signatures
            app_config temp_config = config; 
            double final_max_drift = calculate_precise_max_drift(sc, temp_config.dedrift_options);
            write_scan_hits_to_file(sc, out_path, out_format, final_max_drift);
        }
    }
}

// ============================================================================
// MAIN PIPELINE
// ============================================================================

/**
 * @brief Orchestrates the BLISS Hit Search CLI application.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 on success, non-zero on error.
 */
int main(int argc, char *argv[]) {
    app_config config;
    
    // 0. Parse arguments
    if (!parse_cli_arguments(argc, argv, config)) {
        return 0;
    }

    // 1. Initialization & Loading
    auto pipeline_object = load_and_slice_data(config);
    handle_legacy_arguments(pipeline_object, config);

    // 2. Preprocessing & Flagging
    pipeline_object = preprocess_and_flag(pipeline_object, config);

    // 3. Noise Estimation
    pipeline_object = estimate_noise_power(
            pipeline_object,
            noise_power_estimate_options{
                .estimator_method = noise_power_estimator::STDDEV,
                .masked_estimate  = true
            }); 

    // 4. Drift Search
    pipeline_object = integrate_drifts(pipeline_object, config.dedrift_options);

    // 5. Hit Detection
    auto pipeline_object_with_hits = hit_search(pipeline_object, config.hit_search_opts);

    // 6. Hit Filtering
    pipeline_object_with_hits = filter_hits(pipeline_object_with_hits, config.hit_filter_opts);

    // 7. Output
    export_results(pipeline_object_with_hits, config);
    
    return 0;
}