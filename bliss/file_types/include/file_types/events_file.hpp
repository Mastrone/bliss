#pragma once

#if BLISS_USE_CAPNP


#include <string_view>
#include <vector>

namespace bliss {

struct event;

/**
 * @brief Serializes a list of detected events to a Cap'n Proto binary file.
 * @param events The vector of events to write.
 * @param base_filename The destination file path.
 */
void write_events_to_file(std::vector<event> events, std::string_view base_filename);

/**
 * @brief Deserializes a list of events from a Cap'n Proto binary file.
 * @param file_path The source file path.
 * @return A vector of reconstructed event objects.
 */
std::vector<event> read_events_from_file(std::string_view file_path);


}

#endif // BLISS_USE_CAPNP