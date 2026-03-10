#include "core/coarse_channel.hpp"

#include <bland/config.hpp>

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <variant>

using namespace bliss;

// Metadata-only constructor
coarse_channel::coarse_channel(scan_metadata metadata, int64_t coarse_channel_number)
    : _coarse_channel_number(coarse_channel_number), _meta(metadata) {}

// Lazy loading constructor
coarse_channel::coarse_channel(std::function<bland::ndarray()> data,
                               std::function<bland::ndarray()> mask,
                               scan_metadata metadata,
                               int64_t coarse_channel_number)
    : _coarse_channel_number(coarse_channel_number), _meta(metadata) {
    _data = data();
    _mask = mask();
}

// Direct data constructor
coarse_channel::coarse_channel(bland::ndarray data,
                               bland::ndarray mask,
                               scan_metadata metadata,
                               int64_t coarse_channel_number)
    : _coarse_channel_number(coarse_channel_number), _meta(metadata) {
    _data = data;
    _mask = mask;
}

bland::ndarray bliss::coarse_channel::data() {
    // Check if the data resides on the correct device.
    // If the internal storage (_data) is on CPU but the channel is configured for GPU,
    // we perform the transfer now and update the internal storage to avoid future transfers.
    if (_data.device().device_type != _device.device_type || _data.device().device_id != _device.device_id) {
        _data = _data.to(_device);
    }
    return _data;
}

void bliss::coarse_channel::set_data(bland::ndarray new_data) {
    _data = new_data;
    // Enforce consistency with the currently configured device.
    if (_data.device().device_type != _device.device_type || _data.device().device_id != _device.device_id) {
        _data = _data.to(_device);
    }
}

bland::ndarray bliss::coarse_channel::mask() {
    // Ensure mask resides on the configured device before returning.
    if (_mask.device().device_type != _device.device_type || _mask.device().device_id != _device.device_id) {
        _mask = _mask.to(_device);
    }
    return _mask;
}

void bliss::coarse_channel::set_mask(bland::ndarray new_mask) {
    _mask = new_mask;
    if (_mask.device().device_type != _device.device_type || _mask.device().device_id != _device.device_id) {
        _mask = _mask.to(_device);
    }
}

noise_stats bliss::coarse_channel::noise_estimate() const {
    if (_noise_stats.has_value()) {
        return _noise_stats.value();
    } else {
        fmt::print("coarse_channel::noise_estimate: requested noise estimate which does not exist yet.");
        throw std::logic_error("coarse_channel::noise_estimate: requested noise estimate which does not exist");
    }
}

void bliss::coarse_channel::set_noise_estimate(noise_stats estimate) {
    _noise_stats = estimate;
}

bool bliss::coarse_channel::has_hits() {
    if (_hits == nullptr) {
        return false;
    } else {
        return true;
    }
}

std::list<hit> bliss::coarse_channel::hits() const {
    if (_hits == nullptr) {
        throw std::logic_error("hits not set");
    }
    
    // Check if the hits are stored as a generator function (lazy evaluation).
    if (std::holds_alternative<std::function<std::list<hit>()>>(*_hits)) {
        // 1. Execute the generator function to perform the hit search.
        auto computed_hits = std::get<std::function<std::list<hit>()>>(*_hits)();
        
        // 2. Cache the result by overwriting the variant.
        // Future calls will access the list directly, avoiding re-computation.
        *_hits = computed_hits;
    }
    return std::get<std::list<hit>>(*_hits);
}

void bliss::coarse_channel::set_hits(std::list<hit> new_hits) {
    _hits = std::make_shared<std::variant<std::list<hit>, std::function<std::list<hit>()>>>(new_hits);
}

void bliss::coarse_channel::set_hits(std::function<std::list<hit>()> find_hits_func) {
    _hits = std::make_shared<std::variant<std::list<hit>, std::function<std::list<hit>()>>>(find_hits_func);
}

bland::ndarray::dev bliss::coarse_channel::device() {
    return _device;
}

void bliss::coarse_channel::set_device(bland::ndarray::dev &device) {
    if (device.device_type == bland::ndarray::dev::cuda.device_type ||
        device.device_type == bland::ndarray::dev::cuda_managed.device_type) {
        if (!bland::g_config.check_is_valid_cuda_device(device.device_id)) {
            fmt::print("The selected device id either does not exist or has a compute capability that is not compatible with this build\n");
            throw std::runtime_error("set_device received invalid cuda device");
        }
    }
    _device = device;
    // Note: set_device strictly configures the target device.
    // The actual memory transfer is deferred until data access or an explicit push_device() call.
}

void bliss::coarse_channel::set_device(std::string_view device) {
    bland::ndarray::dev proper_dev = device;
    set_device(proper_dev);
}

void bliss::coarse_channel::push_device() {
    // Immediately transfer all data components to the configured device.
    if (_data.device().device_type != _device.device_type || _data.device().device_id != _device.device_id) {
        _data = _data.to(_device);
    }
    if (_mask.device().device_type != _device.device_type || _mask.device().device_id != _device.device_id) {
        _mask = _mask.to(_device);
    }
    
    if (_integrated_drift_plane != nullptr && std::holds_alternative<frequency_drift_plane>(*_integrated_drift_plane)) {
        auto &idp = std::get<frequency_drift_plane>(*_integrated_drift_plane);
        idp.set_device(_device);
        idp.push_device();
    }
}

frequency_drift_plane bliss::coarse_channel::integrated_drift_plane() {
    if (_integrated_drift_plane == nullptr) {
        throw std::runtime_error("integrated_drift_plane not set");
    }
    
    // Check if the drift plane needs to be computed.
    if (std::holds_alternative<std::function<frequency_drift_plane()>>(*_integrated_drift_plane)) {
        // 1. Execute the generator (runs integration kernels).
        auto calculated_plane = std::get<std::function<frequency_drift_plane()>>(*_integrated_drift_plane)();
        calculated_plane.set_device(_device);
        
        // 2. Cache the computed plane.
        *_integrated_drift_plane = calculated_plane;
        
        return calculated_plane;
    } else {
        auto &ddp = std::get<frequency_drift_plane>(*_integrated_drift_plane);
        // Ensure the plane is on the correct device.
        ddp.set_device(_device);
        return ddp;
    }
}

void bliss::coarse_channel::set_integrated_drift_plane(frequency_drift_plane integrated_plane) {
    _integrated_drift_plane =
            std::make_shared<std::variant<frequency_drift_plane, std::function<frequency_drift_plane()>>>(
                    integrated_plane);
}

void bliss::coarse_channel::set_integrated_drift_plane(
        std::function<frequency_drift_plane()> integrated_plane_generator) {
    _integrated_drift_plane =
            std::make_shared<std::variant<frequency_drift_plane, std::function<frequency_drift_plane()>>>(
                    integrated_plane_generator);
}

// --- GETTERS (Metadata Proxies) ---

double bliss::coarse_channel::fch1() const { return _meta.fch1; }
double bliss::coarse_channel::foff() const { return _meta.foff; }
int64_t bliss::coarse_channel::machine_id() const { return _meta.machine_id.value_or(0); }
int64_t bliss::coarse_channel::nbits() const { return _meta.nbits.value_or(0); }
int64_t bliss::coarse_channel::nchans() const { return _meta.nchans; }
int64_t bliss::coarse_channel::ntsteps() const { return _meta.ntsteps; }
int64_t bliss::coarse_channel::nifs() const { return _meta.nifs; }
std::string bliss::coarse_channel::source_name() const { return _meta.source_name; }
double bliss::coarse_channel::src_dej() const { return _meta.src_dej.value_or(0.0); }
double bliss::coarse_channel::src_raj() const { return _meta.src_raj.value_or(0.0); }
int64_t bliss::coarse_channel::telescope_id() const { return _meta.telescope_id.value_or(0); }
double bliss::coarse_channel::tsamp() const { return _meta.tsamp; }
double bliss::coarse_channel::tstart() const { return _meta.tstart; }
int64_t bliss::coarse_channel::data_type() const { return _meta.data_type; }
double bliss::coarse_channel::az_start() const { return _meta.az_start.value_or(0.0); }
double bliss::coarse_channel::za_start() const { return _meta.za_start.value_or(0.0); }