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

std::list<hit> bliss::hit_search(coarse_channel dedrifted_scan, hit_search_options options) {

    // Retrieve necessary data products
    auto noise_estimate  = dedrifted_scan.noise_estimate();
    auto dedrifted_plane = dedrifted_scan.integrated_drift_plane();

    // 1. Find local maxima/clusters in the drift plane (Raw Detections)
    auto protohits = protohit_search(dedrifted_plane, noise_estimate, options);

    auto integration_length = dedrifted_plane.integration_steps();
    auto drift_rate_info    = dedrifted_plane.drift_rate_info();

    // OPTIMIZATION: Loop Invariant Code Motion
    // Extract constant metadata once to avoid redundant getter calls inside the loop.
    const double foff = dedrifted_scan.foff();
    const double fch1 = dedrifted_scan.fch1();
    const double tstart_seconds = dedrifted_scan.tstart() * 24.0 * 60.0 * 60.0; // Pre-calc MJD -> Seconds
    const double tsamp = dedrifted_scan.tsamp();
    const int64_t cc_number = dedrifted_scan._coarse_channel_number;
    
    // Pre-calculate channel bandwidth in MHz
    const float channel_bandwidth_MHz = std::abs(1e6 * foff); 

    std::list<hit> hits;
    for (const auto &c : protohits) {
        hit this_hit;
        
        // Map raw grid coordinates to the hit struct
        this_hit.rate_index       = c.index_max.drift_index;
        this_hit.rfi_counts       = c.rfi_counts;
        this_hit.start_freq_index = c.index_max.frequency_channel;

        // Calculate Start Frequency (MHz)
        auto freq_offset        = foff * this_hit.start_freq_index;
        this_hit.start_freq_MHz = fch1 + freq_offset;

        // Retrieve physical drift rate from pre-computed info
        this_hit.drift_rate_Hz_per_sec = drift_rate_info[this_hit.rate_index].drift_rate_Hz_per_sec;

        // Calculate Signal Power and SNR
        auto signal_power = (c.max_integration - noise_estimate.noise_floor());
        this_hit.power = signal_power; // Unnormalized power
        this_hit.snr   = signal_power / c.desmeared_noise; // SNR accounting for desmearing

        // Bandwidth calculations
        this_hit.binwidth  = c.binwidth;
        this_hit.bandwidth = this_hit.binwidth * channel_bandwidth_MHz;

        // Recalculate start frequency based on the centroid (optional refinement)
        freq_offset             = foff * c.index_center.frequency_channel;
        this_hit.start_freq_MHz = fch1 + freq_offset;
        
        // Time parameters
        this_hit.start_time_sec = tstart_seconds; 
        this_hit.duration_sec   = tsamp * integration_length;
        
        // Integration metadata
        this_hit.integrated_channels = drift_rate_info[this_hit.rate_index].desmeared_bins * integration_length;
        this_hit.coarse_channel_number = cc_number;
        
        // Populate RFI flags if present in the protohit
        if (c.rfi_counts.find(flag_values::sigma_clip) != c.rfi_counts.end()) {
             this_hit.rfi_counts[flag_values::sigma_clip] = c.rfi_counts.at(flag_values::sigma_clip);
        }
        if (c.rfi_counts.find(flag_values::low_spectral_kurtosis) != c.rfi_counts.end()) {
             this_hit.rfi_counts[flag_values::low_spectral_kurtosis] = c.rfi_counts.at(flag_values::low_spectral_kurtosis);
        }
        if (c.rfi_counts.find(flag_values::high_spectral_kurtosis) != c.rfi_counts.end()) {
             this_hit.rfi_counts[flag_values::high_spectral_kurtosis] = c.rfi_counts.at(flag_values::high_spectral_kurtosis);
        }
        
        hits.push_back(this_hit);
    }

    return hits;
}

// Wrapper implementations for high-level transforms
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