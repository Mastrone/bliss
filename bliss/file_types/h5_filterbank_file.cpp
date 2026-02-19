#include "file_types/h5_filterbank_file.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <bland/bland.hpp>
#include <iostream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <type_traits> 

using namespace bliss;

constexpr bool pedantic = false;

// ============================================================================
// 1. TEMPLATE SPECIALIZATIONS (MUST BE AT THE TOP)
// ============================================================================

/**
 * @brief Specialization for reading std::string attributes.
 * @details HDF5 handles strings differently than POD types. We need to check if it's
 * a fixed-length string or a variable-length string and allocate memory accordingly.
 */
template <>
std::string bliss::h5_filterbank_file::read_data_attr<std::string>(const std::string &key) const {
    if (_h5_data_handle.attrExists(key)) {
        auto attr  = _h5_data_handle.openAttribute(key);
        auto dtype = attr.getDataType();

        if (dtype.getClass() == H5T_STRING && !dtype.isVariableStr()) {
            // Fixed length string
            hsize_t size = attr.getInMemDataSize();
            std::vector<uint8_t> val(size);
            attr.read(dtype, val.data());
            return std::string(val.begin(), val.end());
        } else if (dtype.getClass() == H5T_STRING && dtype.isVariableStr()) {
            // Variable length string
            char* val;
            attr.read(dtype, &val);
            std::string result(val);
            // Handle memory freeing for HDF5 allocated C-strings
            #if H5_VERSION_GE(1, 8, 13)
            H5free_memory(val);
            #else
            free(val);
            #endif
            return result;
        } else {
            throw std::invalid_argument(fmt::format("{} expected as string but is not a string type", key));
        }
    } else {
        throw std::invalid_argument(fmt::format("H5 data does not have an attribute key {}", key));
    }
}

/**
 * @brief Specialization for reading a vector of strings.
 * @details Often used for 'DIMENSION_LABELS'. Requires reading an array of C-string pointers.
 */
template <>
std::vector<std::string> bliss::h5_filterbank_file::read_data_attr<std::vector<std::string>>(const std::string &key) const {
    if (_h5_data_handle.attrExists(key)) {
        H5::Attribute             attr = _h5_data_handle.openAttribute(key);
        std::vector<const char *> vals_as_read_from_h5(attr.getInMemDataSize() / sizeof(char *));
        attr.read(attr.getDataType(), vals_as_read_from_h5.data());

        std::vector<std::string> vals;
        for (size_t i = 0; i < vals_as_read_from_h5.size(); ++i) {
            vals.emplace_back(vals_as_read_from_h5[i]);
        }
        return vals;
    } else {
        throw std::invalid_argument("H5 data does not have an attribute key");
    }
}

// ============================================================================
// 2. GENERIC TEMPLATE IMPLEMENTATION
// ============================================================================

/**
 * @brief Generic reader for arithmetic types (int, float, double).
 * @details Maps HDF5 native types to C++ types to ensure correct reading regardless of file endianness.
 */
template <typename T>
T bliss::h5_filterbank_file::read_data_attr(const std::string &key) const {
    if constexpr (!std::is_arithmetic_v<T>) {
        throw std::runtime_error("read_data_attr generic called for non-arithmetic type without specialization");
    } else {
        T val;
        if (_h5_data_handle.attrExists(key)) {
            auto attr  = _h5_data_handle.openAttribute(key);
            auto dtype = attr.getDataType();

            // Match HDF5 types to C++ types dynamically
            if (dtype == H5::PredType::NATIVE_INT16) {
                int16_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_UINT16) {
                uint16_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_INT32) {
                int32_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_UINT32) {
                uint32_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_INT64) {
                int64_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_UINT64) {
                uint64_t v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_FLOAT) {
                float v; attr.read(dtype, &v); return static_cast<T>(v);
            } else if (dtype == H5::PredType::NATIVE_DOUBLE) {
                double v; attr.read(dtype, &v); return static_cast<T>(v);
            } else {
                // Fallback for simple types
                attr.read(dtype, &val);
                return val;
            }
        } else {
            throw std::invalid_argument(fmt::format("H5 data does not have an attribute key {}", key));
        }
    }
}

// ============================================================================
// 3. CONSTRUCTOR & CACHING LOGIC
// ============================================================================

bliss::h5_filterbank_file::h5_filterbank_file(std::string_view file_path) {
    // Setup HDF5 Plugins (legacy support)
    #if H5_VERSION_GE(1, 10, 1)
    unsigned int number_plugin_paths;
    H5PLsize(&number_plugin_paths);
    // (Ommesso log verbose plugin per pulizia, riaggiungibile se serve)
    #endif

    // 1. Open File and Dataset
    _h5_file_handle = H5::H5File(file_path.data(), H5F_ACC_RDONLY);
    try {
        _h5_data_handle = _h5_file_handle.openDataSet("data");
    } catch (H5::FileIException h5_data_exception) {
        fmt::print("ERROR: h5_filterbank_file: got an exception while reading data. Cannot continue.\n");
        throw h5_data_exception;
    }

    // Try to open the mask dataset if it exists
    try {
        if (H5Lexists(_h5_file_handle.getId(), "mask", H5P_DEFAULT)) {
             _h5_mask_handle = _h5_file_handle.openDataSet("mask");
        }
    } catch (...) {
        // Mask is optional, ignore errors
    }

    // 2. Read and Cache Mandatory Metadata
    // These calls will throw if the attribute is missing, which is correct for mandatory fields.
    _fch1        = read_data_attr<double>("fch1");
    _foff        = read_data_attr<double>("foff");
    _tsamp       = read_data_attr<double>("tsamp");
    _tstart      = read_data_attr<double>("tstart");
    _source_name = read_data_attr<std::string>("source_name");

    // 3. Read and Cache Optional Metadata
    // Uses helper to catch exceptions and return std::nullopt if missing.
    _machine_id   = read_optional<int64_t>("machine_id");
    _telescope_id = read_optional<int64_t>("telescope_id");
    _src_dej      = read_optional<double>("src_dej");
    _src_raj      = read_optional<double>("src_raj");
    _az_start     = read_optional<double>("az_start");
    _za_start     = read_optional<double>("za_start");
    _data_type    = read_optional<int64_t>("data_type");
    _nbits        = read_optional<int64_t>("nbits");
    _nchans       = read_optional<int64_t>("nchans");
    _nifs         = read_optional<int64_t>("nifs");

    // 4. Compute and Cache Data Shape
    // This involves reading dimension labels and logic, so we do it once here.
    _data_shape = compute_data_shape();
}

/**
 * @brief Helper to determine the [time, feed, freq] shape.
 * @details Reads DIMENSION_LABELS to map dimensions correctly. Falls back to raw dimensions
 * if labels are missing. Also includes logic to swap dimensions if they appear incorrect
 * based on channel count (a known issue with some legacy files).
 */
std::vector<int64_t> bliss::h5_filterbank_file::compute_data_shape() {
    auto dataspace   = _h5_data_handle.getSpace();
    auto number_dims = dataspace.getSimpleExtentNdims();
    std::vector<hsize_t> dims(number_dims);
    dataspace.getSimpleExtentDims(dims.data());

    std::vector<std::string> dim_labels;
    try {
        dim_labels = read_data_attr<std::vector<std::string>>("DIMENSION_LABELS");
    } catch (...) {
        // Fallback: return raw dims if labels missing
        std::vector<int64_t> shape(dims.begin(), dims.end());
        return shape;
    }

    std::tuple<int64_t, bool, int> time_steps = {0, false, -1};
    std::tuple<int64_t, bool, int> feed_id    = {0, false, -1};
    std::tuple<int64_t, bool, int> freq_bins  = {0, false, -1};

    // Map dimensions based on labels
    for (int ii = 0; ii < dims.size(); ++ii) {
        if (!std::get<1>(time_steps) && dim_labels[ii] == "time") {
            time_steps = {dims[ii], true, ii};
        } else if (!std::get<1>(freq_bins) && dim_labels[ii] == "frequency") {
            freq_bins = {dims[ii], true, ii};
        } else if (!std::get<1>(feed_id) && dim_labels[ii] == "feed_id") {
            feed_id = {dims[ii], true, ii};
        }
    }

    // Workaround for known issues where labels might be swapped
    if (std::get<1>(time_steps) && std::get<1>(freq_bins)) {
        if (_nchans.has_value()) {
            // If the dimension labeled "time" matches the channel count, swap them.
            if (std::get<0>(time_steps) == _nchans.value() && std::get<0>(freq_bins) != _nchans.value()) {
                auto temp = freq_bins;
                freq_bins = time_steps;
                time_steps = temp;
            }
        }
    }

    // Construct standard shape [time, feed, freq]
    if (std::get<1>(time_steps) && std::get<1>(freq_bins) && std::get<1>(feed_id)) {
        return {std::get<0>(time_steps), std::get<0>(feed_id), std::get<0>(freq_bins)};
    } else {
        return std::vector<int64_t>(dims.begin(), dims.end());
    }
}

// ============================================================================
// 4. GETTER IMPLEMENTATIONS (Returning Cached Values)
// ============================================================================

std::vector<int64_t> bliss::h5_filterbank_file::get_data_shape() { return _data_shape; }

double bliss::h5_filterbank_file::get_fch1() const { return _fch1; }
double bliss::h5_filterbank_file::get_foff() const { return _foff; }
double bliss::h5_filterbank_file::get_tsamp() const { return _tsamp; }
double bliss::h5_filterbank_file::get_tstart() const { return _tstart; }
std::string bliss::h5_filterbank_file::get_source_name() const { return _source_name; }

std::optional<int64_t> bliss::h5_filterbank_file::get_machine_id() const { return _machine_id; }
std::optional<int64_t> bliss::h5_filterbank_file::get_telescope_id() const { return _telescope_id; }
std::optional<double> bliss::h5_filterbank_file::get_src_dej() const { return _src_dej; }
std::optional<double> bliss::h5_filterbank_file::get_src_raj() const { return _src_raj; }
std::optional<double> bliss::h5_filterbank_file::get_az_start() const { return _az_start; }
std::optional<double> bliss::h5_filterbank_file::get_za_start() const { return _za_start; }
std::optional<int64_t> bliss::h5_filterbank_file::get_data_type() const { return _data_type; }
std::optional<int64_t> bliss::h5_filterbank_file::get_nbits() const { return _nbits; }
std::optional<int64_t> bliss::h5_filterbank_file::get_nchans() const { return _nchans; }
std::optional<int64_t> bliss::h5_filterbank_file::get_nifs() const { return _nifs; }

// ============================================================================
// 5. DATA READING METHODS
// ============================================================================

bland::ndarray bliss::h5_filterbank_file::read_data(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) {
    auto dataspace = _h5_data_handle.getSpace();
    auto shape = get_data_shape(); // Uses cached shape
    
    // Calculate actual offsets and counts
    std::vector<int64_t> actual_offset = offset.empty() ? std::vector<int64_t>(shape.size(), 0) : offset;
    std::vector<int64_t> actual_count = count;
    if (actual_count.empty()) {
        actual_count = shape;
        for(size_t i=0; i<shape.size(); ++i) actual_count[i] -= actual_offset[i];
    }

    // Allocate host memory (CPU)
    bland::ndarray spectrum_grid(actual_count, bland::ndarray::datatype::float32, bland::ndarray::dev::cpu);
    
    std::vector<hsize_t> off(actual_offset.begin(), actual_offset.end());
    std::vector<hsize_t> cnt(actual_count.begin(), actual_count.end());
    
    // Select Hyperslab in the file
    dataspace.selectHyperslab(H5S_SELECT_SET, cnt.data(), off.data());
    H5::DataSpace memspace(cnt.size(), cnt.data());
    
    // Perform Read
    _h5_data_handle.read(spectrum_grid.data_ptr<float>(), H5::PredType::NATIVE_FLOAT, memspace, dataspace);
    
    // Squeeze the feed dimension if it is 1 (standard filterbank format usually has 1 feed)
    return spectrum_grid.squeeze(1);
}

bland::ndarray bliss::h5_filterbank_file::read_mask(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) {
    auto shape = get_data_shape();
    std::vector<int64_t> actual_count = count.empty() ? shape : count; 
    
    // Currently returns a zero-filled mask.
    auto mask = bland::zeros(actual_count, bland::ndarray::datatype::uint8, bland::ndarray::dev::cpu);
    // If we wanted to implement real mask reading, we would use _h5_mask_handle here similar to read_data
    return mask.squeeze(1);
}

std::string bliss::h5_filterbank_file::repr() {
    return fmt::format("File at {}", _h5_file_handle.getFileName());
}

std::string bliss::h5_filterbank_file::get_file_path() const {
    return _h5_file_handle.getFileName();
}

// EXPLICIT INSTANTIATIONS (Required for linking the template methods)
template float       bliss::h5_filterbank_file::read_data_attr<float>(const std::string &key) const;
template double      bliss::h5_filterbank_file::read_data_attr<double>(const std::string &key) const;
template uint8_t     bliss::h5_filterbank_file::read_data_attr<uint8_t>(const std::string &key) const;
template uint16_t    bliss::h5_filterbank_file::read_data_attr<uint16_t>(const std::string &key) const;
template uint32_t    bliss::h5_filterbank_file::read_data_attr<uint32_t>(const std::string &key) const;
template uint64_t    bliss::h5_filterbank_file::read_data_attr<uint64_t>(const std::string &key) const;
template int8_t      bliss::h5_filterbank_file::read_data_attr<int8_t>(const std::string &key) const;
template int16_t     bliss::h5_filterbank_file::read_data_attr<int16_t>(const std::string &key) const;
template int32_t     bliss::h5_filterbank_file::read_data_attr<int32_t>(const std::string &key) const;
template int64_t     bliss::h5_filterbank_file::read_data_attr<int64_t>(const std::string &key) const;