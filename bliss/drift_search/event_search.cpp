#include <drift_search/event_search.hpp>

#include <core/cadence.hpp>

#include <fmt/format.h>
#include <cstdint>
#include <limits>
#include <optional>

using namespace bliss;

constexpr float seconds_per_day = 24 * 60 * 60;

// Tuning parameters for the distance metric
constexpr float freq_localization_weight = 0.01f;
constexpr float drift_error_weight       = 10.0f;
constexpr float snr_difference_weight    = 0.0f; // Currently ignored
constexpr float eps                      = 1e-8; // Small epsilon to prevent division by zero

/**
 * @brief Computes a "distance" metric between two hits to determine if they belong to the same signal.
 *
 * @details The metric projects both hits to a common "rendezvous time" (midpoint) based on their
 * drift rates. It calculates the mismatch in frequency at that time, along with differences
 * in drift rate and SNR.
 *
 * @param a The first hit.
 * @param b The second hit.
 * @return A float representing the dissimilarity (lower is better, 0 means perfect match).
 */
float distance_func(const hit &a, const hit &b) {
    auto snr_difference   = std::abs(a.snr - b.snr);
    auto power_difference = std::abs(a.power - b.power);
    
    // Normalize drift difference relative to the magnitude of the drift rates
    auto drift_difference = std::abs(a.drift_rate_Hz_per_sec - b.drift_rate_Hz_per_sec) /
                            (eps + a.drift_rate_Hz_per_sec * a.drift_rate_Hz_per_sec +
                             b.drift_rate_Hz_per_sec * b.drift_rate_Hz_per_sec);
    auto drift_error = drift_difference * drift_difference;

    auto first_sample_time = std::min(a.start_time_sec, b.start_time_sec);
    auto last_sample_time  = std::max(a.start_time_sec + b.duration_sec, b.start_time_sec + b.duration_sec);
    auto rendezvous_time   = (last_sample_time + first_sample_time) / 2;

    auto a_time_to_rendezvous = rendezvous_time - a.start_time_sec;
    auto b_time_to_rendezvous = rendezvous_time - b.start_time_sec;

    // Project frequencies to the rendezvous time using the linear drift model: f(t) = f0 + drift * t
    auto a_rendezvous_frequency = a.start_freq_MHz * 1e6 + a.drift_rate_Hz_per_sec * a_time_to_rendezvous;
    auto b_rendezvous_frequency = b.start_freq_MHz * 1e6 + b.drift_rate_Hz_per_sec * b_time_to_rendezvous;

    auto rendezvous_frequency_difference = std::abs(a_rendezvous_frequency - b_rendezvous_frequency);

    // Weighted sum of errors
    auto distance = freq_localization_weight * rendezvous_frequency_difference + drift_error_weight * drift_error +
                    snr_difference_weight * snr_difference;
    return distance;
}

/**
 * @brief Cache for the expensive distance function.
 * @details Uses a map to memoize distance calculations between pairs of hits, 
 * preventing redundant computations during the intensive search loop.
 */
struct hit_distance {
    struct hit_pair_comparator {
        bool operator()(const std::pair<hit, hit> &p1, const std::pair<hit, hit> &p2) const {
            if (p1.first == p2.first && p1.second == p2.second) {
                return false;
            }
            if (p1.first == p2.second && p1.second == p2.first) {
                return false;
            }
            return p1 < p2;
        }
    };

    std::map<std::pair<hit, hit>, float, hit_pair_comparator> distance_cache;

    float operator()(hit p1, hit p2) {
        auto pair = std::make_pair(p1, p2);
        if (distance_cache.find(pair) != distance_cache.end()) {
            return distance_cache.at(pair);
        } else {
            auto d           = distance_func(p1, p2);
            distance_cache[pair] = d;
            return d;
        }
    }
};

// ============================================================================
// HELPER FUNCTIONS (To reduce cognitive complexity in event_search)
// ============================================================================

/**
 * @brief Structure to hold the result of a hit matching operation.
 */
struct match_result {
    float distance;
    std::list<hit>::iterator hit_it;
};

/**
 * @brief Finds the hit in a subsequent scan that best matches the trajectory of a candidate event.
 * @param candidate_event The multi-scan event currently being built.
 * @param hits_to_check The list of hits in the target scan to compare against.
 * @param distance_calc The memoized distance calculator functor.
 * @return A match_result containing the best distance found and an iterator to that hit.
 */
match_result find_best_matching_hit(const event& candidate_event, std::list<hit>& hits_to_check, hit_distance& distance_calc) {
    float best_distance = std::numeric_limits<float>::max();
    auto best_it = hits_to_check.end();

    for (auto it = hits_to_check.begin(); it != hits_to_check.end(); ++it) {
        float lowest_dist_to_event = std::numeric_limits<float>::max();
        for (const auto& hit_in_event : candidate_event.hits) {
            float d = distance_calc(hit_in_event, *it);
            if (d < lowest_dist_to_event) {
                lowest_dist_to_event = d;
            }
        }
        if (lowest_dist_to_event < best_distance) {
            best_distance = lowest_dist_to_event;
            best_it = it;
        }
    }
    return {best_distance, best_it};
}

/**
 * @brief Counts how many times the candidate event's signature appears in OFF-target scans.
 * @details In SETI, if a signal is seen when the telescope is pointed away from the target (OFF scan),
 * it is highly likely to be local human-made Radio Frequency Interference (RFI) rather than ET.
 * @param candidate_event The event to verify.
 * @param off_scans List of background/OFF-target scans.
 * @param distance_calc The memoized distance calculator.
 * @return The number of matching hits found in OFF scans.
 */
int count_event_in_off_scans(const event& candidate_event, const std::vector<scan>& off_scans, hit_distance& distance_calc) {
    int times_in_off = 0;
    for (auto off_scan : off_scans) {
        for (auto off_hit : off_scan.hits()) {
            float dist_to_event_hits = 0;
            for (auto event_hit : candidate_event.hits) {
                dist_to_event_hits += distance_calc(off_hit, event_hit);
            }
            if (dist_to_event_hits / candidate_event.hits.size() < 50) {
                times_in_off += 1;
                fmt::print("INFO: Event was found in an off scan\n");
            }
        }
    }
    return times_in_off;
}

/**
 * @brief Computes and sets the average properties for a finalized event.
 * @param candidate_event The event to finalize (updated in-place).
 */
void finalize_event_averages(event& candidate_event) {
    candidate_event.average_drift_rate_Hz_per_sec = 0;
    candidate_event.average_power = 0;
    candidate_event.average_snr = 0;
    candidate_event.average_bandwidth = 0;
    
    for (auto &hit_in_event : candidate_event.hits) {
        candidate_event.average_drift_rate_Hz_per_sec += hit_in_event.drift_rate_Hz_per_sec;
        candidate_event.average_power += hit_in_event.power;
        candidate_event.average_snr += hit_in_event.snr;
        candidate_event.average_bandwidth += hit_in_event.bandwidth;
    }
    
    float num_hits = candidate_event.hits.size();
    candidate_event.average_drift_rate_Hz_per_sec /= num_hits;
    candidate_event.average_power /= num_hits;
    candidate_event.average_bandwidth /= num_hits;
    candidate_event.average_snr /= num_hits;
    
    fmt::print("INFO: Average SNR of this candidate event is {} and drift is {}\n",
               candidate_event.average_snr,
               candidate_event.average_drift_rate_Hz_per_sec);
}

/**
 * @brief Extracts all OFF-target scans from a cadence.
 * @param cadence_with_hits The full observation cadence.
 * @param on_target_index The index of the primary ON-target observation within the cadence.
 * @return A vector of scans where the telescope was pointing off-target.
 */
std::vector<scan> extract_off_scans(const cadence& cadence_with_hits, int64_t on_target_index) {
    std::vector<scan> off_scans;
    for (size_t ii = 0; ii < cadence_with_hits._observations.size(); ++ii) {
        if (ii != on_target_index) {
            for (auto &target_scan : cadence_with_hits._observations[ii]._scans) {
                off_scans.push_back(target_scan);
            }
        }
    }
    return off_scans;
}

/**
 * @brief Evaluates a single hit to see if it forms a valid multi-scan event.
 * @details Takes a "seed" hit from an ON scan and attempts to track its trajectory through 
 * subsequent ON scans. If it forms a track (matches hits in other scans) and is NOT 
 * found in any OFF scans, it is considered a valid ET candidate event.
 * @param starting_hit The seed hit to start tracking.
 * @param starting_scan The scan where the seed hit was found.
 * @param on_scan_index The index of the starting scan.
 * @param on_target_obs The full primary observation target containing all ON scans.
 * @param on_scan_hits A vector containing lists of available hits for each ON scan.
 * @param off_scans The extracted list of OFF scans for RFI rejection.
 * @param distance_calc Memoized distance calculator.
 * @return An optional event. Contains the event if valid, std::nullopt otherwise.
 */
std::optional<event> evaluate_candidate_event(
    const hit& starting_hit,
    const scan& starting_scan,
    size_t on_scan_index,
    const observation_target& on_target_obs,
    std::vector<std::list<hit>>& on_scan_hits,
    const std::vector<scan>& off_scans,
    hit_distance& distance_calc) 
{
    event candidate_event;
    candidate_event.hits.push_back(starting_hit);
    candidate_event.average_power               = starting_hit.power;
    candidate_event.average_snr                 = starting_hit.snr;
    candidate_event.average_drift_rate_Hz_per_sec = starting_hit.drift_rate_Hz_per_sec;
    candidate_event.event_start_seconds         = starting_scan.tstart() * seconds_per_day;
    candidate_event.starting_frequency_Hz       = starting_hit.start_freq_MHz * 1e6;
    candidate_event.event_end_seconds = starting_scan.tstart() * seconds_per_day + starting_scan.tduration_secs();

    // Look through hits in subsequent scans
    for (size_t matching_scan_index = on_scan_index + 1; matching_scan_index < on_target_obs._scans.size(); ++matching_scan_index) {
        auto &candidate_scan = on_target_obs._scans[matching_scan_index];

        auto hypothetical_start = std::min(candidate_event.event_start_seconds, seconds_per_day * candidate_scan.tstart());
        auto hypothetical_end = std::max(candidate_event.event_end_seconds,
                                         seconds_per_day * candidate_scan.tstart() + candidate_scan.tsamp());
        auto rendezvous_time = (hypothetical_start + hypothetical_end) / 2; 

        auto candidate_event_time_to_rendezvous = rendezvous_time - candidate_event.event_start_seconds;
        auto scan_time_to_rendezvous            = rendezvous_time - candidate_scan.tstart() * seconds_per_day;
        auto event_rendezvous_frequency = candidate_event.starting_frequency_Hz +
                                          candidate_event.average_drift_rate_Hz_per_sec * candidate_event_time_to_rendezvous;

        auto& hits_to_check = on_scan_hits[matching_scan_index];
        auto match = find_best_matching_hit(candidate_event, hits_to_check, distance_calc);
        
        // If a match is found within acceptable distance threshold
        if (match.distance < 50) {
            candidate_event.hits.push_back(*(match.hit_it));
            hits_to_check.erase(match.hit_it); // Remove hit so it isn't matched twice
        }
    }

    int times_event_in_off = count_event_in_off_scans(candidate_event, off_scans, distance_calc);
    
    // A valid event must appear in multiple ON scans and zero OFF scans
    if (candidate_event.hits.size() > 1 && times_event_in_off == 0) {
        finalize_event_averages(candidate_event);
        return candidate_event;
    }

    return std::nullopt;
}

// ============================================================================
// MAIN EVENT SEARCH FUNCTION (Fully Flattened)
// ============================================================================

/**
 * @brief Searches a cadence for persistent signals (events) across multiple scans.
 * * 
 * * @details This is the core multi-scan event detection algorithm. It iterates through
 * all hits in the primary target's scans, attempting to build trajectories across time
 * and frequency. It uses OFF-target scans as a control to reject terrestrial RFI.
 * The logic has been flattened into helper functions for improved readability.
 * * @param cadence_with_hits The cadence containing both ON and OFF observations with detected hits.
 * @return A vector of verified extraterrestrial candidate events.
 */
std::vector<event> bliss::event_search(cadence cadence_with_hits) {
    hit_distance       distance;
    std::vector<event> detected_events;

    const int64_t on_target_index = 0; // Assume first target is the primary
    auto          on_target_obs   = cadence_with_hits._observations[on_target_index];
    
    // Pre-load all hits from ON scans for fast access and erasure
    std::vector<std::list<hit>> on_scan_hits;
    for (auto &scan : on_target_obs._scans) {
        on_scan_hits.push_back(scan.hits());
    }

    // Extract background scans for RFI rejection
    std::vector<scan> off_scans = extract_off_scans(cadence_with_hits, on_target_index);

    // Clean iteration: check each hit in each scan to see if it seeds a valid event
    for (size_t on_scan_index = 0; on_scan_index < on_target_obs._scans.size(); ++on_scan_index) {
        auto starting_scan = on_target_obs._scans[on_scan_index];
        
        for (const auto &starting_hit : on_scan_hits[on_scan_index]) {
            std::optional<event> candidate = evaluate_candidate_event(
                starting_hit, starting_scan, on_scan_index, 
                on_target_obs, on_scan_hits, off_scans, distance
            );
            
            if (candidate.has_value()) {
                detected_events.emplace_back(std::move(candidate.value()));
            }
        }
    }

    return detected_events;
}