#pragma once

#include "scan.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bliss {

/**
 * @brief Represents a physical target observed by the telescope.
 * * An `observation_target` aggregates one or more `scan` objects that correspond to the same 
 * celestial coordinates and source name. It is the second level of the data hierarchy,
 * sitting above `scan` and below `cadence`.
 */
struct observation_target {
  public:
    /// @brief Default constructor.
    observation_target() = default;
    
    /// @brief Constructs a target from a list of existing scans.
    /// @param filterbanks A vector of scans belonging to this target.
    observation_target(std::vector<scan> filterbanks);

    /**
     * @brief Validates that all scans within this target are compatible.
     * @details Checks that `fch1` (start frequency), `foff` (channel width), and 
     * `nchans` (channel count) are identical across all contained scans.
     * @return `true` if consistent, `false` otherwise.
     */
    bool validate_scan_consistency();

    /**
     * @brief Maps a frequency to a coarse channel index.
     * @details Delegates the lookup to the underlying scans (assuming consistency).
     * @param frequency The frequency in MHz.
     * @return The 0-based index of the coarse channel.
     * @throws std::runtime_error if scans are inconsistent.
     */
    int get_coarse_channel_with_frequency(double frequency);

    /**
     * @brief Returns the number of coarse channels per scan.
     * @return The integer count of coarse channels.
     * @throws std::runtime_error if scans are inconsistent.
     */
    int get_number_coarse_channels();

    /**
     * @brief Creates a new `observation_target` containing a subset of the frequency band.
     * @details Slices every contained scan to the specified channel range.
     * @param start_channel The starting coarse channel index.
     * @param count The number of channels to include.
     * @return A new `observation_target` instance representing the sub-band.
     */
    observation_target slice_observation_channels(int64_t start_channel=0, int64_t count=1);

    // REMOVED: device(), set_device(), and _device.
    // Memory management is delegated to the execution context or directly to `bland`.

    std::vector<scan> _scans;       ///< The list of scans associated with this target.
    std::string       _target_name; ///< The name of the source (derived from the scans).
};

/**
 * @brief Represents a full observing sequence (Cadence).
 * * A cadence is a collection of `observation_target`s arranged in time.
 * @details In single-dish SETI, a common pattern is the "ABACAD" cadence:
 * - **A:** Primary target (e.g., Exoplanet).
 * - **B, C, D:** Off-target reference sources or empty sky.
 * * Signals are considered candidate ETIs only if they appear in 'A' scans but vanish in 'B/C/D' scans.
 */
struct cadence {
  public:
    /// @brief Default constructor.
    cadence() = default;

    /**
     * @brief Constructs a cadence from a sequence of observations.
     * @param observations The ordered list of targets.
     */
    cadence(std::vector<observation_target> observations);

    // TODO: Add functionality to auto-sort a flat list of scans into a structured cadence based on metadata.

    std::vector<observation_target> _observations; ///< The sequence of observations.

    /**
     * @brief Validates that all scans across all targets in the cadence are compatible.
     * @details Ensures frequency structure (start freq, channel width, count) matches globally.
     * @return `true` if consistent, `false` otherwise.
     */
    bool validate_scan_consistency();

    /**
     * @brief Maps a frequency to a coarse channel index.
     * @param frequency The frequency in MHz.
     * @return The coarse channel index.
     * @throws std::runtime_error if the cadence is inconsistent.
     */
    int get_coarse_channel_with_frequency(double frequency);

    /**
     * @brief Returns the number of coarse channels per scan.
     * @return The integer count of coarse channels.
     * @throws std::runtime_error if the cadence is inconsistent.
     */
    int get_number_coarse_channels();

    /**
     * @brief Creates a new `cadence` containing a subset of the frequency band.
     * @details Slices every observation target (and thus every scan) to the specified range.
     * @param start_channel The starting coarse channel index.
     * @param count The number of channels to include.
     * @return A new `cadence` instance representing the sub-band.
     */
    cadence slice_cadence_channels(int64_t start_channel=0, int64_t count=1);

    // REMOVED: device(), set_device(), _device.
    // REMOVED: Hardware config prop drilling.
};
} // namespace bliss