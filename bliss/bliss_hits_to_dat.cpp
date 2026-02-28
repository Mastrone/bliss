#include <file_types/cpnp_files.hpp>
#include <file_types/dat_file.hpp>

#include "fmt/core.h"
#include <fmt/ranges.h>
#include <cstdint>
#include <string>
#include <vector>

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

// ============================================================================
// CONFIGURATION STATE
// ============================================================================

namespace {

    /**
     * @brief Encapsulates the application's configuration parameters.
     */
    struct app_config {
        std::string input_file = "";
        std::string output_path = "";
        bool help = false;
    };

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

    /**
     * @brief Parses and validates command-line arguments.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line argument strings.
     * @param config The application configuration struct to populate.
     * @return True if parsing succeeds and execution should continue, false otherwise.
     */
    bool parse_cli_arguments(int argc, char *argv[], app_config& config) {
        auto cli = (
            (
                (clipp::option("-i", "--input") & clipp::value("input_file").set(config.input_file)) % "input files with scan hits, expected to be capnp serialized",
                (clipp::option("-o", "--output") & (clipp::value("output_file").set(config.output_path))) % "path to output file"
            )
            |
            clipp::option("-h", "--help").set(config.help) % "Show this screen."
        );

        auto parse_result = clipp::parse(argc, argv, cli);

        if (config.input_file.empty()) {
            fmt::print("Missing an input file. Use '-i' or '--input' to provide a file to convert.\n");
            config.help = true;
        }

        if (!parse_result || config.help) {
            std::cout << clipp::make_man_page(cli, "bliss_hits_to_dat");
            return false;
        }

        return true;
    }

    /**
     * @brief Resolves the output path, auto-generating it from the input filename if necessary.
     * @param config The application configuration struct containing current paths.
     * @return A finalized string path for the output file.
     */
    std::string resolve_output_path(const app_config& config) {
        if (config.output_path.empty()) {
            auto path = fs::path(config.input_file);
            return path.filename().replace_extension("dat").string();
        }
        return config.output_path;
    }

    /**
     * @brief Routes the decoded hits to either the console or a physical file.
     * @param scan_with_hits The scan object populated with decoded hit data.
     * @param output_path The destination path or standard output identifier.
     */
    void export_hits(bliss::scan& scan_with_hits, const std::string& output_path) {
        auto hits = scan_with_hits.hits();
        
        if (output_path == "-" || output_path == "stdout") {
            fmt::print("Got {} hits\n", hits.size());
            for (auto &h : hits) {
                std::cout << h.repr() << std::endl;
            }
        } else {
            bliss::write_scan_hits_to_dat_file(scan_with_hits, output_path);
        }
    }
}

// ============================================================================
// MAIN PIPELINE
// ============================================================================

/**
 * @brief Utility to convert binary hits files to text format.
 * @details Converts Cap'n Proto serialized hit files (produced by `bliss_find_hits`)
 * into the legacy .dat format compatible with TurboSETI. This facilitates
 * interoperability and manual inspection of results.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return Execution status code.
 */
int main(int argc, char *argv[]) {
    app_config config;

    // 1. Parse Arguments
    if (!parse_cli_arguments(argc, argv, config)) {
        return 0;
    }

    // 2. Resolve Paths
    std::string final_output_path = resolve_output_path(config);

    // 3. Read Binary Hits
    auto scan_with_hits = bliss::read_scan_hits_from_capnp_file(config.input_file);
    
    // 4. Export Data
    export_hits(scan_with_hits, final_output_path);

    return 0;
}