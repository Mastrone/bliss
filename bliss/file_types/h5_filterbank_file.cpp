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
#include <utility> // For std::swap

using namespace bliss;

constexpr bool pedantic = false;

// 

// ============================================================================
// 1. TEMPLATE SPECIALIZATIONS (MUST BE AT THE TOP)
// ============================================================================

/**
 * @brief Specialization for reading std::string attributes from HDF5.
 * @details HDF5 handles strings differently than Plain Old Data (POD) types. 
 * This specialization checks if the attribute is a fixed-length string or a 
 * variable-length string, allocates memory accordingly, and ensures that 
 * dynamically allocated C-strings by the HDF5 C-API are properly freed to prevent memory leaks.
 * @param key The name of the attribute to read.
 * @return The parsed string value.
 * @throws std::invalid_argument if the attribute doesn't exist or is not a string type.
 */
template <>
std::string bliss::h5_filterbank_file::read_data_attr<std::string>(const std::string &key) const {
    if (!_h5_data_handle.attrExists(key)) {
        throw std::invalid_argument(fmt::format("H5 data does not have an attribute key {}", key));
    }

    auto attr  = _h5_data_handle.openAttribute(key);
    auto dtype = attr.getDataType();

    if (dtype.getClass() != H5T_STRING) {
        throw std::invalid_argument(fmt::format("{} expected as string but is not a string type", key));
    }

    if (!dtype.isVariableStr()) {
        // Fixed length string
        hsize_t size = attr.getInMemDataSize();
        std::vector<uint8_t> val(size);
        attr.read(dtype, val.data());
        return std::string(val.begin(), val.end());
    } 
    
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
}

/**
 * @brief Specialization for reading a vector of strings from HDF5.
 * @details Often used for metadata arrays like 'DIMENSION_LABELS'. Requires reading an 
 * array of C-string pointers and converting them safely to a C++ std::vector<std::string>.
 * @param key The name of the attribute array to read.
 * @return A vector of parsed string values.
 */
template <>
std::vector<std::string> bliss::h5_filterbank_file::read_data_attr<std::vector<std::string>>(const std::string &key) const {
    if (!_h5_data_handle.attrExists(key)) {
        throw std::invalid_argument("H5 data does not have an attribute key");
    }

    H5::Attribute            attr = _h5_data_handle.openAttribute(key);
    std::vector<const char *> vals_as_read_from_h5(attr.getInMemDataSize() / sizeof(char *));
    attr.read(attr.getDataType(), vals_as_read_from_h5.data());

    std::vector<std::string> vals;
    vals.reserve(vals_as_read_from_h5.size());
    for (const char* v : vals_as_read_from_h5) {
        vals.emplace_back(v);
    }
    return vals;
}

// ============================================================================
// 2. GENERIC TEMPLATE IMPLEMENTATION
// ============================================================================

/**
 * @brief Generic reader for arithmetic attribute types (int, float, double).
 * @details Dynamically maps HDF5 native memory types (which are endian-aware) to 
 * standard C++ types to ensure correct reading regardless of the host system's architecture 
 * or the file's endianness.
 * @tparam T The expected C++ return type (must be arithmetic).
 * @param key The name of the attribute to read.
 * @return The parsed arithmetic value casted to type T.
 */
template <typename T>
T bliss::h5_filterbank_file::read_data_attr(const std::string &key) const {
    if constexpr (!std::is_arithmetic_v<T>) {
        throw std::runtime_error("read_data_attr generic called for non-arithmetic type without specialization");
    } 
    
    if (!_h5_data_handle.attrExists(key)) {
        throw std::invalid_argument(fmt::format("H5 data does not have an attribute key {}", key));
    }

    auto attr  = _h5_data_handle.openAttribute(key);
    auto dtype = attr.getDataType();

    // Match HDF5 types to C++ types dynamically using guard clauses
    if (dtype == H5::PredType::NATIVE_INT16)  { int16_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_UINT16) { uint16_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_INT32)  { int32_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_UINT32) { uint32_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_INT64)  { int64_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_UINT64) { uint64_t v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_FLOAT)  { float v; attr.read(dtype, &v); return static_cast<T>(v); }
    if (dtype == H5::PredType::NATIVE_DOUBLE) { double v; attr.read(dtype, &v); return static_cast<T>(v); }

    // Fallback for simple types
    T val;
    attr.read(dtype, &val);
    return val;
}

// ============================================================================
// 3. CONSTRUCTOR & CACHING LOGIC
// ============================================================================

/**
 * @brief Constructs an HDF5 Filterbank file reader and builds the metadata cache.
 * @details Opens the file and eagerly loads all relevant metadata into memory. 
 * This aggressive caching guarantees that subsequent metadata queries (like `get_tsamp()`)
 * execute in O(1) time without triggering slow disk I/O operations, meeting the high-performance 
 * requirements of the pipeline's computational loops.
 * @param file_path The path to the HDF5 file.
 */
bliss::h5_filterbank_file::h5_filterbank_file(std::string_view file_path) {
    // Setup HDF5 Plugins (legacy support)
    #if H5_VERSION_GE(1, 10, 1)
    unsigned int number_plugin_paths;
    H5PLsize(&number_plugin_paths);
    // (Verbose plugin logging omitted for cleanliness, can be re-added if needed)
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
    // These calls will throw if the attribute is missing, which is the correct behavior for mandatory fields.
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
 * @brief Helper to determine the standard [time, feed, freq] shape of the dataset.
 * @details Reads the 'DIMENSION_LABELS' attribute to map the physical dimensions correctly. 
 * Falls back to raw dimensions if labels are missing. It also includes heuristics to swap 
 * dimensions if they appear incorrect based on the total channel count (a known workaround 
 * for some legacy or non-compliant HDF5 files in the SETI community).
 * @return A vector representing the ordered extents of the data tensor.
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
        return std::vector<int64_t>(dims.begin(), dims.end());
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
    bool has_time_and_freq = std::get<1>(time_steps) && std::get<1>(freq_bins);
    
    if (has_time_and_freq && _nchans.has_value()) {
        if (std::get<0>(time_steps) == _nchans.value() && std::get<0>(freq_bins) != _nchans.value()) {
            std::swap(freq_bins, time_steps);
        }
    }

    // Construct standard shape [time, feed, freq]
    if (std::get<1>(time_steps) && std::get<1>(freq_bins) && std::get<1>(feed_id)) {
        return {std::get<0>(time_steps), std::get<0>(feed_id), std::get<0>(freq_bins)};
    }
    
    return std::vector<int64_t>(dims.begin(), dims.end());
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

// 

/**
 * @brief Reads a physical subset (hyperslab) of the spectral data from disk.
 * @details Translates multi-dimensional offsets into an HDF5 hyperslab selection, 
 * pulls the block of data into host CPU memory, and encapsulates it in a `bland::ndarray` tensor. 
 * The single-element 'feed' dimension is squeezed out to simplify downstream matrix operations.
 * @param offset The starting coordinate for each dimension.
 * @param count The number of elements to read along each dimension.
 * @return A floating-point tensor containing the waterfall data block.
 */
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

/**
 * @brief Reads the RFI mask hyperslab corresponding to the data extent.
 * @details Currently generates an empty (zero-filled) mask representing no pre-flagged RFI. 
 * Provides the hook to read physical mask datasets if they are present in future files.
 * @param offset The starting coordinate for each dimension.
 * @param count The number of elements to read along each dimension.
 * @return An unsigned 8-bit integer tensor representing the RFI mask.
 */
bland::ndarray bliss::h5_filterbank_file::read_mask(const std::vector<int64_t>& offset, const std::vector<int64_t>& count) {
    auto shape = get_data_shape();
    std::vector<int64_t> actual_count = count.empty() ? shape : count; 
    
    // Currently returns a zero-filled mask.
    auto mask = bland::zeros(actual_count, bland::ndarray::datatype::uint8, bland::ndarray::dev::cpu);
    // If we wanted to implement real mask reading, we would use _h5_mask_handle here similar to read_data
    return mask.squeeze(1);
}

/**
 * @brief Generates a printable representation of the file object.
 */
std::string bliss::h5_filterbank_file::repr() {
    return fmt::format("File at {}", _h5_file_handle.getFileName());
}

/**
 * @brief Returns the system path to the opened HDF5 file.
 */
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