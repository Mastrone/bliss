#pragma once

#include <bland/ndarray.hpp>

#include <tuple>
#include <string>

namespace bliss {

/**
 * @brief Container for noise statistics used to normalize signals and compute SNR.
 *
 * @details This class holds estimates for:
 * - **Noise Floor (Mean):** The expected baseline value of the noise.
 * - **Noise Power (Variance):** The expected variability of the noise power.
 * * These statistics are typically computed per-channel or per-band using methods like
 * spectral kurtosis or simple statistical estimation on RFI-free samples.
 */
struct noise_stats {

    public:
    /// @brief Returns the estimated variance (power) of the noise.
    float noise_power();
    
    /// @brief Sets the noise power tensor.
    void set_noise_power(bland::ndarray noise_power);

    /// @brief Returns the noise amplitude (Square root of noise power).
    /// @details Useful for normalizing voltage data or converting power SNR to amplitude units.
    float noise_amplitude();

    /// @brief Returns the estimated mean noise floor.
    float noise_floor();
    
    /// @brief Sets the noise floor tensor.
    void set_noise_floor(bland::ndarray noise_floor);

    /// @brief Returns a string representation of the stats for debugging.
    std::string repr() const;

    // using state_tuple = std::tuple<float, float>;
    // state_tuple get_state() const;
    // void set_state(state_tuple);

    protected:
    bland::ndarray _noise_floor;
    bland::ndarray _noise_power;
};

} // namespace bliss