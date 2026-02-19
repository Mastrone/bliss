#pragma once

#include <bland/ndarray.hpp>
#include <core/cadence.hpp>
#include <core/coarse_channel.hpp>
#include <core/scan.hpp>

namespace bliss {

    /**
     * @brief Removes the DC (Direct Current) spike from the center of the coarse channel.
     * * @details The DC bin (center frequency) often contains a strong artifact due to LO leakage
     * or FPGA arithmetic in the upstream channelizer. This function replaces the central bin
     * with the average of its neighbors to prevent it from triggering false positives.
     * * @param cc The coarse channel to process.
     * @return A modified coarse channel with the DC spike excised.
     */
    coarse_channel excise_dc(coarse_channel cc);

    /**
     * @brief Schedules DC excision for all channels in a scan.
     * @param sc The scan to process.
     * @return A scan object with the transform added to its pipeline.
     */
    scan excise_dc(scan sc);

    /**
     * @brief Applies DC excision to an entire observation target.
     */
    observation_target excise_dc(observation_target ot);

    /**
     * @brief Applies DC excision to an entire cadence.
     */
    cadence excise_dc(cadence ca);

}