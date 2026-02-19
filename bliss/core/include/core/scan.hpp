#pragma once

#include "coarse_channel.hpp"
#include "scan_datasource.hpp"
#include "scan_metadata.hpp"

#include <bland/bland.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <memory>

namespace bliss {

/**
 * @brief Represents a full SETI observation or a loaded scan.
 *
 * The `scan` class is the high-level coordinator of the BLISS pipeline.
 * It manages the connection to the data source (file or stream), handles metadata,
 * and orchestrates the creation and processing of `coarse_channel` objects.
 *
 * @details
 * - **Abstraction:** It hides the details of file I/O behind the `IScanDataSource` interface.
 * - **Slicing:** It supports virtual slicing of the bandwidth via `slice_scan_channels`.
 * - **Pipeline:** It allows defining a sequence of transforms (filters) applied to every channel.
 * - **Device Management:** Acts as the primary interface for setting the compute device (CPU/CUDA)
 * for the entire observation, propagating this setting to all child channels.
 */
class scan {
  public:
    /// @brief Default constructor. Creates an empty scan.
    scan() = default;

    /// @brief Constructs a scan from an existing map of coarse channels.
    /// @param coarse_channels A map where key is the channel index and value is the channel object.
    scan(std::map<int, std::shared_ptr<coarse_channel>> coarse_channels);

    /// @brief Primary Constructor.
    /// @details Initializes the scan by connecting to a data source.
    /// @param data_source Shared pointer to a data source (e.g., H5 file reader).
    /// @param num_fine_channels_per_coarse Number of fine channels per coarse channel. 
    ///        If 0, it attempts to infer the channelization scheme automatically.
    scan(std::shared_ptr<IScanDataSource> data_source, int num_fine_channels_per_coarse=0);

    /// @brief Retrieves (and loads if necessary) a specific coarse channel.
    /// @details This method triggers the reading of data from disk for the requested channel.
    /// The loaded channel is cached in memory.
    /// @param coarse_channel_index The 0-based index of the coarse channel to read.
    /// @return A shared pointer to the `coarse_channel` object.
    /// @throws std::out_of_range if index is invalid.
    std::shared_ptr<coarse_channel> read_coarse_channel(int coarse_channel_index = 0);

    /// @brief Peeks at a coarse channel if it's already in memory.
    /// @return Shared pointer to the channel if loaded, `nullptr` otherwise.
    std::shared_ptr<coarse_channel> peak_coarse_channel(int coarse_channel_index = 0);

    /// @brief Adds a processing step to the channel pipeline.
    /// @details The transform function will be applied to every coarse channel *upon loading*.
    /// This allows building a lazy processing chain (e.g., normalize -> excise_dc -> flag).
    /// @param transform A function taking a coarse_channel and returning a modified one.
    /// @param description Optional string describing the transform (for logging).
    void add_coarse_channel_transform(std::function<coarse_channel(coarse_channel)> transform, std::string description="");

    /// @brief Calculates which coarse channel contains a specific frequency.
    /// @param frequency The frequency in MHz.
    /// @return The index of the coarse channel.
    int get_coarse_channel_with_frequency(double frequency) const;

    /// @brief Returns the total number of coarse channels in this scan.
    int get_number_coarse_channels() const;

    /// @brief Returns the path of the underlying data file (if applicable).
    std::string get_file_path() const;

    /// @brief Collects all hits from all loaded channels.
    /// @details Iterates over all cached channels, triggering processing if necessary, and aggregates detected hits.
    /// @return A list of all detected hits.
    std::list<hit> hits();

    /// @brief Computes the range of drift rates found across all channels.
    /// @return A pair {min_drift, max_drift} in Hz/s.
    std::pair<float, float> get_drift_range();

    /// @brief Gets the current compute device.
    bland::ndarray::dev device();

    /// @brief Sets the compute device for the scan and all its channels.
    /// @details Propagates the device setting to all currently loaded coarse channels.
    /// Future loaded channels will also use this device.
    /// @param device The target device (e.g., `bland::ndarray::dev::cuda`).
    /// @param verbose If true, prints device info.
    void set_device(bland::ndarray::dev &device, bool verbose=true);

    /// @brief Sets the compute device by name string (e.g., "cuda:0", "cpu").
    void set_device(std::string_view device, bool verbose=true);

    /// @brief Forces data migration to the configured device for all loaded channels.
    /// @details Useful to ensure data is on GPU before starting a heavy compute kernel.
    void push_device();

    /// @brief Creates a new `scan` object representing a subset of channels.
    /// @details This is a "virtual slice"; it shares the underlying data source but restricts the view.
    /// Useful for distributed processing (e.g., assigning different channel ranges to different nodes).
    /// @param start_channel The starting coarse channel index.
    /// @param count The number of channels to include (-1 for "until end").
    /// @return A new `scan` instance.
    scan slice_scan_channels(int64_t start_channel = 0, int64_t count = 1);

    // --- GETTERS & SETTERS (Proxies to internal `scan_metadata`) ---
    // These methods provide direct access to the metadata fields stored in the underlying `scan_metadata` struct.
    
    double      fch1() const;
    void        set_fch1(double);
    double      foff() const;
    void        set_foff(double);
    int64_t     machine_id() const;
    void        set_machine_id(int64_t);
    int64_t     nbits() const;
    void        set_nbits(int64_t);
    int64_t     nchans() const;
    void        set_nchans(int64_t);
    int64_t     nifs() const;
    void        set_nifs(int64_t);
    std::string source_name() const;
    void        set_source_name(std::string);
    double      src_dej() const;
    void        set_src_dej(double);
    double      src_raj() const;
    void        set_src_raj(double);
    int64_t     telescope_id() const;
    void        set_telescope_id(int64_t);
    double      tsamp() const;
    void        set_tsamp(double);
    double      tstart() const;
    void        set_tstart(double);
    int64_t     data_type() const;
    void        set_data_type(int64_t);
    double      az_start() const;
    void        set_az_start(double);
    double      za_start() const;
    void        set_za_start(double);
    int64_t     ntsteps() const;
    double      tduration_secs() const;

    /// @brief Defines a processing stage in the pipeline.
    struct transform_stage {
      std::string description;
      std::function<coarse_channel(coarse_channel)> transform;
    };

  protected:
    // Cache of loaded coarse channels.
    std::map<int, std::shared_ptr<coarse_channel>> _coarse_channels;
    
    // Abstract source for reading data.
    std::shared_ptr<IScanDataSource> _data_source = nullptr;
    
    // Ordered list of transforms to apply to new channels.
    std::vector<transform_stage> _coarse_channel_pipeline;

    // Unified metadata storage.
    scan_metadata _meta;

    // Derived values
    int _fine_channels_per_coarse;
    int64_t _num_coarse_channels;
    int64_t _coarse_channel_offset = 0;
    double  _tduration_secs;

    bland::ndarray::dev _device = bland::ndarray::dev::cpu;
};

} // namespace bliss