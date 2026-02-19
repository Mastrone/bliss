#pragma once

#include <core/scan.hpp>
#include <core/cadence.hpp>

namespace bliss {


/**
 * @brief Generates a mask for elements exceeding a fixed magnitude threshold.
 * @param data The input data array (spectrogram).
 * @param threshold The value above which samples are flagged.
 * @return A uint8 mask with `flag_values::magnitude` set where `data > threshold`.
 */
bland::ndarray flag_magnitude(const bland::ndarray &data, float threshold);

/**
 * @brief Flags a coarse channel based on a hard magnitude threshold.
 * @return A modified coarse channel with updated RFI mask.
 */
coarse_channel flag_magnitude(coarse_channel fb_data, float threshold);

/**
 * @brief Flags a coarse channel using an automatically calculated threshold.
 * @details Computes `mean + 10 * stddev` of the channel data and uses that as the threshold.
 */
coarse_channel flag_magnitude(coarse_channel fb_data);

/**
 * @brief Applies magnitude flagging to an entire scan (hard threshold).
 */
scan flag_magnitude(scan fb_data, float threshold);

/**
 * @brief Applies magnitude flagging to an entire scan (auto threshold: mean + 10*stddev).
 */
scan flag_magnitude(scan fb_data);

observation_target flag_magnitude(observation_target observations, float threshold);
observation_target flag_magnitude(observation_target observations);

cadence flag_magnitude(cadence observations, float threshold);
cadence flag_magnitude(cadence observations);

} // namespace bliss