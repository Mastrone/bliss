#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace bliss {

/// @brief Data Transfer Object (DTO) encapsulating all metadata for a scan or channel.
///
/// This structure consolidates the observational parameters (frequency, time, source)
/// previously scattered across multiple variables. It serves as the single source
/// of truth for metadata within both `scan` and `coarse_channel` objects.
struct scan_metadata {
    // --- Mandatory Fields (Core Physics) ---
    
    double      fch1;           ///< Center frequency of the first channel (MHz).
    double      foff;           ///< Frequency resolution / channel bandwidth (MHz).
    double      tsamp;          ///< Time resolution / sampling time (seconds).
    double      tstart;         ///< Start time of the observation (MJD).
    std::string source_name;    ///< Name of the observed source (e.g., "Kepler-1625").

    // --- Optional Fields (Telescope/Hardware Context) ---
    
    std::optional<int64_t> machine_id;      ///< ID of the backend instrument/machine.
    std::optional<int64_t> telescope_id;    ///< ID of the telescope facility.
    std::optional<double>  src_dej;         ///< Source Declination (J2000).
    std::optional<double>  src_raj;         ///< Source Right Ascension (J2000).
    std::optional<double>  az_start;        ///< Telescope Azimuth at start.
    std::optional<double>  za_start;        ///< Telescope Zenith Angle at start.
    
    // --- Technical Details ---
    
    int64_t                data_type = 1;   ///< Data type ID (Default: 1 = float32 filterbank).
    std::optional<int64_t> nbits;           ///< Number of bits per sample.
    int64_t                nchans = 0;      ///< Total number of frequency channels.
    int64_t                nifs = 0;        ///< Number of IF streams (polarizations).
    
    // --- Derived Fields ---
    
    int64_t                ntsteps = 0;     ///< Number of time integration steps available.
};

} // namespace bliss