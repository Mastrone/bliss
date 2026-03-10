#pragma once

#if BLISS_USE_CAPNP

#include <core/hit.hpp>
#include <core/scan.hpp>
#include <core/cadence.hpp>

#include <string_view>
#include <vector>
#include <list>

namespace bliss {

/**
 * @brief Writes a list of hits to a binary file using Cap'n Proto serialization.
 */
template<typename Container>
void write_hits_to_capnp_file(Container hits, std::string_view file_path);

/**
 * @brief Reads a list of hits from a Cap'n Proto binary file.
 */
std::list<hit> read_hits_from_capnp_file(std::string_view file_path);

/**
 * @brief Serializes a single coarse channel (metadata + hits) to Cap'n Proto.
 */
void write_coarse_channel_hits_to_capnp_file(coarse_channel scan_with_hits, std::string_view file_path);

/**
 * @brief Deserializes a coarse channel from Cap'n Proto.
 */
coarse_channel read_coarse_channel_hits_from_capnp_file(std::string_view file_path);

/**
 * @brief Serializes an entire scan (collection of coarse channels) to Cap'n Proto.
 */
void write_scan_hits_to_capnp_file(scan scan_with_hits, std::string_view file_path);

/**
 * @brief Deserializes an entire scan.
 */
scan read_scan_hits_from_capnp_file(std::string_view file_path);

/**
 * @brief Serializes an entire observation target.
 */
void write_observation_target_hits_to_capnp_files(observation_target scan_with_hits, std::string_view file_path);

/**
 * @brief Deserializes an observation target.
 */
observation_target read_observation_target_hits_from_capnp_files(std::string_view file_path);

/**
 * @brief Serializes a full cadence.
 */
void write_cadence_hits_to_capnp_files(cadence cadence_with_hits, std::string_view file_path);

/**
 * @brief Deserializes a full cadence.
 */
cadence read_cadence_hits_from_capnp_files(std::string_view file_path);


} // namespace bliss

#endif // BLISS_USE_CAPNP