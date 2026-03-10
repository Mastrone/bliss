#pragma once

#include <core/coarse_channel.hpp>
#include <core/scan.hpp>
#include <core/cadence.hpp>

namespace bliss {

/**
 * @brief Computes Spectral Kurtosis (SK) flags to identify non-Gaussian signals.
 * * @details Spectral kurtosis measures the variability of the signal power over time.
 * RFI (man-made signals) often has a different statistical distribution than
 * natural Gaussian noise (e.g., radar pulses are highly non-Gaussian).
 * * The estimator used is:
 * SK = (M N d + 1)/(M-1) * (M S_2 / S_1^2 - 1)
 * * Where:
 * - d: Parameter of the gamma function describing the power spectrum (usually 2).
 * - N: Number of spectrograms averaged (time integration factor).
 * - M: Number of spectra used for the kurtosis estimate.
 * * Reference: Nita, G. M and Gary, D. E. "The Generalized Spectral Kurtosis Estimator" (2010).
 * * @param data Input spectrogram.
 * @param N Integration factor.
 * @param M Estimation window size.
 * @param d Gamma parameter (default 2).
 * @param lower_threshold Lower bound for valid SK.
 * @param upper_threshold Upper bound for valid SK.
 * @return A mask with `low_spectral_kurtosis` or `high_spectral_kurtosis` flags.
 */
bland::ndarray flag_spectral_kurtosis(const bland::ndarray &data, int64_t N, int64_t M, float d=2, float lower_threshold=0.25, float upper_threshold=50);

/**
 * @brief Applies Spectral Kurtosis flagging to a coarse channel.
 * @details Automatically calculates N and M based on channel metadata (tsamp, foff).
 */
coarse_channel flag_spectral_kurtosis(coarse_channel channel_data, float lower_threshold=0.05f, float upper_threshold=50.f, float d=2);

/**
 * @brief Applies SK flagging to an entire scan.
 * @details TODO: make this work on *all* coarse channels in a filterbank, it might be useful to
 * defer computing perhaps with a future 
 */
scan flag_spectral_kurtosis(scan fb_data, float lower_threshold=0.25f, float upper_threshold=50.f, float d=2);

observation_target flag_spectral_kurtosis(observation_target observations, float lower_threshold=0.25f, float upper_threshold=50.f, float d=2);

cadence flag_spectral_kurtosis(cadence cadence_data, float lower_threshold=0.25f, float upper_threshold=50.f, float d=2);

} // namespace bliss