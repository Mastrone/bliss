#pragma once

#include <bland/ndarray.hpp>
#include <core/scan.hpp>

namespace bliss {

/**
 * @brief Computes the Spectral Kurtosis (SK) estimator for a spectrum.
 *
 * @details Spectral kurtosis is a statistical measure used to detect Radio Frequency Interference (RFI).
 * Natural Gaussian noise has a specific kurtosis value (related to `d` and `N`). Man-made signals
 * often deviate significantly from this value.
 *
 * The estimator used is the "Generalized Spectral Kurtosis Estimator" (Nita & Gary, 2010):
 * \f[ SK = \frac{M N d + 1}{M-1} \left( \frac{M S_2}{S_1^2} - 1 \right) \f]
 *
 * @param spectrum_grid Input 2D spectrogram (Time x Frequency).
 * @param N Number of raw samples averaged per spectral bin (Integration factor).
 * @param M Number of time samples used for the estimate (Window size).
 * @param d Gamma distribution parameter (typically 2.0 for power spectra).
 * @return An array containing the SK value for each frequency channel.
 */
[[nodiscard]] bland::ndarray estimate_spectral_kurtosis(const bland::ndarray &spectrum_grid, int32_t N, int32_t M, float d = 2.0);

/**
 * @brief Helper to estimate SK for a coarse channel using its metadata.
 */
[[nodiscard]] bland::ndarray estimate_spectral_kurtosis(coarse_channel &cc_data, float d = 2.0);


} // namespace bliss