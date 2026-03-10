#pragma once

#include <core/coarse_channel.hpp>
#include <core/scan.hpp>
#include <core/cadence.hpp>

namespace bliss {

/**
 * @brief Computes a mask for statistical outliers using iterative Sigma Clipping.
 * * @details This algorithm calculates mean and stddev, flags outliers, and then
 * *re-calculates* mean and stddev excluding those outliers. This repeats until convergence
 * or max_iter. It is robust against strong RFI skewing the statistics.
 * * @param data Input spectrogram.
 * @param max_iter Maximum number of iterations.
 * @param low Lower threshold in sigmas (mean - low*std).
 * @param high Upper threshold in sigmas (mean + high*std).
 * @return A mask with `flag_values::sigma_clip` set.
 */
bland::ndarray flag_sigmaclip(const bland::ndarray &data, int max_iter=5, float low=3.0f, float high=4.0f);

// Overloads for CoarseChannel, Scan, ObservationTarget, Cadence
coarse_channel flag_sigmaclip(coarse_channel fb_data, int max_iter=5, float low=3.0f, float high=4.0f);
scan flag_sigmaclip(scan fb_data, int max_iter=5, float low=3.0f, float high=4.0f);
observation_target flag_sigmaclip(observation_target target, int max_iter=5, float low=3.0f, float high=4.0f);
cadence flag_sigmaclip(cadence cadence_data, int max_iter=5, float low=3.0f, float high=4.0f);

}