#pragma once

#include <core/hit.hpp>
#include <core/scan.hpp>
#include <core/cadence.hpp>

#include <string_view>
#include <vector>
#include <list>


namespace bliss {

/**
 * @brief Writes hits to a .dat file format compatible with TurboSETI.
 * * @details The .dat format is a tab-separated text file containing metadata header
 * lines (starting with #) followed by a table of hits. This allows interoperability
 * with existing SETI analysis tools.
 * * @param scan_with_hits The scan containing the hits to write.
 * @param file_path Destination path.
 * @param max_drift_rate The maximum drift rate searched (metadata for the header).
 */
void write_scan_hits_to_dat_file(scan scan_with_hits, std::string_view file_path, double max_drift_rate=0.0);

/**
 * @brief Reads hits from a .dat file.
 * * @details Parses the text file to reconstruct a `scan` object containing the list of hits.
 * Note that the reconstructed scan will likely not have the original spectrogram data,
 * only the metadata and the hit list.
 * * @param file_path Path to the .dat file.
 * @return A scan object populated with the read hits.
 */
scan read_scan_hits_from_dat_file(std::string_view file_path);

} // namespace bliss