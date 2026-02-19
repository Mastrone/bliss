#include "file_types/scan_factory.hpp"

// We include the concrete reader implementations here (and only here).
// This isolates the rest of the system from knowing about specific file formats.
#include "file_types/h5_filterbank_file.hpp"

#include <fmt/format.h>
#include <stdexcept>
#include <memory>
#include <string>

using namespace bliss;

// Helper to check file extensions case-sensitively (consider making case-insensitive for robustness)
static bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

scan ScanFactory::create_from_file(std::string_view file_path, int num_fine_channels_per_coarse) {
    std::shared_ptr<IScanDataSource> datasource = nullptr;

    // Strategy Selection: Choose the correct reader based on file extension.
    if (ends_with(file_path, ".h5") || ends_with(file_path, ".hdf5") || ends_with(file_path, ".fil")) {
        
        // Instantiate the HDF5/Filterbank reader
        datasource = std::make_shared<h5_filterbank_file>(file_path);

    } 
    // Future extension point for other formats:
    // else if (ends_with(file_path, ".raw")) {
    //      datasource = std::make_shared<raw_file>(file_path);
    // }
    else {
        // Fallback: If unknown, default to HDF5 but warn.
        fmt::print("WARN: ScanFactory: Unknown file extension for '{}'. Defaulting to HDF5/Filterbank reader.\n", file_path);
        datasource = std::make_shared<h5_filterbank_file>(file_path);
    }

    if (!datasource) {
        throw std::runtime_error("ScanFactory: Failed to create data source.");
    }

    // Create and return the Scan object.
    // The scan constructor receives the abstract IScanDataSource, adhering to dependency inversion.
    return scan(datasource, num_fine_channels_per_coarse);
}