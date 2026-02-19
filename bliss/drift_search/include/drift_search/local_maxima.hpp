#pragma once

#include <core/protohit.hpp>
#include <core/frequency_drift_plane.hpp>

#include <bland/ndarray.hpp>
#include <bland/stride_helper.hpp> 

namespace bliss {

/**
 * @brief Scans the drift plane for local maxima exceeding the SNR threshold.
 * * @param doppler_spectrum The 2D integrated drift plane (Drift x Frequency).
 * @param dedrifted_rfi The RFI flags associated with the plane.
 * @param noise_floor The global noise floor baseline.
 * @param noise_per_drift Vector containing the noise standard deviation for each drift rate (row).
 * @param snr_threshold The minimum SNR required for a detection.
 * @param neighbor_l1_dist The radius to search for higher neighbors to confirm a local maximum.
 * @return A vector of detected protohits.
 */
std::vector<protohit> find_local_maxima_above_threshold(bland::ndarray   doppler_spectrum,
                                                        integrated_flags dedrifted_rfi,
                                                        float            noise_floor,
                                                        std::vector<protohit_drift_info> noise_per_drift,
                                                        float            snr_threshold,
                                                        int              neighbor_l1_dist);

} // namespace bliss