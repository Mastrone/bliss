#pragma once

#include <bland/ndarray.hpp>
#include <core/cadence.hpp>
#include <core/scan.hpp>
#include <core/noise_power.hpp>

namespace bliss {

/**
 * @brief Method selector for noise estimation.
 */
enum class noise_power_estimator {
    STDDEV,                   ///< Standard Mean and Variance (sensitive to outliers).
    MEAN_ABSOLUTE_DEVIATION,  ///< Median Absolute Deviation (MAD), robust against RFI outliers.
};

/**
 * @brief Configuration options for noise estimation.
 */
struct noise_power_estimate_options {
    noise_power_estimator estimator_method;
    bool                  masked_estimate = true; ///< If true, excludes flagged samples from the estimate.
};

/**
 * @brief Estimates noise statistics (floor and power) from a raw array.
 * @param x Input data.
 * @param options Estimation method (STDDEV or MAD).
 * @return A `noise_stats` object containing the computed floor and power.
 */
[[nodiscard]] noise_stats estimate_noise_power(bland::ndarray x, noise_power_estimate_options options);

/**
 * @brief Estimates noise statistics for a coarse channel.
 * @details If `options.masked_estimate` is true, this uses the channel's mask to ignore RFI.
 */
[[nodiscard]] noise_stats estimate_noise_power(coarse_channel cc_data,
                                               noise_power_estimate_options    options);

/**
 * @brief Estimates noise statistics for an entire scan.
 * @details Adds the estimation step to the scan's transform pipeline.
 */
[[nodiscard]] scan estimate_noise_power(scan fil_data, noise_power_estimate_options options);

/**
 * @brief Estimates noise statistics for all scans in an observation target.
 */
[[nodiscard]] observation_target estimate_noise_power(observation_target           observations,
                                                      noise_power_estimate_options options);

/**
 * @brief Estimates noise statistics for all observations in a cadence.
 */
[[nodiscard]] cadence estimate_noise_power(cadence observations, noise_power_estimate_options options);

} // namespace bliss