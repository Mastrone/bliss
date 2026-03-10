#pragma once

#include <bland/stride_helper.hpp>

#include <vector>

namespace bliss {

/// @brief Algorithm selection for hit detection.
enum class hit_search_methods {
    CONNECTED_COMPONENTS, ///< Groups adjacent pixels above threshold into a single hit (better for spread signals).
    LOCAL_MAXIMA          ///< Finds isolated peaks (faster, good for sharp signals).
};

/// @brief Configuration parameters for the hit search algorithm.
struct hit_search_options {
    /// @brief The algorithm to use for detecting hits.
    hit_search_methods method = hit_search_methods::CONNECTED_COMPONENTS;

    /// @brief The SNR threshold (Linear).
    /// @details A peak must have `(power - noise_floor) / noise_stddev > snr_threshold` to be detected.
    float snr_threshold = 10.0f;

    /// @brief The L1 distance (Manhattan distance) defining "neighborhood".
    /// @details Used to determine if two pixels are connected or if a local maximum is dominant.
    int neighbor_l1_dist = 7;

    /// @brief If true, runs the connected components graph algorithm detached (potentially faster).
    bool detach_graph = true;
};

} // namespace bliss