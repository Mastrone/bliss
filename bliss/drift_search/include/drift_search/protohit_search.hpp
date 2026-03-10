#pragma once
#include "hit_search_options.hpp"

#include <core/protohit.hpp>
#include <core/coarse_channel.hpp>

#include <vector>

namespace bliss {

/**
 * @brief Prepares metadata and executes the configured hit search algorithm.
 * * @details This function acts as a coordinator:
 * 1. Calculates the expected noise power for each drift rate (accounting for desmearing).
 * 2. Selects the search algorithm (Local Maxima or Connected Components) based on options.
 * 3. Executes the search on the drift plane.
 * * @param drift_plane The processed drift plane object.
 * @param noise_estimate The global noise statistics.
 * @param options Configuration options.
 * @return A vector of raw candidate signals (protohits).
 */
std::vector<protohit>
protohit_search(bliss::frequency_drift_plane &drift_plane, noise_stats noise_estimate, hit_search_options options = {});

} // namespace bliss