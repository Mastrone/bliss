#pragma once

#include "hit_search_options.hpp"

#include <core/protohit.hpp> 
#include <core/cadence.hpp>
#include <core/flag_values.hpp>
#include <core/hit.hpp>
#include <core/noise_power.hpp>
#include <core/scan.hpp>

#include <bland/stride_helper.hpp>

#include <list>

namespace bliss {

// Type alias for RFI flag map (TODO: Optimize for performance)
using rfi = std::map<flag_values, uint8_t>; 

/**
 * @brief Performs signal detection on a single dedrifted coarse channel.
 * * @details This function scans the integrated drift plane for peaks that exceed
 * the SNR threshold defined in `options`. It converts detected `protohits` (raw indices)
 * into fully characterized `hit` objects (physical units).
 * * @param dedrifted_scan A coarse channel that has already undergone drift integration.
 * @param options Configuration for thresholding and clustering.
 * @return A list of detected hits.
 */
std::list<hit> hit_search(coarse_channel dedrifted_scan, hit_search_options options = {});

/**
 * @brief Configures hit search for an entire scan.
 * @details Adds the hit search step to the processing pipeline. The returned scan
 * will contain the detected hits once processed.
 * @return A copy of the scan with the transform configured.
 */
scan hit_search(scan dedrifted_scan, hit_search_options options = {});

/**
 * @brief Runs hit search on all scans within an observation target.
 * @return A copy of the target with hits populated in all scans.
 */
observation_target hit_search(observation_target dedrifted_target, hit_search_options options = {});

/**
 * @brief Runs hit search on an entire cadence.
 * @return A copy of the cadence with hits populated.
 */
cadence hit_search(cadence dedrifted_cadence, hit_search_options options = {});

} // namespace bliss