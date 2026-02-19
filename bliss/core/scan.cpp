#include "core/scan.hpp"
#include "core/scan_datasource.hpp" 
#include "bland/config.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <array>
#include <tuple>
#include <cmath>

using namespace bliss;

/* * Table of known telescope channelization schemes.
 * Tuple format:
 * 1. Number of fine channels per coarse channel
 * 2. Frequency resolution (Hz/channel or similar metric)
 * 3. Time resolution (seconds)
 * 4. Revision Name / Identifier
 */
// clang-format off
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
// clang-format on

/**
 * @brief Heuristic to deduce the coarse channelization structure from file metadata.
 * @details Compares the file's resolution params against known telescope configurations.
 * @return A tuple containing {number of coarse channels, number of fine channels per coarse}.
 */
std::tuple<int, int>
infer_number_coarse_channels(int number_fine_channels, double foff, double tsamp) {
    for (const auto &channelization : known_channelizations) {
        auto [fine_channels_per_coarse, freq_res, time_res, version] = channelization;

        auto num_coarse_channels = number_fine_channels / fine_channels_per_coarse;
        // Check if this matches an integer number of coarse channels and if resolutions align.
        if (num_coarse_channels * fine_channels_per_coarse == number_fine_channels &&
            std::abs(std::abs(foff) - freq_res) < .1 && std::abs(std::abs(tsamp) - time_res) < .1) {
            return std::make_tuple(num_coarse_channels, std::get<0>(channelization));
        }
    }

    // Fallback logic for unknown channelization schemes
    {
        // Try 2**18 (Matches ATA standard)
        constexpr int common_channelization = 1<<18;
        auto number_coarse = number_fine_channels / common_channelization;
        auto remainder_per_coarse = (number_fine_channels % common_channelization) / number_coarse;
        auto fine_per_coarse = common_channelization + remainder_per_coarse;
        if (fine_per_coarse * number_coarse == number_fine_channels) {
            fmt::print("WARN: scan with {} fine channels could not be matched with a known channelization scheme. "
                    "Rounding from 2**18 fine channels per coarse to give {} coarse channels with {} fine channels each\n",
                    number_fine_channels, number_coarse, fine_per_coarse);
            return {number_coarse, fine_per_coarse};
        }
    }

    {
        // Try 1M (Matches Parkes standard)
        constexpr int common_channelization = 1000000;
        auto number_coarse = number_fine_channels / common_channelization;
        auto remainder_per_coarse = (number_fine_channels % common_channelization) / number_coarse;
        auto fine_per_coarse = common_channelization + remainder_per_coarse;
        if (fine_per_coarse * number_coarse == number_fine_channels) {
            fmt::print("WARN: scan with {} fine channels could not be matched with a known channelization scheme. "
                    "Rounding from 1M fine channels per coarse to give {} coarse channels with {} fine channels each\n",
                    number_fine_channels, number_coarse, fine_per_coarse);
            return {number_coarse, fine_per_coarse};
        }
    }
    fmt::print("WARN: scan with {} fine channels could not be matched with a known channelization scheme. "
            "Rounding to standard known channelization didn't work, so working from 1 coarse channel\n",
            number_fine_channels);
    return {1, number_fine_channels};
}

// Constructor from existing channel map
scan::scan(std::map<int, std::shared_ptr<coarse_channel>> coarse_channels) {
    _coarse_channels = coarse_channels;
    // Grab the first coarse channel to use as a template for metadata
    auto first_cc = _coarse_channels.at(0);

    _num_coarse_channels = _coarse_channels.size();
    
    // Populate metadata struct from the first channel
    _meta.foff        = first_cc->foff();
    _meta.fch1        = first_cc->fch1();
    _meta.nchans      = first_cc->nchans() * _num_coarse_channels;
    _meta.tstart      = first_cc->tstart();
    _meta.tsamp       = first_cc->tsamp();
    _meta.source_name = first_cc->source_name();
    _meta.ntsteps     = first_cc->ntsteps();
    
    // Optional fields
    _meta.machine_id   = first_cc->machine_id(); 
    _meta.nbits        = first_cc->nbits();
    _meta.nifs         = first_cc->nifs();
    _meta.data_type    = first_cc->data_type();
    
    _meta.src_raj      = first_cc->src_raj();
    _meta.src_dej      = first_cc->src_dej();
    _meta.telescope_id = first_cc->telescope_id();
    _meta.az_start     = first_cc->az_start();
    _meta.za_start     = first_cc->za_start();

    _tduration_secs = _meta.ntsteps * _meta.tsamp;
}

// Primary Constructor using DataSource
scan::scan(std::shared_ptr<IScanDataSource> data_source, int num_fine_channels_per_coarse) {
    
    _data_source = data_source;
    
    _coarse_channels = std::map<int, std::shared_ptr<coarse_channel>>();

    // Load metadata via the interface
    _meta.fch1        = _data_source->get_fch1();
    _meta.foff        = _data_source->get_foff();
    _meta.source_name = _data_source->get_source_name();
    _meta.tsamp       = _data_source->get_tsamp();
    _meta.tstart      = _data_source->get_tstart();

    _meta.machine_id   = _data_source->get_machine_id();
    _meta.src_dej      = _data_source->get_src_dej();
    _meta.src_raj      = _data_source->get_src_raj();
    _meta.telescope_id = _data_source->get_telescope_id();
    _meta.az_start     = _data_source->get_az_start();
    _meta.za_start     = _data_source->get_za_start();

    _meta.data_type    = _data_source->get_data_type().value_or(1);
    _meta.nbits        = _data_source->get_nbits();
    _meta.nchans       = _data_source->get_nchans().value_or(0);
    _meta.nifs         = _data_source->get_nifs().value_or(0);

    // Validate data shape
    auto data_shape = _data_source->get_data_shape();
    if (data_shape.size() == 3) {
        _meta.ntsteps = data_shape[0];
        _tduration_secs = _meta.ntsteps * _meta.tsamp;
    } else {
        fmt::print("ERROR: reading data_shape from DataSource did not have 3 dimensions but we expect [time, feed, freq]\n");
    }

    if (_meta.nchans == 0 && data_shape.size() > 2) {
        _meta.nchans = data_shape[2]; 
    }

    // Determine Coarse Channel Structure
    if (num_fine_channels_per_coarse == 0) {
        // Infer automatically
        std::tie(_num_coarse_channels, _fine_channels_per_coarse) =
                infer_number_coarse_channels(_meta.nchans, 1e6 * _meta.foff, _meta.tsamp);
    } else {
        // Use user provided structure
        _num_coarse_channels = _meta.nchans / num_fine_channels_per_coarse;
        _fine_channels_per_coarse = num_fine_channels_per_coarse;
    }

    if (_num_coarse_channels * _fine_channels_per_coarse != _meta.nchans) {
        fmt::print("WARN: the provided number of fine channels per coarse ({}) is not divisible by the total number of channels ({})\n", _fine_channels_per_coarse, _meta.nchans);
    }
}

std::shared_ptr<coarse_channel> bliss::scan::read_coarse_channel(int coarse_channel_index) {
    if (coarse_channel_index < 0 || coarse_channel_index > _num_coarse_channels) {
        throw std::out_of_range("ERROR: invalid coarse channel");
    }

    auto global_offset_in_file = coarse_channel_index + _coarse_channel_offset;
    
    // Lazy Loading: Check if channel is already in cache
    if (_coarse_channels.find(global_offset_in_file) == _coarse_channels.end()) {
        
        auto data_count = _data_source->get_data_shape();
        std::vector<int64_t> data_offset(3, 0);

        // Configure reading window for this specific coarse channel
        data_count[2] = _fine_channels_per_coarse;
        auto global_start_fine_channel = _fine_channels_per_coarse * global_offset_in_file;
        data_offset[2] = global_start_fine_channel;
                  
        // Define Lambdas for Lazy Execution (captured by the coarse_channel)
        auto data_reader = [ds = this->_data_source, data_offset, data_count]() {
            return ds->read_data(data_offset, data_count);
        };
        auto mask_reader = [ds = this->_data_source, data_offset, data_count]() {
            return ds->read_mask(data_offset, data_count);
        };

        auto relative_start_fine_channel = _fine_channels_per_coarse * coarse_channel_index;
        
        // Create specific metadata for this channel
        scan_metadata channel_meta = _meta;
        channel_meta.fch1 = _meta.fch1 + _meta.foff * relative_start_fine_channel;
        channel_meta.nchans = _fine_channels_per_coarse;
        channel_meta.ntsteps = data_count[0]; 

        // Instantiate the coarse_channel with the lazy readers
        auto new_coarse = std::make_shared<coarse_channel>(data_reader,
                                                           mask_reader,
                                                           channel_meta, 
                                                           global_offset_in_file);
        new_coarse->set_device(_device);
        _coarse_channels.insert({global_offset_in_file, new_coarse});
    }
    
    // Retrieve and Apply Transforms
    auto cc = _coarse_channels.at(global_offset_in_file);
    cc->set_device(_device);
    
    auto transformed_cc = *cc;
    for (auto &transform : _coarse_channel_pipeline) {
        transformed_cc = transform.transform(transformed_cc);
    }
    
    return std::make_shared<coarse_channel>(transformed_cc);
}

std::shared_ptr<coarse_channel> bliss::scan::peak_coarse_channel(int coarse_channel_index) {
    if (coarse_channel_index < 0 || coarse_channel_index > _num_coarse_channels) {
        throw std::out_of_range("ERROR: invalid coarse channel");
    }

    auto global_offset_in_file = coarse_channel_index + _coarse_channel_offset;
    if (_coarse_channels.find(global_offset_in_file) != _coarse_channels.end()) {
        auto cc = _coarse_channels.at(global_offset_in_file);
        cc->set_device(_device);
        auto transformed_cc = *cc;
        for (auto &transform : _coarse_channel_pipeline) {
            transformed_cc = transform.transform(transformed_cc);
        }
        return std::make_shared<coarse_channel>(transformed_cc);
    } else {
        return nullptr;
    }
}

void bliss::scan::add_coarse_channel_transform(std::function<coarse_channel(coarse_channel)> transform, std::string description) {
    _coarse_channel_pipeline.push_back({description, transform});
}

int bliss::scan::get_coarse_channel_with_frequency(double frequency) const {
    auto band_fraction = ((frequency - _meta.fch1) / _meta.foff / static_cast<double>(_meta.nchans));
    // TODO: if band_fraction is < 0 or > 1 then it's not in this filterbank. Throw an error
    auto fractional_channel = band_fraction * _num_coarse_channels;
    return std::floor(fractional_channel);
}

int bliss::scan::get_number_coarse_channels() const {
    return _num_coarse_channels;
}

std::string bliss::scan::get_file_path() const {
    if (_data_source != nullptr) {
        return _data_source->get_file_path();
    } else {
        return "n/a";
    }
}

std::list<hit> bliss::scan::hits() {
    std::list<hit> all_hits;
    int            number_coarse_channels = get_number_coarse_channels();
    
    // Iterate over all channels and collect hits
    for (int cc_index = 0; cc_index < number_coarse_channels; ++cc_index) {
        auto cc = read_coarse_channel(cc_index);
        if (cc != nullptr) {
            try {
                auto this_channel_hits = cc->hits();
                all_hits.insert(all_hits.end(), this_channel_hits.cbegin(), this_channel_hits.cend());
            } catch (const std::logic_error &e) {
                fmt::print("WARN: caught exception ({}) while getting hits from pipeline on coarse channel {}, this might indicate a bad pipeline\n", e.what(), cc_index);
            }
        }
    }
    return all_hits;
}

std::pair<float, float> bliss::scan::get_drift_range() {
    std::pair<float, float> drift_range = {0, 0};
    int            number_coarse_channels = get_number_coarse_channels();
    
    for (int cc_index = 0; cc_index < number_coarse_channels; ++cc_index) {
        auto cc = read_coarse_channel(cc_index);
        if (cc != nullptr) {
            try {
                auto drift_rates = cc->integrated_drift_plane().drift_rate_info();
                auto low = drift_rates.front().drift_rate_Hz_per_sec;
                auto high = drift_rates.back().drift_rate_Hz_per_sec;
                for (const auto &drift_rate : drift_rates) {
                    if (drift_rate.drift_rate_Hz_per_sec < low) {
                        low = drift_rate.drift_rate_Hz_per_sec;
                    }
                    if (drift_rate.drift_rate_Hz_per_sec > high) {
                        high = drift_rate.drift_rate_Hz_per_sec;
                    }
                }
                if (low < drift_range.first) {
                    drift_range.first = low;
                }
                if (high > drift_range.second) {
                    drift_range.second = high;
                }
            } catch (const std::logic_error &e) {
                fmt::print("WARN: caught exception ({}) while getting hits from pipeline on coarse channel {}, this might indicate a bad pipeline\n", e.what(), cc_index);
            }
        }
    }
    return drift_range;
}

bland::ndarray::dev bliss::scan::device() {
    return _device;
}

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

void bliss::scan::set_device(std::string_view dev_str, bool verbose) {
    bland::ndarray::dev device = dev_str;
    set_device(device, verbose);
}

void bliss::scan::push_device() {
    for (auto &[channel_index, cc] : _coarse_channels) {
        cc->set_device(_device);
        cc->push_device();
    }
}

bliss::scan bliss::scan::slice_scan_channels(int64_t start_channel, int64_t count) {
    if (count == -1) {
        fmt::print("INFO: Got count of -1 channels, automatically extending to last coarse channel ({})\n", get_number_coarse_channels(), get_number_coarse_channels() - start_channel);
        count = get_number_coarse_channels() - start_channel;
    }

    auto sliced_scan = *this;

    // Apply offset for the virtual slice
    sliced_scan._coarse_channel_offset += start_channel;
    sliced_scan._num_coarse_channels = count;

    // Update metadata to reflect the slice
    sliced_scan._meta.fch1 = _meta.fch1 + _meta.foff * _fine_channels_per_coarse * start_channel;
    sliced_scan._meta.nchans = count * _fine_channels_per_coarse;

    return sliced_scan;
}

// --- GETTERS & SETTERS Implementation (Proxies to _meta) ---

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