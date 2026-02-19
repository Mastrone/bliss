#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <pybland.hpp>

#include "core/pyblisscore.hpp"
#include "file_types/pyfile_types.hpp"
#include "flaggers/pyflaggers.hpp"
#include "estimators/pyestimators.hpp"
#include "drift_search/pydrift_search.hpp"
#include "preprocess/pypreprocess.hpp"

// Necessary to link against the concrete ScanFactory implementation
#include <file_types/scan_factory.hpp>

#if BLISS_CUDA
#include <cuda_runtime.h>
#endif

#include <fmt/format.h>

namespace nb = nanobind;
using namespace nb::literals;

// ============================================================================
// FACTORY SHIM IMPLEMENTATIONS
// ============================================================================
// These functions were declared in core/pyblisscore.hpp (or similar) to allow
// the Core module to expose creation functions without depending directly on
// the I/O implementation libraries. We implement them here where all parts meet.

namespace bliss {

    /**
     * @brief Creates a single scan from a file path (Python shim).
     */
    scan create_scan_from_file_shim(std::string_view file_path, int n_fine) {
        return ScanFactory::create_from_file(file_path, n_fine);
    }

    /**
     * @brief Creates an Observation Target from a list of files (Python shim).
     * @param files List of file paths belonging to the same target.
     * @param n_fine Fine channels per coarse channel.
     */
    observation_target create_obs_target_from_files_shim(std::vector<std::string> files, int n_fine) {
        std::vector<bliss::scan> scans;
        scans.reserve(files.size());
        for (const auto &f : files) {
            scans.push_back(ScanFactory::create_from_file(f, n_fine));
        }
        return observation_target(scans);
    }

    /**
     * @brief Creates a Cadence from a nested list of files (Python shim).
     * @param files List of Lists: [ [target1_scan1, target1_scan2], [target2_scan1], ... ]
     */
    cadence create_cadence_from_files_shim(std::vector<std::vector<std::string>> files, int n_fine) {
        std::vector<observation_target> obs_targets;
        obs_targets.reserve(files.size());
        for (const auto &obs_files : files) {
            std::vector<bliss::scan> scans;
            scans.reserve(obs_files.size());
            for (const auto &f : obs_files) {
                scans.push_back(ScanFactory::create_from_file(f, n_fine));
            }
            obs_targets.emplace_back(scans);
        }
        return cadence(obs_targets);
    }

}

// ============================================================================
// MAIN MODULE DEFINITION
// ============================================================================

NB_MODULE(pybliss, m) {

    // --- Build Info / Versioning ---
    int cuda_runtime_version = 0;
#if BLISS_CUDA
    cudaRuntimeGetVersion(&cuda_runtime_version);
#endif
    std::string cuda_version_string = "none";
    if (cuda_runtime_version != 0) {
        int cuda_major = cuda_runtime_version/1000;
        int cuda_minor = (cuda_runtime_version - 1000 * cuda_major)/10;
        cuda_version_string = fmt::format("{}.{}", cuda_major, cuda_minor);
    }
    m.attr("_cuda_version") = cuda_version_string;

    // --- Core Bindings ---
    // Binds Scan, Hit, CoarseChannel, and the create_* shims defined above
    bind_pycore(m);

    // --- Submodules ---

    // BLAND (NDArray library)
    auto bland_module = m.def_submodule("bland", "Breakthrough Listen Arrays with N Dimensions. A multi-device ndarray library based on dlpack.");
    bind_pybland(bland_module);

    // Drift Search (Dedoppler, Event Search)
    auto drift_search_module = m.def_submodule("drift_search", "integrate & search for doppler drifting signals");
    bind_pydrift_search(drift_search_module);

    // I/O (File Types)
    auto fileio_module = m.def_submodule("io", "File I/O types");
    bind_pyfile_types(fileio_module);

    // Estimators (Noise, Kurtosis math)
    auto estimators_module = m.def_submodule("estimators", "Estimators");
    bind_pyestimators(estimators_module);

    // Flaggers (RFI filtering)
    auto flaggers_module = m.def_submodule("flaggers", "Flaggers");
    bind_pyflaggers(flaggers_module);

    // Preprocess (Normalization, DC excision)
    auto preprocess_module = m.def_submodule("preprocess", "Preprocess");
    bind_pypreprocess(preprocess_module);
    
}