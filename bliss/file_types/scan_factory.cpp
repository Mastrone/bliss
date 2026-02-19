#include "file_types/scan_factory.hpp"

// We include the concrete implementations here (and only here) to keep
// the rest of the system decoupled from specific file format dependencies.
#include "file_types/h5_filterbank_file.hpp"

#include <fmt/format.h>
#include <stdexcept>
#include <memory>
#include <string>

using namespace bliss;

// Local helper to check file extension case-insensitively (roughly)
static bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

scan ScanFactory::create_from_file(std::string_view file_path, int num_fine_channels_per_coarse) {
    std::shared_ptr<IScanDataSource> datasource = nullptr;

    // Implicit Strategy Pattern: select reader based on extension
    if (ends_with(file_path, ".h5") || ends_with(file_path, ".hdf5") || ends_with(file_path, ".fil")) {
        
        // Instantiate the HDF5/Filterbank reader
        datasource = std::make_shared<h5_filterbank_file>(file_path);

    } 
    // Future extension point:
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

    // Create and return the scan object.
    // The scan constructor accepts the abstract IScanDataSource, so it doesn't know
    // or care what specific file format lies underneath.
    return scan(datasource, num_fine_channels_per_coarse);
}