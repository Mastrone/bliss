#include "core/scan.hpp"
#include "core/scan_datasource.hpp" 
#include "bland/config.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <array>
#include <tuple>
#include <cmath>
#include <optional>

using namespace bliss;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions)
// ============================================================================
namespace {

    /*
     * Table of known telescope channelization schemes.
     * Tuple format:
     * 1. Number of fine channels per coarse channel
     * 2. Frequency resolution (Hz/channel or similar metric)
     * 3. Time resolution (seconds)
     * 4. Revision Name / Identifier
     */
    constexpr std::array<std::tuple<int, double, double, const char*>, 9> known_channelizations = {{
        {1033216,      2.84, 17.98,       "HSR-Rev1A"},
        {      8, 366210.0,   0.00034953, "HTR-Rev1A"},
        {   1024,   2860.0,   1.06,       "MR-Rev1A"},
        {999424,      2.93, 17.4,        "HSR-Rev1B"},
        {      8, 366210.0,   0.00034953, "HTR-Rev1B"},
        {   1024,   2860.0,   1.02,       "MR-Rev1B"},
        {1048576,       2.79, 18.25,       "HSR-Rev2A"},
        {      8,  366210.0,   0.00034953, "HTR-Rev2A"},
        {   1024,    2860.0,   1.07,       "MR-Rev2A"}
    }};

    /**
     * @brief Attempts to match the dataset resolution to a known telescope channelization scheme.
     * @param number_fine_channels Total fine channels in the data.
     * @param foff Frequency offset.
     * @param tsamp Sampling time.
     * @return Optional tuple of {number of coarse channels, fine channels per coarse}.
     */
    std::optional<std::tuple<int, int>> try_known_schemes(int number_fine_channels, double foff, double tsamp) {
        for (const auto &channelization : known_channelizations) {
            auto [fine_channels_per_coarse, freq_res, time_res, version] = channelization;
            auto num_coarse_channels = number_fine_channels / fine_channels_per_coarse;
            
            if (num_coarse_channels * fine_channels_per_coarse == number_fine_channels &&
                std::abs(std::abs(foff) - freq_res) < 0.1 && std::abs(std::abs(tsamp) - time_res) < 0.1) {
                return std::make_tuple(num_coarse_channels, fine_channels_per_coarse);
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Evaluates a fallback channelization heuristic based on a common baseline.
     * @param number_fine_channels Total fine channels in the data.
     * @param common_channelization The baseline division factor to test.
     * @param label String identifier for logging purposes.
     * @return Optional tuple of {number of coarse channels, fine channels per coarse}.
     */
    std::optional<std::tuple<int, int>> try_fallback_scheme(int number_fine_channels, int common_channelization, const char* label) {
        auto number_coarse = number_fine_channels / common_channelization;
        if (number_coarse == 0) return std::nullopt;

        auto remainder_per_coarse = (number_fine_channels % common_channelization) / number_coarse;
        auto fine_per_coarse = common_channelization + remainder_per_coarse;
        
        if (fine_per_coarse * number_coarse == number_fine_channels) {
            fmt::print("WARN: scan with {} fine channels could not be matched with a known channelization scheme. "
                       "Rounding from {} fine channels per coarse to give {} coarse channels with {} fine channels each\n",
                       number_fine_channels, label, number_coarse, fine_per_coarse);
            return std::make_tuple(number_coarse, fine_per_coarse);
        }
        return std::nullopt;
    }

    /**
     * @brief Extracts hits from a single coarse channel, safely catching operational exceptions.
     * @param all_hits Reference to the master list of hits to append to.
     * @param cc The coarse channel instance to pull hits from.
     * @param cc_index The index of the coarse channel for logging.
     */
    void safely_append_channel_hits(std::list<hit>& all_hits, const std::shared_ptr<coarse_channel>& cc, int cc_index) {
        if (!cc) return;
        try {
            auto this_channel_hits = cc->hits();
            all_hits.insert(all_hits.end(), this_channel_hits.cbegin(), this_channel_hits.cend());
        } catch (const std::logic_error &e) {
            fmt::print("WARN: caught exception ({}) while getting hits from pipeline on coarse channel {}, this might indicate a bad pipeline\n", e.what(), cc_index);
        }
    }

    /**
     * @brief Populates the scan metadata structure using values from an existing coarse channel.
     * @param meta The target metadata struct to populate.
     * @param first_cc The source coarse channel object.
     * @param num_coarse_channels The total number of coarse channels.
     */
    void populate_metadata_from_first_channel(scan_metadata& meta, const std::shared_ptr<coarse_channel>& first_cc, size_t num_coarse_channels) {
        meta.foff         = first_cc->foff();
        meta.fch1         = first_cc->fch1();
        meta.nchans       = first_cc->nchans() * num_coarse_channels;
        meta.tstart       = first_cc->tstart();
        meta.tsamp        = first_cc->tsamp();
        meta.source_name  = first_cc->source_name();
        meta.ntsteps      = first_cc->ntsteps();
        meta.machine_id   = first_cc->machine_id(); 
        meta.nbits        = first_cc->nbits();
        meta.nifs         = first_cc->nifs();
        meta.data_type    = first_cc->data_type();
        meta.src_raj      = first_cc->src_raj();
        meta.src_dej      = first_cc->src_dej();
        meta.telescope_id = first_cc->telescope_id();
        meta.az_start     = first_cc->az_start();
        meta.za_start     = first_cc->za_start();
    }

    /**
     * @brief Validates the physical shape of the data tensor and assigns related metadata.
     * @param meta The target metadata struct to populate.
     * @param data_shape The dimensional extents of the data tensor.
     * @param tduration_secs Reference to store the computed total duration in seconds.
     */
    void validate_and_apply_data_shape(scan_metadata& meta, const std::vector<int64_t>& data_shape, double& tduration_secs) {
        if (data_shape.size() == 3) {
            meta.ntsteps = data_shape[0];
            tduration_secs = meta.ntsteps * meta.tsamp;
        } else {
            fmt::print("ERROR: reading data_shape from DataSource did not have 3 dimensions but we expect [time, feed, freq]\n");
        }

        if (meta.nchans == 0 && data_shape.size() > 2) {
            meta.nchans = data_shape[2]; 
        }
    }

    /**
     * @brief Populates the scan metadata structure using values from a data source.
     * @param meta The target metadata struct to populate.
     * @param ds The underlying data source object.
     * @param tduration_secs Reference to store the computed total duration in seconds.
     */
    void populate_metadata_from_datasource(scan_metadata& meta, const std::shared_ptr<IScanDataSource>& ds, double& tduration_secs) {
        meta.fch1         = ds->get_fch1();
        meta.foff         = ds->get_foff();
        meta.source_name  = ds->get_source_name();
        meta.tsamp        = ds->get_tsamp();
        meta.tstart       = ds->get_tstart();
        meta.machine_id   = ds->get_machine_id();
        meta.src_dej      = ds->get_src_dej();
        meta.src_raj      = ds->get_src_raj();
        meta.telescope_id = ds->get_telescope_id();
        meta.az_start     = ds->get_az_start();
        meta.za_start     = ds->get_za_start();
        meta.data_type    = ds->get_data_type().value_or(1);
        meta.nbits        = ds->get_nbits();
        meta.nchans       = ds->get_nchans().value_or(0);
        meta.nifs         = ds->get_nifs().value_or(0);

        validate_and_apply_data_shape(meta, ds->get_data_shape(), tduration_secs);
    }

    /**
     * @brief Constructs a new coarse channel instance bound to lazy data readers.
     * @param global_offset The physical channel offset within the full file.
     * @param relative_start The relative frequency channel start index.
     * @param fine_chans Number of fine channels per coarse block.
     * @param parent_meta The overarching scan metadata.
     * @param ds The underlying data source for reads.
     * @return A constructed shared pointer to a coarse channel object.
     */
    std::shared_ptr<coarse_channel> build_lazy_coarse_channel(
        int global_offset, 
        int relative_start, 
        int fine_chans, 
        const scan_metadata& parent_meta, 
        std::shared_ptr<IScanDataSource> ds) 
    {
        auto data_count = ds->get_data_shape();
        std::vector<int64_t> data_offset(3, 0);

        data_count[2] = fine_chans;
        data_offset[2] = fine_chans * global_offset;
                  
        auto data_reader = [ds, data_offset, data_count]() { return ds->read_data(data_offset, data_count); };
        auto mask_reader = [ds, data_offset, data_count]() { return ds->read_mask(data_offset, data_count); };

        scan_metadata channel_meta = parent_meta;
        channel_meta.fch1 = parent_meta.fch1 + parent_meta.foff * relative_start;
        channel_meta.nchans = fine_chans;
        channel_meta.ntsteps = data_count[0]; 

        return std::make_shared<coarse_channel>(data_reader, mask_reader, channel_meta, global_offset);
    }
}

// ============================================================================
// CLASS IMPLEMENTATION
// ============================================================================

/**
 * @brief Heuristic to deduce the coarse channelization structure from file metadata.
 * @details Sequentially tests against exact known formats and common fallbacks.
 * @param number_fine_channels Total fine channels in the data.
 * @param foff Frequency offset.
 * @param tsamp Sampling time.
 * @return A tuple containing {number of coarse channels, number of fine channels per coarse}.
 */
std::tuple<int, int> infer_number_coarse_channels(int number_fine_channels, double foff, double tsamp) {
    if (auto known = try_known_schemes(number_fine_channels, foff, tsamp)) {
        return known.value();
    }
    
    if (auto ata_fallback = try_fallback_scheme(number_fine_channels, 1<<18, "2**18")) {
        return ata_fallback.value();
    }

    if (auto parkes_fallback = try_fallback_scheme(number_fine_channels, 1000000, "1M")) {
        return parkes_fallback.value();
    }

    fmt::print("WARN: scan with {} fine channels could not be matched with a known channelization scheme. "
               "Rounding to standard known channelization didn't work, so working from 1 coarse channel\n",
               number_fine_channels);
    return {1, number_fine_channels};
}

/**
 * @brief Constructs a scan from an existing map of initialized coarse channels.
 * @param coarse_channels A pre-populated mapping of indices to coarse channel pointers.
 */
scan::scan(std::map<int, std::shared_ptr<coarse_channel>> coarse_channels) {
    _coarse_channels = coarse_channels;
    _num_coarse_channels = _coarse_channels.size();
    
    populate_metadata_from_first_channel(_meta, _coarse_channels.at(0), _num_coarse_channels);
    _tduration_secs = _meta.ntsteps * _meta.tsamp;
}

/**
 * @brief Constructs a scan backed by an overarching data source.
 * @param data_source The abstract data source handling disk operations.
 * @param num_fine_channels_per_coarse Optional override for the channelization scheme.
 */
scan::scan(std::shared_ptr<IScanDataSource> data_source, int num_fine_channels_per_coarse) {
    _data_source = data_source;
    _coarse_channels = std::map<int, std::shared_ptr<coarse_channel>>();

    populate_metadata_from_datasource(_meta, _data_source, _tduration_secs);

    if (num_fine_channels_per_coarse == 0) {
        std::tie(_num_coarse_channels, _fine_channels_per_coarse) =
                infer_number_coarse_channels(_meta.nchans, 1e6 * _meta.foff, _meta.tsamp);
    } else {
        _num_coarse_channels = _meta.nchans / num_fine_channels_per_coarse;
        _fine_channels_per_coarse = num_fine_channels_per_coarse;
    }

    if (_num_coarse_channels * _fine_channels_per_coarse != _meta.nchans) {
        fmt::print("WARN: the provided number of fine channels per coarse ({}) is not divisible by the total number of channels ({})\n", _fine_channels_per_coarse, _meta.nchans);
    }
}

/**
 * @brief Populates the internal cache with a newly generated coarse channel if it does not already exist.
 * @param coarse_channel_index The relative index of the target coarse channel.
 */
void bliss::scan::ensure_coarse_channel_cached(int coarse_channel_index) {
    auto global_offset = coarse_channel_index + _coarse_channel_offset;
    if (_coarse_channels.find(global_offset) == _coarse_channels.end()) {
        auto relative_start = _fine_channels_per_coarse * coarse_channel_index;
        auto new_coarse = build_lazy_coarse_channel(global_offset, relative_start, _fine_channels_per_coarse, _meta, _data_source);
        new_coarse->set_device(_device);
        _coarse_channels.insert({global_offset, new_coarse});
    }
}

/**
 * @brief Applies the registered pipeline transformations to a given coarse channel.
 * @param cc The source coarse channel to transform.
 * @return A shared pointer to the newly transformed coarse channel.
 */
std::shared_ptr<coarse_channel> bliss::scan::apply_transforms_to_channel(std::shared_ptr<coarse_channel> cc) {
    cc->set_device(_device);
    auto transformed_cc = *cc;
    for (auto &transform : _coarse_channel_pipeline) {
        transformed_cc = transform.transform(transformed_cc);
    }
    return std::make_shared<coarse_channel>(transformed_cc);
}

/**
 * @brief Retrieves a coarse channel, lazily loading it from the data source if necessary.
 * @param coarse_channel_index The requested index relative to the scan's slice offset.
 * @return A shared pointer to the populated and transformed coarse channel.
 * @throws std::out_of_range If the index exceeds bounds.
 */
std::shared_ptr<coarse_channel> bliss::scan::read_coarse_channel(int coarse_channel_index) {
    if (coarse_channel_index < 0 || coarse_channel_index > _num_coarse_channels) {
        throw std::out_of_range("ERROR: invalid coarse channel");
    }
    ensure_coarse_channel_cached(coarse_channel_index);
    auto global_offset = coarse_channel_index + _coarse_channel_offset;
    return apply_transforms_to_channel(_coarse_channels.at(global_offset));
}

/**
 * @brief Peaks into the cache to retrieve a coarse channel without triggering lazy reads.
 * @param coarse_channel_index The requested index relative to the scan's slice offset.
 * @return A shared pointer to the coarse channel if cached, or nullptr if unavailable.
 * @throws std::out_of_range If the index exceeds bounds.
 */
std::shared_ptr<coarse_channel> bliss::scan::peak_coarse_channel(int coarse_channel_index) {
    if (coarse_channel_index < 0 || coarse_channel_index > _num_coarse_channels) {
        throw std::out_of_range("ERROR: invalid coarse channel");
    }
    auto global_offset = coarse_channel_index + _coarse_channel_offset;
    if (_coarse_channels.find(global_offset) != _coarse_channels.end()) {
        return apply_transforms_to_channel(_coarse_channels.at(global_offset));
    }
    return nullptr;
}

/**
 * @brief Adds a lazy transformation function to be applied when reading coarse channels.
 * @param transform The callable transformation function.
 * @param description A text descriptor of the transform stage.
 */
void bliss::scan::add_coarse_channel_transform(std::function<coarse_channel(coarse_channel)> transform, std::string description) {
    _coarse_channel_pipeline.push_back({description, transform});
}

/**
 * @brief Derives the coarse channel index covering a specific target frequency.
 * @param frequency The absolute target frequency in MHz.
 * @return The corresponding coarse channel index.
 */
int bliss::scan::get_coarse_channel_with_frequency(double frequency) const {
    auto band_fraction = ((frequency - _meta.fch1) / _meta.foff / static_cast<double>(_meta.nchans));
    auto fractional_channel = band_fraction * _num_coarse_channels;
    return std::floor(fractional_channel);
}

/**
 * @brief Retrieves the total number of defined coarse channels in this scan.
 * @return Count of coarse channels.
 */
int bliss::scan::get_number_coarse_channels() const {
    return _num_coarse_channels;
}

/**
 * @brief Retrieves the physical file path of the underlying data source.
 * @return The string path, or "n/a" if strictly memory backed.
 */
std::string bliss::scan::get_file_path() const {
    if (_data_source != nullptr) {
        return _data_source->get_file_path();
    } else {
        return "n/a";
    }
}

/**
 * @brief Iterates all valid coarse channels and aggregates detected signals.
 * @return A unified list of all hits discovered across the entire scan.
 */
std::list<hit> bliss::scan::hits() {
    std::list<hit> all_hits;
    int number_coarse_channels = get_number_coarse_channels();
    
    for (int cc_index = 0; cc_index < number_coarse_channels; ++cc_index) {
        auto cc = read_coarse_channel(cc_index);
        safely_append_channel_hits(all_hits, cc, cc_index);
    }
    return all_hits;
}

/**
 * @brief Retrieves the active compute device for tensor operations.
 * @return A bland::ndarray::dev struct.
 */
bland::ndarray::dev bliss::scan::device() {
    return _device;
}

/**
 * @brief Validates and binds a target compute device to the scan and its children.
 * @param device The target compute hardware identifier.
 * @param verbose Enables logging of the binding process.
 * @throws std::runtime_error If the requested CUDA device is invalid or missing.
 */
void bliss::scan::set_device(bland::ndarray::dev &device, bool verbose) {
    _device = device;

    if (device.device_type == bland::ndarray::dev::cuda.device_type ||
        device.device_type == bland::ndarray::dev::cuda_managed.device_type) {
        if (!bland::g_config.check_is_valid_cuda_device(device.device_id, verbose)) {
            throw std::runtime_error("set_device received invalid cuda device");
        }
    }

    for (auto &[channel_index, cc] : _coarse_channels) {
        cc->set_device(device);
    }
}

/**
 * @brief String-based overload to bind a target compute device.
 * @param dev_str The hardware identifier string (e.g. "cuda:0").
 * @param verbose Enables logging.
 */
void bliss::scan::set_device(std::string_view dev_str, bool verbose) {
    bland::ndarray::dev device = dev_str;
    set_device(device, verbose);
}

/**
 * @brief Forces all cached child channels to actively transfer data to the configured compute device.
 */
void bliss::scan::push_device() {
    for (auto &[channel_index, cc] : _coarse_channels) {
        cc->set_device(_device);
        cc->push_device();
    }
}

/**
 * @brief Generates a virtual scan representing a subset of the coarse channels.
 * @param start_channel The index offset to begin the slice.
 * @param count The number of consecutive channels to include.
 * @return A new scan object bound to the same data source but with restricted bounds.
 */
bliss::scan bliss::scan::slice_scan_channels(int64_t start_channel, int64_t count) {
    if (count == -1) {
        fmt::print("INFO: Got count of -1 channels, automatically extending to last coarse channel ({})\n", get_number_coarse_channels(), get_number_coarse_channels() - start_channel);
        count = get_number_coarse_channels() - start_channel;
    }

    auto sliced_scan = *this;

    sliced_scan._coarse_channel_offset += start_channel;
    sliced_scan._num_coarse_channels = count;

    sliced_scan._meta.fch1 = _meta.fch1 + _meta.foff * _fine_channels_per_coarse * start_channel;
    sliced_scan._meta.nchans = count * _fine_channels_per_coarse;

    return sliced_scan;
}

// --- GETTERS & SETTERS Implementation ---

double bliss::scan::fch1() const { return _meta.fch1; }
void bliss::scan::set_fch1(double fch1) { _meta.fch1 = fch1; }

double bliss::scan::foff() const { return _meta.foff; }
void bliss::scan::set_foff(double foff) { _meta.foff = foff; }

int64_t bliss::scan::machine_id() const { return _meta.machine_id.value_or(0); }
void bliss::scan::set_machine_id(int64_t machine_id) { _meta.machine_id = machine_id; }

int64_t bliss::scan::nbits() const { return _meta.nbits.value_or(0); }
void bliss::scan::set_nbits(int64_t nbits) { _meta.nbits = nbits; }

int64_t bliss::scan::nchans() const { return _meta.nchans; }
void bliss::scan::set_nchans(int64_t nchans) { _meta.nchans = nchans; }

int64_t bliss::scan::nifs() const { return _meta.nifs; }
void bliss::scan::set_nifs(int64_t nifs) { _meta.nifs = nifs; }

std::string bliss::scan::source_name() const { return _meta.source_name; }
void bliss::scan::set_source_name(std::string source_name) { _meta.source_name = source_name; }

double bliss::scan::src_dej() const { return _meta.src_dej.value_or(0.0); }
void bliss::scan::set_src_dej(double src_dej) { _meta.src_dej = src_dej; }

double bliss::scan::src_raj() const { return _meta.src_raj.value_or(0.0); }
void bliss::scan::set_src_raj(double src_raj) { _meta.src_raj = src_raj; }

int64_t bliss::scan::telescope_id() const { return _meta.telescope_id.value_or(0); }
void bliss::scan::set_telescope_id(int64_t telescope_id) { _meta.telescope_id = telescope_id; }

double bliss::scan::tsamp() const { return _meta.tsamp; }
void bliss::scan::set_tsamp(double tsamp) { _meta.tsamp = tsamp; }

double bliss::scan::tstart() const { return _meta.tstart; }
void bliss::scan::set_tstart(double tstart) { _meta.tstart = tstart; }

int64_t bliss::scan::data_type() const { return _meta.data_type; }
void bliss::scan::set_data_type(int64_t data_type) { _meta.data_type = data_type; }

double bliss::scan::az_start() const { return _meta.az_start.value_or(0.0); }
void bliss::scan::set_az_start(double az_start) { _meta.az_start = az_start; }

double bliss::scan::za_start() const { return _meta.za_start.value_or(0.0); }
void bliss::scan::set_za_start(double za_start) { _meta.za_start = za_start; }

int64_t bliss::scan::ntsteps() const { return _meta.ntsteps; }

double bliss::scan::tduration_secs() const { return _tduration_secs; }