#pragma once

#include <bland/ndarray.hpp>
#include <core/cadence.hpp>
#include <core/scan.hpp>

namespace bliss {

/**
 * @brief Integrates energy along linear trajectories in the time-frequency plane.
 *
 * @details This is the core **De-Doppler** algorithm. It transforms the input Time-Frequency
 * data into the Drift Rate-Frequency plane.
 * * **Concept:**
 * A narrowband signal from an accelerating source (e.g., a transmitter on a rotating planet)
 * will "drift" in frequency over time. To detect it, we must sum the power of pixels along
 * all possible drift paths. If the path matches the signal's drift, the integrated energy
 * will be significantly higher than the noise floor (constructive interference).
 *
 * **Implementation (Linear Round Method):**
 * The integration follows a discrete line where the frequency column index `col` for a
 * given time step `t` is:
 * \f[ \text{col}(t) = \text{round}(\text{drift\_slope} \times t) \f]
 * This effectively "stacks" the pixels corresponding to a specific drift rate.
 *
 * **Desmearing:**
 * Optionally, the algorithm can account for energy spread across adjacent frequency bins
 * due to high drift rates (desmearing), improving sensitivity for fast-drifting signals.
 */

/**
 * @brief Runs the drift integration (De-Doppler) on a single coarse channel.
 * @details This function triggers the heavy computation (often on GPU). It populates 
 * the `integrated_drift_plane` member of the channel with the result.
 * @param cc_data The channel to process.
 * @param options Configuration for the integration (drift range, resolution, desmearing).
 * @return A copy of the coarse channel containing the cached integration results.
 */
[[nodiscard]] coarse_channel
integrate_drifts(coarse_channel cc_data, integrate_drifts_options options = integrate_drifts_options{.desmear = true});

/**
 * @brief Schedules drift integration for an entire scan.
 * @details Adds the integration step to the scan's lazy processing pipeline.
 * The actual computation happens when individual channels are read/accessed.
 * @return A new scan object with the transform registered.
 */
[[nodiscard]] scan integrate_drifts(scan scan_data,
                                    integrate_drifts_options options = integrate_drifts_options{.desmear = true});

/**
 * @brief Runs drift integration on all scans within an observation target.
 * @details Applies the transform to every scan in the target group.
 * @return A new observation_target with processed scans.
 */
[[nodiscard]] observation_target integrate_drifts(observation_target target,
                                                  integrate_drifts_options options = integrate_drifts_options{
                                                          .desmear = true});

/**
 * @brief Runs drift integration on an entire cadence (multiple targets).
 * @return A new cadence with processed observations.
 */
[[nodiscard]] cadence integrate_drifts(cadence observations,
                                     integrate_drifts_options options = integrate_drifts_options{.desmear = true});

} // namespace bliss