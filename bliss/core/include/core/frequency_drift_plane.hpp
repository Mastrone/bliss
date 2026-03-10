#pragma once

#include "integrate_drifts_options.hpp" // integrated_flags

#include <fmt/core.h>

#include <cstdint>
#include <vector>

namespace bliss {

/// @brief Helper structure containing noise information adjusted for integration.
struct protohit_drift_info {
    float integration_adjusted_noise;
};

/// @brief Represents the Time-Frequency-Drift plane resulting from the dedoppler transform.
///
/// This class encapsulates the output of the integration kernel. It contains the
/// 2D plane (Drift Rate x Frequency) where signals are searched.
/// It manages the memory residing on either CPU or GPU (`bland::ndarray`) and
/// tracks the specific drift rates that were searched.
class frequency_drift_plane {
    public:
    
    /// @brief Metadata describing a single drift trajectory searched during integration.
    struct drift_rate {
        int index_in_plane;            ///< The row index in the drift plane matrix.
        double drift_rate_slope = 0.0F; ///< The slope in terms of frequency bins per time step.
        double drift_rate_Hz_per_sec = 0.0F; ///< The physical drift rate in Hz/s.
        int drift_channels_span = 0;   ///< Total frequency channels crossed by this drift.
        int desmeared_bins=1;          ///< Number of bins per spectrum used to desmear (normalize power).

        /// @brief Returns a string representation of the drift rate metadata.
        std::string repr() const {
            return fmt::format("index_in_plane: {}  drift_rate_slope: {}  drift_rate_Hz_per_sec: {}  drift_channels_span: {}  desmeared_bins: {}\n",
                        index_in_plane, drift_rate_slope, drift_rate_Hz_per_sec, drift_channels_span, desmeared_bins);
        }
    };

    /// @brief Basic constructor.
    /// @param drift_plane The integrated power tensor.
    /// @param drift_rfi The RFI flags associated with the integration.
    frequency_drift_plane(bland::ndarray drift_plane, integrated_flags drift_rfi);

    /// @brief Full constructor with metadata.
    /// @param drift_plane The integrated power tensor.
    /// @param drift_rfi The RFI flags associated with the integration.
    /// @param integration_steps The number of time steps integrated.
    /// @param dri Vector of drift rate metadata corresponding to the rows of the plane.
    frequency_drift_plane(bland::ndarray drift_plane, integrated_flags drift_rfi, int64_t integration_steps, std::vector<drift_rate> dri);

    /// @brief Returns the number of time steps that were summed to create this plane.
    int64_t integration_steps();

    /// @brief Returns the metadata for all drift rates present in this plane.
    std::vector<drift_rate> drift_rate_info();

    /// @brief Accesses the integrated power values (Drift Plane).
    /// @details Ensures the data resides on the configured device before returning.
    bland::ndarray integrated_drift_plane();

    /// @brief Accesses the RFI flags for the drift plane.
    /// @details Ensures the flags reside on the configured device before returning.
    integrated_flags integrated_rfi();

    /// @brief Sets the target compute device (e.g., CPU or CUDA).
    void set_device(bland::ndarray::dev dev);

    /// @brief Sets the target compute device by name string.
    void set_device(std::string_view dev_str);

    /// @brief Forces immediate transfer of data and flags to the configured device.
    void push_device();

    private:
    // Slow-time steps passed through for a complete integration.
    // The total number of bins contributing to this integration is desmeared_bins * integration_steps.
    int64_t _integration_steps;

    // Info for each drift rate searched.
    // TODO: Consider changing to a map keyed by integer number of unit drifts.
    // TODO: Consider a richer class containing search resolution and max rate searched.
    std::vector<drift_rate> _drift_rate_info;

    // The actual frequency drift plane tensor (dimensions: Drifts x Frequency).
    bland::ndarray _integrated_drifts;

    // RFI flags corresponding to the dedoppler output.
    integrated_flags _dedrifted_rfi;

    bland::ndarray::dev _device = default_device;
};

} // namespace bliss