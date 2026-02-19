#pragma once

#include <core/scan_datasource.hpp>
#include <H5Cpp.h>
#include <bland/bland.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <stdexcept>
#include <fmt/format.h>

namespace bliss {

/**
 * @brief Concrete implementation of IScanDataSource for HDF5 Filterbank files.
 * * @details This class handles the low-level HDF5 C++ API calls to read data and metadata
 * from `.h5` or `.fil` files that follow the standard filterbank structure (a main `data` 
 * dataset and attributes for metadata). It implements a caching strategy for metadata 
 * to avoid repeated disk access.
 */
class h5_filterbank_file : public IScanDataSource {
  public:
    /**
     * @brief Constructor. Opens the file and actively caches all metadata.
     * @param file_path Path to the HDF5 file.
     * @throws H5::FileIException if the file cannot be opened.
     */
    h5_filterbank_file(std::string_view file_path);

    // --- IScanDataSource Implementation (Data Methods) ---
    
    /**
     * @brief Returns the shape of the data cube [time, feed, frequency].
     * @details The shape is computed once in the constructor (handling dimension labels) and cached.
     */
    std::vector<int64_t> get_data_shape() override;
    
    /**
     * @brief Reads a hyperslab of spectral data from the HDF5 dataset.
     * @param offset The starting indices [time, feed, freq].
     * @param count The number of elements to read along each dimension.
     * @return A `bland::ndarray` containing the data slice.
     */
    bland::ndarray read_data(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) override;
    
    /**
     * @brief Reads a hyperslab of the RFI mask (if present in the file).
     * @return A `bland::ndarray` containing the mask slice (or zeros if no mask exists).
     */
    bland::ndarray read_mask(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) override;
    
    /**
     * @brief Returns the file path associated with this data source.
     */
    std::string get_file_path() const override;


    // --- IScanDataSource Implementation (Mandatory Metadata) ---
    // These methods return values cached during object construction for performance.
    
    double get_fch1() const override;        ///< Frequency of the first channel.
    double get_foff() const override;        ///< Channel bandwidth.
    double get_tsamp() const override;       ///< Time sampling interval.
    double get_tstart() const override;      ///< Start time (MJD).
    std::string get_source_name() const override;

    // --- IScanDataSource Implementation (Optional Metadata) ---
    std::optional<int64_t> get_machine_id() const override;
    std::optional<int64_t> get_telescope_id() const override;
    std::optional<double> get_src_dej() const override;
    std::optional<double> get_src_raj() const override;
    std::optional<double> get_az_start() const override;
    std::optional<double> get_za_start() const override;
    std::optional<int64_t> get_data_type() const override;
    std::optional<int64_t> get_nbits() const override;
    std::optional<int64_t> get_nchans() const override;
    std::optional<int64_t> get_nifs() const override;

    // --- Helper Methods ---
    
    /**
     * @brief Reads an attribute from the root file group (`/`).
     * @tparam T The C++ type to read into (e.g., double, string).
     */
    template <typename T>
    T read_file_attr(const std::string &key) const;

    /**
     * @brief Reads an attribute from the `data` dataset.
     * @tparam T The C++ type to read into.
     */
    template <typename T>
    T read_data_attr(const std::string &key) const;

    std::string repr();

  private:
    H5::H5File  _h5_file_handle;
    H5::DataSet _h5_data_handle;
    std::optional<H5::DataSet> _h5_mask_handle;

    // --- Metadata Cache ---
    // Mandatory fields
    double _fch1;
    double _foff;
    double _tsamp;
    double _tstart;
    std::string _source_name;
    std::vector<int64_t> _data_shape;

    // Optional fields
    std::optional<int64_t> _machine_id;
    std::optional<int64_t> _telescope_id;
    std::optional<double> _src_dej;
    std::optional<double> _src_raj;
    std::optional<double> _az_start;
    std::optional<double> _za_start;
    std::optional<int64_t> _data_type;
    std::optional<int64_t> _nbits;
    std::optional<int64_t> _nchans;
    std::optional<int64_t> _nifs;

    // Helper to safely read optional attributes without throwing
    template <typename T>
    std::optional<T> read_optional(const std::string &key) const {
        try {
            return read_data_attr<T>(key);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    // Internal helper to robustly determine data dimensions and mapping
    std::vector<int64_t> compute_data_shape();
};

// Inline implementation for file attributes
template <typename T>
T bliss::h5_filterbank_file::read_file_attr(const std::string &key) const {
    T val;
    if (_h5_file_handle.attrExists(key)) {
        auto attr  = _h5_file_handle.openAttribute(key);
        auto dtype = attr.getDataType();
        attr.read(dtype, val);
        return val;
    } else {
        auto err_msg = fmt::format("H5 file does not have an attribute key '{}'\n", key);
        throw std::invalid_argument(err_msg);
    }
}

} // namespace bliss