#include <core/scan.hpp>
#include <core/cadence.hpp>
#include <estimators/noise_estimate.hpp>
#include <preprocess/excise_dc.hpp>
#include <preprocess/normalize.hpp>
#include <drift_search/event_search.hpp>
#include <drift_search/hit_search.hpp>
#include <drift_search/integrate_drifts.hpp>
#include <flaggers/filter_rolloff.hpp>
#include <flaggers/spectral_kurtosis.hpp>
#include <file_types/scan_factory.hpp>
#include <drift_search/filter_hits.hpp>

#include <fmt/core.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// Standard filesystem inclusion (handles different compiler standards)
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "Filesystem library not available"
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
namespace {

    /**
     * @brief Applies standard Radio Frequency Interference (RFI) cleaning to a scan.
     * @param sc The raw scan data.
     * @return The normalized and flagged scan.
     */
    bliss::scan apply_rfi_cleaning(bliss::scan sc) {
        sc = bliss::normalize(sc);
        sc = bliss::flag_filter_rolloff(sc, 0.2f);
        return bliss::flag_spectral_kurtosis(sc, 0.1f, 50.0f, 2.0f);
    }

    /**
     * @brief Configures and executes the De-Doppler drift integration process.
     * @param sc The scan to process.
     * @return The scan with integrated drift planes.
     */
    bliss::scan apply_drift_integration(bliss::scan sc) {
        bliss::noise_power_estimate_options noise_opt;
        noise_opt.estimator_method = bliss::noise_power_estimator::STDDEV;
        noise_opt.masked_estimate = true;
        sc = bliss::estimate_noise_power(sc, noise_opt);

        bliss::integrate_drifts_options dedrift_opt;
        dedrift_opt.desmear = false;
        dedrift_opt.low_rate_Hz_per_sec = -2.0;
        dedrift_opt.high_rate_Hz_per_sec = 2.0;
        return bliss::integrate_drifts(sc, dedrift_opt);
    }

    /**
     * @brief Executes the hit search algorithm and applies standard hit filtering.
     * @param sc The scan containing the drift planes.
     * @return The scan populated with valid hits.
     */
    bliss::scan detect_and_filter_hits(bliss::scan sc) {
        bliss::hit_search_options hit_opt;
        hit_opt.snr_threshold = 20.0f;
        sc = bliss::hit_search(sc, hit_opt);

        bliss::filter_options filter_opt;
        filter_opt.filter_zero_drift = true;
        filter_opt.filter_sigmaclip = false; 
        filter_opt.filter_high_sk = false;
        filter_opt.filter_low_sk = false;
        
        return bliss::filter_hits(sc, filter_opt);
    }

    /**
     * @brief Scans a directory and returns a sorted list of file paths matching the extension.
     * @param directory_path The system path to the target directory.
     * @param extension The file extension to filter by.
     * @return A sorted vector of file path strings.
     * @throws std::runtime_error If the directory does not exist or cannot be accessed.
     */
    std::vector<std::string> get_files_from_directory(const std::string& directory_path, const std::string& extension = ".h5") {
        std::vector<std::string> files;
        
        if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
            throw std::runtime_error(fmt::format("Invalid directory path: {}", directory_path));
        }

        for (const auto& entry : fs::directory_iterator(directory_path)) {
            if (entry.is_regular_file() && entry.path().extension() == extension) {
                files.push_back(entry.path().string());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    /**
     * @brief Formats and outputs the details of detected candidate events.
     * @param events The vector of detected events.
     */
    void print_event_report(const std::vector<bliss::event>& events) {
        fmt::print("\n==================================================\n");
        fmt::print("[+] SEARCH COMPLETED! Found {} CANDIDATE EVENTS.\n", events.size());
        fmt::print("==================================================\n");

        for (size_t i = 0; i < events.size(); ++i) {
            const auto& ev = events[i];
            fmt::print("Event {}:\n", i + 1);
            fmt::print("  - Starting Freq. : {:.6f} MHz\n", ev.starting_frequency_Hz / 1e6);
            fmt::print("  - Drift Rate     : {:.4f} Hz/s\n", ev.average_drift_rate_Hz_per_sec);
            fmt::print("  - Average SNR    : {:.2f}\n", ev.average_snr);
            fmt::print("  - Hit Count      : {} (Survived OFF scans!)\n", ev.hits.size());
            fmt::print("--------------------------------------------------\n");
        }
    }
}

// ============================================================================
// PIPELINE EXECUTION
// ============================================================================

/**
 * @brief Highly optimized function that processes one file at a time through the entire pipeline.
 * @param file_path The system path to the input data file.
 * @return A scan object containing the detected hits.
 */
bliss::scan process_scan_and_get_hits(const std::string& file_path) {
    fmt::print("\n---> Processing on GPU: {}\n", file_path);
    
    auto sc = bliss::ScanFactory::create_from_file(file_path, 0);
    sc.set_device("cuda:0");

    sc = apply_rfi_cleaning(std::move(sc));
    sc = apply_drift_integration(std::move(sc));
    sc = detect_and_filter_hits(std::move(sc));

    fmt::print("     [OK] Found {} hits!\n", sc.hits().size());
    return sc;
}

/**
 * @brief Validates command line arguments and populates file lists.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @param on_files Reference to the vector storing ON target paths.
 * @param off_files Reference to the vector storing OFF target paths.
 * @return True if parsing and loading was successful, false otherwise.
 */
bool parse_and_validate_directories(int argc, char* argv[], std::vector<std::string>& on_files, std::vector<std::string>& off_files) {
    if (argc != 3) {
        fmt::print("Usage: {} <path_to_ON_directory> <path_to_OFF_directory>\n", argv[0]);
        fmt::print("Example: {} /data/target_A/ON /data/target_A/OFF\n", argv[0]);
        return false;
    }

    try {
        on_files = get_files_from_directory(argv[1]);
        off_files = get_files_from_directory(argv[2]);
    } catch (const std::exception& e) {
        fmt::print("ERROR: {}\n", e.what());
        return false;
    }

    if (on_files.empty() || off_files.empty()) {
        fmt::print("ERROR: Could not find .h5 files in one or both directories.\n");
        return false;
    }

    return true;
}

/**
 * @brief Constructs the full observation cadence from the provided file paths.
 * @param on_files Vector of ON target paths.
 * @param off_files Vector of OFF target paths.
 * @return A fully populated cadence object ready for event searching.
 */
bliss::cadence build_observation_cadence(const std::vector<std::string>& on_files, const std::vector<std::string>& off_files) {
    std::vector<bliss::observation_target> targets;

    // Process ON scans
    std::vector<bliss::scan> on_scans;
    for (const auto& f : on_files) {
        on_scans.push_back(process_scan_and_get_hits(f));
    }
    targets.push_back(bliss::observation_target(on_scans));

    // Process OFF scans (each goes into its own separate block)
    for (const auto& f : off_files) {
        std::vector<bliss::scan> single_off;
        single_off.push_back(process_scan_and_get_hits(f));
        targets.push_back(bliss::observation_target(single_off));
    }

    return bliss::cadence(targets);
}

/**
 * @brief Main orchestrator for the ON/OFF event search application.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return Execution status code.
 */
int main(int argc, char* argv[]) {
    std::vector<std::string> on_files;
    std::vector<std::string> off_files;

    if (!parse_and_validate_directories(argc, argv, on_files, off_files)) {
        return 1;
    }

    fmt::print("[*] Found {} ON files and {} OFF files. Starting SETI Pipeline...\n", on_files.size(), off_files.size());

    bliss::cadence my_cadence = build_observation_cadence(on_files, off_files);

    fmt::print("\n[*] GPU computations finished. Creating Cadence...\n");
    fmt::print("[*] Starting Event Search (ON-OFF Cross-Correlation)...\n");
    
    auto events = bliss::event_search(my_cadence);

    print_event_report(events);

    return 0;
}