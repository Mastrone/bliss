#pragma once

#include <bland/bland.hpp>
#include "scan_metadata.hpp"
#include "frequency_drift_plane.hpp"
#include "hit.hpp"
#include "integrate_drifts_options.hpp"
#include "noise_power.hpp"

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace bliss {

/**
 * @brief Represents a single frequency sub-band (coarse channel) derived from a larger scan.
 *
 * The `coarse_channel` is the primary unit of processing/parallelism in the BLISS pipeline.
 * It encapsulates the spectral data (tensor), associated metadata, and the results of
 * detection algorithms (drift planes and hits).
 *
 * @details **Lazy Evaluation & Caching Architecture**
 * This class is designed to handle high-bandwidth data efficiently by deferring
 * I/O and heavy computation:
 * - **Lazy Loading:** Data and masks can be initialized via generator functions (std::function),
 * loading from disk/network into memory only when `data()` is first accessed.
 * - **Memoization:** Computationally expensive results (like the `frequency_drift_plane` from dedoppler)
 * are stored internally using `std::variant`. They are computed once upon the first request 
 * (or via a provided generator) and then cached for subsequent access.
 * - **Device Agnostic:** Uses `bland::ndarray` to hold data, allowing seamless migration between
 * CPU and GPU (CUDA) memory spaces via `set_device()` / `push_device()`.
 */
struct coarse_channel {

    /// @brief Constructs a channel container with metadata only (no data loaded).
    /// @param metadata The structural metadata describing this channel's frequency, time, and source.
    /// @param coarse_channel_number The logical index of this channel within the original scan.
    coarse_channel(scan_metadata metadata, int64_t coarse_channel_number=0);

    /// @brief Constructs a channel with lazy data generators.
    /// @details The actual data tensors are not loaded into memory until explicitly requested via `data()`.
    /// @param data A function that returns the spectral data tensor (Time x Frequency) when called.
    /// @param mask A function that returns the RFI mask tensor when called.
    /// @param metadata The structural metadata.
    /// @param coarse_channel_number The logical index of this channel.
    coarse_channel(std::function<bland::ndarray()> data,
                   std::function<bland::ndarray()> mask,
                   scan_metadata metadata,
                   int64_t coarse_channel_number=0);

    /// @brief Constructs a channel with existing (already loaded) data tensors.
    /// @param data The spectral data tensor (Time x Frequency).
    /// @param mask The RFI mask tensor.
    /// @param metadata The structural metadata.
    /// @param coarse_channel_number The logical index of this channel.
    coarse_channel(bland::ndarray data,
                   bland::ndarray mask,
                   scan_metadata metadata,
                   int64_t coarse_channel_number=0);

    /// @brief Accesses the spectral data tensor.
    /// @details **Lazy Load Trigger:** If the data was initialized with a generator, this call
    /// executes it to load the data. Also ensures the data resides on the configured `device()`.
    /// @return A reference to the underlying `bland::ndarray`.
    bland::ndarray data();

    /// @brief Updates the spectral data tensor (e.g., after normalization).
    /// @param new_mask The new data tensor.
    void set_data(bland::ndarray new_mask);

    /// @brief Accesses the RFI mask tensor.
    /// @details Triggers lazy loading and device migration if necessary.
    /// @return A reference to the underlying `bland::ndarray`.
    bland::ndarray mask();

    /// @brief Updates the RFI mask tensor (e.g., after flagging).
    /// @param new_mask The new mask tensor.
    void set_mask(bland::ndarray new_mask);

    /// @brief Retrieves the Time-Frequency-Drift plane (De-doppler result).
    /// @details **Blocking/Compute Trigger:** If the plane is not cached, this method executes
    /// the integration kernel (typically `integrate_drifts`). Subsequent calls return the cached result.
    /// @return The `frequency_drift_plane` containing integrated spectra.
    frequency_drift_plane integrated_drift_plane();

    /// @brief Sets a pre-computed drift plane, bypassing internal calculation.
    void set_integrated_drift_plane(frequency_drift_plane integrated_plane);

    /// @brief Sets a generator function for the drift plane (Lazy computation).
    /// @details Allows deferring the heavy de-doppler step until the result is actually needed.
    /// @param integrated_plane_generator Function that produces the drift plane when called.
    void set_integrated_drift_plane(std::function<frequency_drift_plane()> integrated_plane_generator);

    /// @brief Retrieves noise statistics used for normalization/SNR calculation.
    /// @throws std::logic_error If the noise stats have not been computed/set.
    noise_stats noise_estimate() const;

    /// @brief Sets the noise statistics.
    void set_noise_estimate(noise_stats estimate);

    /// @brief Checks if hits exist in memory without triggering detection.
    /// @return `true` if hits are available (computed), `false` otherwise.
    bool has_hits();

    /// @brief Retrieves the list of detected signals (Hits).
    /// @details **Blocking/Compute Trigger:** Executes the hit search algorithm if hits are not yet cached.
    /// @return A list of `hit` objects.
    std::list<hit> hits() const;

    /// @brief Sets a concrete list of hits (e.g., after filtering).
    void set_hits(std::list<hit> new_hits);

    /// @brief Sets a generator function for hits (Lazy computation).
    /// @param find_hits_func Function that runs the hit search when called.
    void set_hits(std::function<std::list<hit>()> find_hits_func);

    /// @brief Gets the current compute device (CPU/CUDA) assigned to this channel.
    bland::ndarray::dev device();

    /// @brief Sets the target compute device.
    /// @details This sets the *intent*. Actual memory transfer occurs lazily upon
    /// the next data access (`data()`, `mask()`) or explicitly via `push_device()`.
    /// @param device The target device (e.g., `bland::ndarray::dev::cuda`).
    void set_device(bland::ndarray::dev &device);

    /// @brief Sets the target compute device by name string.
    /// @param device Device string (e.g., "cuda:0", "cpu").
    void set_device(std::string_view device);

    /// @brief Forces immediate memory transfer to the configured device.
    /// @details Ensures `data`, `mask`, and `integrated_drift_plane` tensors reside physically on the target device.
    void push_device();

    // Proxy methods to access underlying metadata fields
    double fch1() const;
    double foff() const;
    int64_t machine_id() const;
    int64_t nbits() const;
    int64_t nchans() const;
    int64_t ntsteps() const;
    int64_t nifs() const;
    std::string source_name() const;
    double src_dej() const;
    double src_raj() const;
    int64_t telescope_id() const;
    double tsamp() const;
    double tstart() const;
    int64_t data_type() const;
    double az_start() const;
    double za_start() const;

    int64_t _coarse_channel_number; 
    
    // Core metadata structure containing scan parameters
    scan_metadata _meta;

    bland::ndarray _data;
    bland::ndarray _mask;

    std::optional<noise_stats> _noise_stats;

    // Variant storage for Lazy Evaluation:
    // Holds either the computed result (T) or the function to compute it (std::function<T()>).
    std::shared_ptr<std::variant<frequency_drift_plane, std::function<frequency_drift_plane()>>>
            _integrated_drift_plane = nullptr;

    std::shared_ptr<std::variant<std::list<hit>, std::function<std::list<hit>()>>> _hits = nullptr;

    bland::ndarray::dev _device = bland::ndarray::dev::cpu;
};

} // namespace bliss