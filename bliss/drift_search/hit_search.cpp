#include "bland/ndarray.hpp"
#include <drift_search/connected_components.hpp>

#include <drift_search/protohit_search.hpp>
#include <drift_search/hit_search.hpp>
#include <drift_search/local_maxima.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <cstdint>
#include <cmath>

using namespace bliss;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions & Data Structures)
// ============================================================================
namespace {

    /**
     * @brief Encapsulates loop-invariant metadata to reduce parameter passing overhead.
     */
    struct hit_translation_context {
        float noise_floor;
        double foff;
        double fch1;
        double tstart_seconds;
        double tsamp;
        int64_t integration_length;
        int64_t cc_number;
        float channel_bandwidth_MHz;
    };

    /**
     * @brief Translates a raw protohit into a fully realized physical hit.
     * @details Applies physical parameters (frequency, time, drift rate) to the raw 
     * pixel coordinates found during the cluster search.
     * @param c The raw protohit detection.
     * @param drift_rate_info Metadata containing physical drift rates.
     * @param ctx The context struct containing loop-invariant channel metadata.
     * @return A populated hit structure.
     */
    hit translate_protohit_to_hit(const protohit& c,
                                  const std::vector<frequency_drift_plane::drift_rate>& drift_rate_info,
                                  const hit_translation_context& ctx) 
    {
        hit this_hit;
        
        // Map raw grid coordinates to the hit struct
        this_hit.rate_index       = c.index_max.drift_index;
        this_hit.start_freq_index = c.index_max.frequency_channel;
        
        // Implicitly copies all flags (sigma_clip, kurtosis, etc.) without needing redundant if-blocks
        this_hit.rfi_counts       = c.rfi_counts; 

        // Retrieve physical drift rate from pre-computed info
        this_hit.drift_rate_Hz_per_sec = drift_rate_info[this_hit.rate_index].drift_rate_Hz_per_sec;

        // Calculate Signal Power and SNR
        auto signal_power = (c.max_integration - ctx.noise_floor);
        this_hit.power = signal_power; // Unnormalized power
        this_hit.snr   = signal_power / c.desmeared_noise; // SNR accounting for desmearing

        // Bandwidth calculations
        this_hit.binwidth  = c.binwidth;
        this_hit.bandwidth = this_hit.binwidth * ctx.channel_bandwidth_MHz;

        // Recalculate start frequency based on the centroid (refinement)
        auto freq_offset        = ctx.foff * c.index_center.frequency_channel;
        this_hit.start_freq_MHz = ctx.fch1 + freq_offset;
        
        // Time parameters
        this_hit.start_time_sec = ctx.tstart_seconds; 
        this_hit.duration_sec   = ctx.tsamp * ctx.integration_length;
        
        // Integration metadata
        this_hit.integrated_channels = drift_rate_info[this_hit.rate_index].desmeared_bins * ctx.integration_length;
        this_hit.coarse_channel_number = ctx.cc_number;
        
        return this_hit;
    }
}

// ============================================================================
// HIT SEARCH IMPLEMENTATIONS
// ============================================================================

std::list<hit> bliss::hit_search(coarse_channel dedrifted_scan, hit_search_options options) {

    // Retrieve necessary data products
    auto noise_estimate  = dedrifted_scan.noise_estimate();
    auto dedrifted_plane = dedrifted_scan.integrated_drift_plane();

    // 1. Find local maxima/clusters in the drift plane (Raw Detections)
    auto protohits = protohit_search(dedrifted_plane, noise_estimate, options);

    auto drift_rate_info = dedrifted_plane.drift_rate_info();

    // OPTIMIZATION: Loop Invariant Code Motion (Packed into Context Struct)
    hit_translation_context ctx;
    ctx.foff = dedrifted_scan.foff();
    ctx.fch1 = dedrifted_scan.fch1();
    ctx.tstart_seconds = dedrifted_scan.tstart() * 24.0 * 60.0 * 60.0; // Pre-calc MJD -> Seconds
    ctx.tsamp = dedrifted_scan.tsamp();
    ctx.cc_number = dedrifted_scan._coarse_channel_number;
    ctx.channel_bandwidth_MHz = std::abs(1e6 * ctx.foff); 
    ctx.noise_floor = noise_estimate.noise_floor();
    ctx.integration_length = dedrifted_plane.integration_steps();

    std::list<hit> hits;
    
    // 2. Translate raw detections into physical hits
    for (const auto &c : protohits) {
        hits.push_back(translate_protohit_to_hit(c, drift_rate_info, ctx));
    }

    return hits;
}

// ============================================================================
// PIPELINE TRANSFORM WRAPPERS
// ============================================================================

scan bliss::hit_search(scan dedrifted_scan, hit_search_options options) {
    dedrifted_scan.add_coarse_channel_transform([options](coarse_channel cc) {
        auto hits = hit_search(cc, options);
        cc.set_hits(hits); // Attach results to the channel object
        return cc;
    });
    return dedrifted_scan;
}

observation_target bliss::hit_search(observation_target dedrifted_target, hit_search_options options) {
    for (auto &dedrifted_scan : dedrifted_target._scans) {
        dedrifted_scan = hit_search(dedrifted_scan, options);
    }
    return dedrifted_target;
}

cadence bliss::hit_search(cadence dedrifted_cadence, hit_search_options options) {
    for (auto &obs_target : dedrifted_cadence._observations) {
        obs_target = hit_search(obs_target, options);
    }
    return dedrifted_cadence;
}