#pragma once

#include "flag_values.hpp"

#include <bland/ndarray.hpp>

#include <cstdint>

namespace bliss {

/**
 * @brief Configuration options for the drift integration (Dedoppler) algorithm.
 */
struct integrate_drifts_options {
    /// @brief If true, applies desmearing correction to account for signal power spreading across bins due to drift.
    bool desmear = true;

    /// @brief The minimum drift rate to search (in Hz/sec).
    float low_rate_Hz_per_sec = -5; 

    /// @brief The maximum drift rate to search (in Hz/sec).
    float high_rate_Hz_per_sec = 5; 

    /// @brief The search step size in terms of drift resolution units.
    /// @details 1 unit corresponds to a drift of `foff` over the total scan duration.
    int resolution = 1;
};


/**
 * @brief Container for tracking RFI contamination in the integrated drift plane.
 * * @details Unlike the initial RFI mask which is Time x Frequency, this structure
 * tracks how many flagged samples were encountered along each integration path
 * in the Drift x Frequency plane.
 */
struct integrated_flags {
    bland::ndarray low_spectral_kurtosis;   ///< Count of SK flagged samples per drift path.
    bland::ndarray high_spectral_kurtosis;  ///< Count of High-SK flagged samples per drift path.
    bland::ndarray sigma_clip;              ///< Count of Sigma-Clip flagged samples per drift path.
    bland::ndarray::dev _device = default_device;

    /// @brief Constructs the flag container with zeroed buffers.
    /// @param drifts Number of drift rates searched (rows).
    /// @param channels Number of frequency channels (columns).
    /// @param device The compute device where these flags will reside.
    integrated_flags(int64_t drifts, int64_t channels, bland::ndarray::dev device = default_device) :
            low_spectral_kurtosis(bland::ndarray({drifts, channels}, 0, bland::ndarray::datatype::uint8, device)),
            high_spectral_kurtosis(bland::ndarray({drifts, channels}, 0, bland::ndarray::datatype::uint8, device)),
            sigma_clip(bland::ndarray({drifts, channels}, 0, bland::ndarray::datatype::uint8, device))
            {}

    /// @brief Sets the target device for these flags.
    void set_device(bland::ndarray::dev device) {
        _device = device;
    }

    /// @brief Sets the target device by name string.
    void set_device(std::string_view device) {
        bland::ndarray::dev dev = device;
        set_device(dev);
    }

    /// @brief Forces immediate transfer of flag buffers to the configured device.
    void push_device() {
        low_spectral_kurtosis = low_spectral_kurtosis.to(_device);
        high_spectral_kurtosis = high_spectral_kurtosis.to(_device);
        sigma_clip = sigma_clip.to(_device);
    }

};

} // namespace bliss