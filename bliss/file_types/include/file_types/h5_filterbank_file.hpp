#pragma once

#include <bland/ndarray.hpp>
#include <H5Cpp.h>
#include <core/scan_datasource.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

namespace bliss {

/**
 * @brief Reader for HDF5 Filterbank files.
 * @details Extracts metadata and reads spectral data and masks from HDF5 formatted files.
 * The class eagerly caches metadata upon instantiation to guarantee O(1) retrieval times 
 * during compute-heavy processing loops.
 */
class h5_filterbank_file : public IScanDataSource {
  public:
    /**
     * @brief Constructs an HDF5 Filterbank file reader and builds the metadata cache.
     * @details Opens the file and eagerly loads all relevant metadata into memory.
     * @param file_path The path to the HDF5 file.
     */
    explicit h5_filterbank_file(std::string_view file_path);

    /**
     * @brief Reads a physical subset (hyperslab) of the spectral data from disk.
     * @param offset The starting coordinate for each dimension.
     * @param count The number of elements to read along each dimension.
     * @return A floating-point tensor containing the waterfall data block.
     */
    bland::ndarray read_data(const std::vector<int64_t>& offset = {}, const std::vector<int64_t>& count = {});

    /**
     * @brief Reads the RFI mask hyperslab corresponding to the data extent.
     * @param offset The starting coordinate for each dimension.
     * @param count The number of elements to read along each dimension.
     * @return An unsigned 8-bit integer tensor representing the RFI mask.
     */
    bland::ndarray read_mask(const std::vector<int64_t>& offset = {}, const std::vector<int64_t>& count = {});

    /**
     * @brief Generates a printable representation of the file object.
     * @return Formatted string containing the file path.
     */
    std::string repr();

    /**
     * @brief Returns the system path to the opened HDF5 file.
     * @return The file path string.
     */
    std::string get_file_path() const;

    // --- Cached Metadata Getters ---
    std::vector<int64_t> get_data_shape();
    double get_fch1() const;
    double get_foff() const;
    double get_tsamp() const;
    double get_tstart() const;
    std::string get_source_name() const;

    std::optional<int64_t> get_machine_id() const;
    std::optional<int64_t> get_telescope_id() const;
    std::optional<double> get_src_dej() const;
    std::optional<double> get_src_raj() const;
    std::optional<double> get_az_start() const;
    std::optional<double> get_za_start() const;
    std::optional<int64_t> get_data_type() const;
    std::optional<int64_t> get_nbits() const;
    std::optional<int64_t> get_nchans() const;
    std::optional<int64_t> get_nifs() const;

  private:
    H5::H5File _h5_file_handle;
    H5::DataSet _h5_data_handle;
    H5::DataSet _h5_mask_handle;

    // Cached mandatory metadata
    double _fch1;
    double _foff;
    double _tsamp;
    double _tstart;
    std::string _source_name;

    // Cached optional metadata
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

    std::vector<int64_t> _data_shape;

    /**
     * @brief Opens the HDF5 file and binds the dataset handles.
     * @param file_path The path to the HDF5 file.
     * @throws H5::FileIException if the data dataset cannot be opened.
     */
    void open_file_and_datasets(std::string_view file_path);

    /**
     * @brief Loads all mandatory and optional metadata into the class attributes.
     */
    void load_all_metadata();

    /**
     * @brief Parses dimension labels to determine the physical shape of the data.
     * @return A vector representing the ordered extents of the data tensor.
     */
    std::vector<int64_t> compute_data_shape();

    /**
     * @brief Reads a specific attribute from the HDF5 dataset.
     * @tparam T The expected C++ return type.
     * @param key The name of the attribute.
     * @return The parsed attribute value.
     */
    template <typename T>
    T read_data_attr(const std::string &key) const;

    /**
     * @brief Safely attempts to read an optional attribute.
     * @tparam T The expected C++ return type.
     * @param key The name of the attribute.
     * @return An std::optional containing the value if present, or std::nullopt.
     */
    template <typename T>
    std::optional<T> read_optional(const std::string &key) const {
        try {
            return read_data_attr<T>(key);
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace bliss