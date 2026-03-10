#pragma once

#include <bland/bland.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace bliss {

/**
 * @brief Abstract interface for a scan data source.
 *
 * This interface decouples the core business logic (the `scan` class) from the
 * specific implementation details of the data storage format (e.g., HDF5, RAW, Socket, Mock).
 * Any class that wishes to provide data to the BLISS pipeline must implement this interface.
 *
 * @details **Performance Note:** Concrete implementations (like `h5_filterbank_file`) are
 * expected to cache metadata (e.g., `fch1`, `tsamp`) upon construction. Reading these values
 * should be instant (O(1)) to avoid blocking the pipeline during high-throughput processing.
 */
class IScanDataSource {
public:
    virtual ~IScanDataSource() = default;

    // --- Data Access Methods ---

    /**
     * @brief Retrieves the shape of the full data tensor.
     * @return A vector representing dimensions, typically `[time, feed, freq]`.
     */
    virtual std::vector<int64_t> get_data_shape() = 0;

    /**
     * @brief Reads a subset of the spectral data (spectrogram).
     * @details This function fetches the actual intensity values (waterfall plot).
     * @param offset The starting index for each dimension (e.g., `{t_start, feed_idx, freq_start}`).
     * @param count The number of elements to read along each dimension.
     * @return A `bland::ndarray` containing the requested data slice.
     */
    virtual bland::ndarray read_data(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) = 0;

    /**
     * @brief Reads a subset of the RFI mask.
     * @details If the source format supports it, this returns the bad-channel or RFI flag mask.
     * @param offset The starting index for each dimension.
     * @param count The number of elements to read along each dimension.
     * @return A `bland::ndarray` containing the requested mask slice (usually boolean or uint8).
     */
    virtual bland::ndarray read_mask(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) = 0;

    /**
     * @brief Returns the file path or identifier of the data source.
     * @return A string representing the source origin (e.g., "/data/file.h5" or "socket:8080").
     */
    virtual std::string get_file_path() const = 0;


    // --- Mandatory Metadata Access ---
    // These fields are critical for the physical interpretation of the scan.
    // Implementations MUST provide valid values or throw an exception if unavailable.

    virtual double get_fch1() const = 0;        ///< Center frequency of the first channel (MHz).
    virtual double get_foff() const = 0;        ///< Frequency resolution / channel width (MHz).
    virtual double get_tsamp() const = 0;       ///< Sampling time per spectrum (seconds).
    virtual double get_tstart() const = 0;      ///< Observation start time (MJD or Unix timestamp).
    virtual std::string get_source_name() const = 0; ///< Name of the observed source.


    // --- Optional Metadata Access ---
    // These fields might not exist in all file formats (e.g., raw data without headers).
    // They return std::optional to explicitly handle missing metadata.

    virtual std::optional<int64_t> get_machine_id() const = 0;      ///< Backend instrument ID.
    virtual std::optional<int64_t> get_telescope_id() const = 0;    ///< Telescope facility ID.
    
    virtual std::optional<double> get_src_dej() const = 0;          ///< Source Declination (J2000).
    virtual std::optional<double> get_src_raj() const = 0;          ///< Source Right Ascension (J2000).
    
    virtual std::optional<double> get_az_start() const = 0;         ///< Telescope Azimuth at start of scan.
    virtual std::optional<double> get_za_start() const = 0;         ///< Telescope Zenith Angle at start of scan.
    
    virtual std::optional<int64_t> get_data_type() const = 0;       ///< Data type identifier (e.g., 1=float32).
    virtual std::optional<int64_t> get_nbits() const = 0;           ///< Number of bits per sample.
    
    virtual std::optional<int64_t> get_nchans() const = 0;          ///< Total number of frequency channels.
    virtual std::optional<int64_t> get_nifs() const = 0;            ///< Number of IF streams (polarizations).
};

} // namespace bliss