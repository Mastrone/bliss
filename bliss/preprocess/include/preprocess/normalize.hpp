#pragma once

#include <bland/ndarray.hpp>
#include <core/cadence.hpp>
#include <core/coarse_channel.hpp>
#include <core/scan.hpp>

namespace bliss {

    /**
     * @brief Normalizes the coarse channel data.
     * * @details Scales the data such that the maximum value is 1.0 (or similar scaling).
     * This prepares the data for algorithms that expect a specific dynamic range.
     * * @param cc The coarse channel to normalize.
     * @return The normalized coarse channel.
     */
    coarse_channel normalize(coarse_channel cc);

    /**
     * @brief Schedules normalization for an entire scan.
     */
    scan normalize(scan sc);

    /**
     * @brief Normalizes all scans in an observation target.
     */
    observation_target normalize(observation_target ot);

    /**
     * @brief Normalizes all scans in a cadence.
     */
    cadence normalize(cadence ca);

}