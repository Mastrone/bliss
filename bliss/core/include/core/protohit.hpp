#pragma once

#include "flag_values.hpp"

#include <map>
#include <vector>

namespace bliss {

// Type alias for counting RFI flags.
// TODO: Consider replacing std::map with a fixed-size array or struct for performance.
using rfi = std::map<flag_values, uint8_t>; 

/// @brief Coordinates representing a specific point in the Drift Plane.
struct freq_drift_coord {
    int64_t drift_index=0;      ///< Row index (Drift Rate).
    int64_t frequency_channel=0;///< Column index (Frequency Bin).
};

/// @brief A candidate signal detected in the drift plane (Host/CPU version).
///
/// A `protohit` is an intermediate representation of a signal. It captures the
/// peak location, signal strength (SNR), and the set of connected pixels (the "blob")
/// that make up the signal, before full physical parameters (like Hz/s) are calculated.
struct protohit {
    freq_drift_coord index_max;    ///< Coordinates of the highest intensity pixel.
    freq_drift_coord index_center; ///< Centroid coordinates of the signal cluster.
    float snr;                     ///< Signal-to-Noise Ratio of the peak.
    float max_integration;         ///< The raw integrated power value at the peak.
    float desmeared_noise;         ///< The noise level used for normalization (adjusted for drift).
    int binwidth;                  ///< The spectral width of the signal in bins.
    
    std::vector<freq_drift_coord> locations; ///< List of all pixels belonging to this signal cluster.
    rfi rfi_counts;                ///< Tally of RFI flags encountered within the signal region.
};


/// @brief A candidate signal detected in the drift plane (Device/CUDA version).
///
/// This structure mirrors `protohit` but is designed for use within CUDA kernels.
/// It avoids dynamic containers like `std::vector` and `std::map`.
struct device_protohit {
    freq_drift_coord index_max;
    freq_drift_coord index_center;
    float snr;
    float max_integration;
    float desmeared_noise;
    
    // Note: 'locations' is omitted here as keeping a dynamic list per thread is expensive/complex.
    
    int binwidth;
    
    // RFI counts are flattened to explicit fields for struct-of-arrays compatibility or register usage.
    uint8_t low_sk_count;
    uint8_t high_sk_count;
    uint8_t sigma_clip_count;
    
    // Clustering / De-duplication status:
    // -1: Invalid/Empty slot.
    //  0: Valid protohit.
    // >0: Index of a "better" protohit that merges/invalidates this one.
    int invalidated_by=-1;
};

} // namespace bliss