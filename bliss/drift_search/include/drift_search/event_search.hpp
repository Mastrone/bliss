#pragma once

#include <core/cadence.hpp>
#include <core/hit.hpp>
#include <core/event.hpp>

#include <list>

namespace bliss {

/**
 * @brief Identifies astrophysical events by correlating hits across multiple scans in a cadence.
 *
 * @details This function implements the core SETI logic of "persistence checks".
 * It looks for signals that:
 * 1. Appear in the ON target.
 * 2. Follow a consistent drift rate over time (linear trajectory in Time-Frequency).
 * 3. (Implicitly via filtering logic) Do not appear in OFF targets.
 *
 * @param cadence_with_hits A cadence object where hits have already been detected in each scan.
 * @return A vector of detected `event` objects.
 *
 * // TODO: there's a better way to structure this so we can make more sense of time / on/off by reusing the
 * // cadence/observation_target structure/hierarchy
 */
std::vector<event> event_search(cadence cadence_with_hits);

} // namespace bliss