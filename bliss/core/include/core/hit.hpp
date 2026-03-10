#pragma once

#include "flag_values.hpp"
#include "protohit.hpp"

#include <map>
#include <string>
#include <tuple>

namespace bliss {

/// @brief Represents a detected signal candidate ("Hit").
///
/// A Hit is a specific region in the Time-Frequency-Drift space that exceeds
/// a signal-to-noise ratio (SNR) threshold. It contains all physical properties
/// required to characterize the signal.
struct hit {
    // -- Physical Properties --
    
    int64_t start_freq_index;      ///< Index of the starting frequency channel.
    double  start_freq_MHz;        ///< Starting frequency in MHz at t=0.
    double  start_time_sec;        ///< Start time of the signal (MJD converted to seconds).
    double  duration_sec;          ///< Duration of the signal in seconds.
    
    int64_t rate_index;            ///< Index of the drift rate in the search plane.
    double  drift_rate_Hz_per_sec; ///< Measured drift rate in Hz/s.
    
    double  power;                 ///< Integrated power of the signal (unnormalized or normalized depending on context).
    int64_t time_span_steps;       ///< Number of time steps the signal spans. (Note: Semantically similar to duration).
    int64_t integrated_channels;   ///< Number of frequency channels integrated to form this hit.
    
    float   snr;                   ///< Signal-to-Noise Ratio (Sigma).
    
    float   bandwidth;             ///< Signal bandwidth in Hz.
    int64_t binwidth;              ///< Signal width in frequency bins.
    
    rfi     rfi_counts;            ///< RFI flags found within the hit region.
    
    int64_t coarse_channel_number=0; ///< The index of the coarse channel where this hit was detected.

  public:
    /// @brief Returns a formatted string describing the hit.
    std::string repr() const;

    /// @brief Equality operator.
    /// @details Checks if all physical properties (frequency, time, drift, power, etc.) are identical.

    /// Note: integrated_channels and coarse_channel_number are intentionally excluded 
    /// for backwards compatibility with legacy sorting behavior.

    bool operator==(const hit& other) const {
        return std::tie(start_freq_index, start_freq_MHz, start_time_sec, duration_sec, 
                        rate_index, drift_rate_Hz_per_sec, power, time_span_steps, 
                        snr, bandwidth, binwidth, rfi_counts) == 
               std::tie(other.start_freq_index, other.start_freq_MHz, other.start_time_sec, other.duration_sec, 
                        other.rate_index, other.drift_rate_Hz_per_sec, other.power, other.time_span_steps, 
                        other.snr, other.bandwidth, other.binwidth, other.rfi_counts);
    }

    /// @brief Less-than operator.
    /// @details Defines a strict ordering for hits, primarily used for sorting or 
    /// storing hits in ordered containers (e.g., std::set).

    /// Note: integrated_channels and coarse_channel_number are intentionally excluded 
    /// for backwards compatibility with legacy sorting behavior.

    /// Ordering priority: Frequency -> Time -> Duration -> Drift Rate -> Power -> SNR.
    bool operator<(const hit& other) const {
        return std::tie(start_freq_index, start_freq_MHz, start_time_sec, duration_sec, 
                        rate_index, drift_rate_Hz_per_sec, power, time_span_steps, 
                        snr, bandwidth, binwidth, rfi_counts) < 
               std::tie(other.start_freq_index, other.start_freq_MHz, other.start_time_sec, other.duration_sec, 
                        other.rate_index, other.drift_rate_Hz_per_sec, other.power, other.time_span_steps, 
                        other.snr, other.bandwidth, other.binwidth, other.rfi_counts);
    }

    /// @brief Tuple definition for serialization/deserialization state.
    using state_tuple = std::tuple<int64_t /*start_freq_index*/,
                                   float /*start_freq_MHz*/,
                                   int64_t /*rate_index*/,
                                   float /*drift_rate_Hz_per_sec*/,
                                   float /*power*/,
                                   float /*time_span_steps*/,
                                   float /*snr*/,
                                   double /*bandwidth*/,
                                   int64_t /*binwidth*/
                                   // rfi /*rfi_counts*/,
                                   >;

    /// @brief Extracts the hit's state as a tuple (useful for serialization).
    state_tuple get_state() const;

    /// @brief Restores the hit's state from a tuple.
    void set_state(state_tuple state);
};

} // namespace bliss