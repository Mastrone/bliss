#pragma once

#include <core/scan.hpp>
#include <string_view>

namespace bliss {

/**
 * @brief Factory class for creating `scan` instances from file paths.
 * * @details This class implements the Factory Pattern to decouple the core system from
 * specific file format implementations (e.g., HDF5, GUPPI RAW). Ideally, the Core should
 * not need to link against heavy libraries like HDF5; it should only know about the
 * `IScanDataSource` interface. This factory handles the logic of detecting the file type
 * and instantiating the correct concrete DataSource.
 */
class ScanFactory {
public:
    /**
     * @brief Creates a scan object from a file path.
     * * @details Automatically detects the file format based on the extension (e.g., .h5, .fil)
     * and initializes the appropriate reader (e.g., `h5_filterbank_file`).
     * * @param file_path The path to the observation file.
     * @param num_fine_channels_per_coarse Optional parameter to specify fine channelization 
     * if it cannot be inferred from the file metadata (0 = auto-detect).
     * @return A `scan` object initialized with the correct data source.
     * @throws std::runtime_error if the file extension is not recognized or supported.
     */
    static scan create_from_file(std::string_view file_path, int num_fine_channels_per_coarse = 0);
};

} // namespace bliss