#pragma once

#include "hit.hpp"

#include <list>
#include <string>

namespace bliss {

/**
 * @brief Represents a candidate SETI event.
 * * An event is an aggregation of one or more `hit` objects that are believed to originate 
 * from the same source. This structure summarizes the aggregate properties of the signal 
 * (average drift, power, duration) across the hits that compose it.
 */
struct event {
    std::list<hit> hits; ///< The collection of hits contributing to this event.

    // --- Aggregate Properties ---
    double  starting_frequency_Hz           = 0;
    float   average_power                   = 0;
    float   average_bandwidth               = 0;
    float   average_snr                     = 0;
    double  average_drift_rate_Hz_per_sec   = 0;
    
    double  event_start_seconds             = 0; ///< Start time of the earliest hit.
    double  event_end_seconds               = 0; ///< End time of the latest hit.

    /// @brief Returns a detailed string representation of the event and its constituent hits.
    std::string repr();
};

} // namespace bliss