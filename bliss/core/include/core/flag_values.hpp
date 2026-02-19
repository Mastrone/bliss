#pragma once

#include <cstdint>

namespace bliss {

/**
 * @brief Bitmask enumeration for Radio Frequency Interference (RFI) flags.
 * * These values are used to tag specific samples or regions in the data that have been
 * identified as interference by various mitigation algorithms.
 */
enum flag_values {
    unflagged              = 0,      ///< Clean data.
    filter_rolloff         = 1 << 0, ///< Flagged due to being on the edge of the polyphase filterbank.
    low_spectral_kurtosis  = 1 << 1, ///< Flagged by SK for being statistically abnormal (too low).
    high_spectral_kurtosis = 1 << 2, ///< Flagged by SK for being statistically abnormal (too high).
    RESERVED_0             = 1 << 3,
    magnitude              = 1 << 4, ///< Flagged due to simple magnitude threshold (too loud).
    sigma_clip             = 1 << 5, ///< Flagged as a statistical outlier (n-sigma deviation).
    RESERVED_1             = 1 << 6,
    RESERVED_2             = 1 << 7,
};
} // namespace bliss