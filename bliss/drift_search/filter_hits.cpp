#include <drift_search/filter_hits.hpp>
#include <core/flag_values.hpp>

#include <fmt/format.h>

#include <stdexcept>
#include <cmath>

using namespace bliss;

// Epsilon value used for safe floating-point comparison when checking for zero drift
constexpr double eps = 1e-6;

/**
 * @brief Filters a list of hits based on the specified criteria.
 * @details Evaluates each hit against a set of rules (e.g., zero drift, RFI flagging percentages).
 * This implementation uses `std::list::remove_if` for efficient in-place filtering.
 * @param hits The initial list of detected hits.
 * @param options The filtering parameters and thresholds.
 * @return The filtered list of hits.
 */
std::list<hit> bliss::filter_hits(std::list<hit> hits, filter_options options) {
    
    // Use the standard remove_if algorithm with a lambda function.
    // The lambda returns 'true' if the hit matches any rejection criteria and should be removed.
    hits.remove_if([&options](hit& current_hit) {
        
        // Filter: Zero Drift (Stationary signals are usually terrestrial RFI)
        // Uses an epsilon comparison to safely handle floating-point inaccuracies.
        if (options.filter_zero_drift && std::fabs(current_hit.drift_rate_Hz_per_sec) < eps) {
            return true;
        }
        
        // Optimization: calculate the absolute value of integrated channels once per hit
        auto abs_integrated = std::fabs(current_hit.integrated_channels);
        
        // Filter: Sigma Clip (Impulsive/Transient RFI)
        // Rejects hits that don't have enough data points passing the sigma clip threshold.
        if (options.filter_sigmaclip && 
            current_hit.rfi_counts[flag_values::sigma_clip] < abs_integrated * options.minimum_percent_sigmaclip) {
            return true;
        }
        
        // Filter: High Spectral Kurtosis (Non-Gaussian signals)
        // Rejects hits lacking enough data points with expected Gaussian noise statistics.
        if (options.filter_high_sk && 
            current_hit.rfi_counts[flag_values::high_spectral_kurtosis] < abs_integrated * options.minimum_percent_high_sk) {
            return true;
        }
        
        // Filter: Low Spectral Kurtosis (Often artificial/engineered signals or specific RFI)
        // Rejects hits where too many data points fall below the low SK threshold.
        if (options.filter_low_sk && 
            current_hit.rfi_counts[flag_values::low_spectral_kurtosis] > abs_integrated * options.maximum_percent_low_sk) {
            return true;
        }
        
        // If the hit passes all filters, do not remove it (return false)
        return false;
    });

    return hits;
}

/**
 * @brief Filters hits associated with a specific coarse channel.
 * @param cc_with_hits The coarse channel containing the hits.
 * @param options Filtering configuration.
 * @return A new coarse channel object with the filtered hit list.
 * @throws std::invalid_argument if the channel does not have any hits calculated yet.
 */
coarse_channel bliss::filter_hits(coarse_channel cc_with_hits, filter_options options) {
    if (cc_with_hits.has_hits()) {
        auto original_hits = cc_with_hits.hits();
        auto filtered_hits = filter_hits(original_hits, options);
        cc_with_hits.set_hits(filtered_hits);
        return cc_with_hits;
    } else {
        throw std::invalid_argument("coarse channel has no hits");
    }
}

/**
 * @brief Schedules hit filtering for all channels in a scan.
 * @details Appends the filter operation to the scan's lazy processing pipeline.
 * @param scan_with_hits The scan object.
 * @param options Filtering configuration.
 * @return A scan object with the filtering transform registered.
 */
scan bliss::filter_hits(scan scan_with_hits, filter_options options) {
    scan_with_hits.add_coarse_channel_transform([options](coarse_channel cc) {
        return filter_hits(cc, options);
    });

    return scan_with_hits;
}

/**
 * @brief Filters hits for all scans within an observation target.
 * @param observation_with_hits The observation target containing multiple scans.
 * @param options Filtering configuration.
 * @return The updated observation target.
 */
observation_target bliss::filter_hits(observation_target observation_with_hits, filter_options options) {
    for (auto &scan : observation_with_hits._scans) {
        scan = filter_hits(scan, options);
    }
    return observation_with_hits;
}

/**
 * @brief Filters hits for an entire cadence (multiple observation targets).
 * @param cadence_with_hits The cadence containing observations.
 * @param options Filtering configuration.
 * @return The updated cadence.
 */
cadence bliss::filter_hits(cadence cadence_with_hits, filter_options options) {
    for (auto &obs_target : cadence_with_hits._observations) {
        obs_target = filter_hits(obs_target, options);
    }
    return cadence_with_hits;
}