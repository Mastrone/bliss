#pragma once

#include <core/protohit.hpp>
#include <drift_search/protohit_search.hpp>
#include <bland/ndarray.hpp>

#include <limits>
#include <vector>

namespace bliss {

/**
 * @brief Finds clusters of connected pixels in a binary mask.
 * @param threshold_mask A uint8 binary mask where 1 indicates a pixel above threshold.
 * @param neighborhood Defines the connectivity (e.g., adjacent pixels).
 * @return A vector of protohits representing the clusters.
 */
std::vector<protohit> find_components_in_binary_mask(const bland::ndarray &threshold_mask, std::vector<bland::nd_coords> neighborhood);

/**
 * @brief Performs thresholding and clustering in a single pass.
 * @details Identifies all pixels above the SNR threshold and groups connected ones into protohits.
 * This is generally more robust than local maxima for real-world signals which may be spread out.
 * * @param doppler_spectrum The drift plane data.
 * @param dedrifted_rfi RFI flags.
 * @param noise_floor Baseline noise level.
 * @param noise_per_drift Noise power per drift row (for SNR calculation).
 * @param snr_threshold Detection threshold.
 * @param neighbor_l1_dist Distance to define connectivity.
 * @return A vector of detected protohits.
 */
std::vector<protohit>
find_components_above_threshold(bland::ndarray                           doppler_spectrum,
                                integrated_flags                         dedrifted_rfi,
                                float                                    noise_floor,
                                std::vector<protohit_drift_info>         noise_per_drift,
                                float                                    snr_threshold,
                                int                                      neighbor_l1_dist);

} // namespace bliss