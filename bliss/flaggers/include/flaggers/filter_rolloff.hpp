#pragma once

#include <core/scan.hpp>
#include <core/cadence.hpp>

namespace bliss {

/**
 * @brief Flags the edges of the frequency band (Filter Rolloff).
 *
 * @details The edges of a polyphase filterbank (PFB) often have degraded sensitivity 
 * and aliasing artifacts due to the filter response rolling off. This function
 * unconditionally flags a specified percentage of channels at both ends of the band.
 *
 * Example: 20% rolloff on a 10-channel band masks indices 0, 1 and 8, 9.
 * index:       0 1 2 3 4 5 6 7 8 9
 * input mask:  o o o o o o o o o o
 * output mask: x x o o o o o o x x
 *
 * @param cc_data The coarse channel to process.
 * @param rolloff_width The fraction of the band to flag at *each* edge (0.0 to 0.5).
 * @return A modified coarse channel with the mask updated.
 */
coarse_channel flag_filter_rolloff(coarse_channel cc_data, float rolloff_width);

/**
 * @brief Applies filter rolloff flagging to an entire scan.
 */
scan flag_filter_rolloff(scan fb_data, float rolloff_width);

/**
 * @brief Applies filter rolloff flagging to all scans in an observation target.
 */
observation_target flag_filter_rolloff(observation_target observations, float rolloff_width);

/**
 * @brief Applies filter rolloff flagging to all scans in a cadence.
 */
cadence flag_filter_rolloff(cadence observations, float rolloff_width);

} // namespace bliss
