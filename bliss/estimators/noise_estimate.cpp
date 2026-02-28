#include "estimators/noise_estimate.hpp"
#include <bland/ops/ops.hpp>
#include <bland/ops/ops_statistical.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

using namespace bliss;

// ============================================================================
// STATISTICAL ESTIMATOR IMPLEMENTATIONS
// ============================================================================
namespace detail {

    /**
     * @brief Computes noise statistics using standard mean and variance.
     * @details This approach is sensitive to outliers if the data is not masked.
     * @param x The input data tensor.
     * @return A noise_stats object containing the computed floor and power.
     */
    noise_stats noise_power_estimate_stddev(bland::ndarray x) {
        noise_stats estimated_stats;
        auto mean_std = bland::mean_stddev(x);
        estimated_stats.set_noise_floor(mean_std.first);
        estimated_stats.set_noise_power(mean_std.second);

        return estimated_stats;
    }

    /**
     * @brief Computes noise statistics using masked mean and variance.
     * @details Calculates statistics exclusively on samples where mask == 0 (unflagged).
     * @param x The input data tensor.
     * @param mask The boolean flag mask tensor.
     * @return A noise_stats object containing the computed floor and power.
     */
    noise_stats noise_power_estimate_stddev(bland::ndarray x, bland::ndarray mask) {
        noise_stats estimated_stats;
        auto mean_std = bland::masked_mean_stddev(x, mask);
        estimated_stats.set_noise_floor(mean_std.first);
        estimated_stats.set_noise_power(mean_std.second);

        return estimated_stats;
    }

    /**
     * @brief Computes noise statistics using Median and Median Absolute Deviation (MAD).
     * @details This approach is robust against outliers.
     * floor = median(x)
     * power = median(|x - median|)
     * @param x The input data tensor.
     * @return A noise_stats object containing the computed floor and power.
     */
    noise_stats noise_power_estimate_mad(const bland::ndarray &x) {
        noise_stats estimated_stats;

        // TODO: do medians right (TM)
        // // bland needs some TLC to do this Right (TM)
        // estimated_stats._noise_floor = bland::median(x);
        // // median absolute deviation is median(|Xi - median|)
        // auto median_absolute_deviation = bland::median(bland::abs(x - estimated_stats._noise_floor));
        // estimated_stats._noise_power   = median_absolute_deviation;

        return estimated_stats;
    }

    /**
     * @brief Computes masked noise statistics using Median and MAD.
     * @details Currently throws a runtime_error as the implementation is pending.
     * @param x The input data tensor.
     * @param mask The boolean flag mask tensor.
     * @return A noise_stats object.
     * @throws std::runtime_error Always throws as the feature is not implemented.
     */
    noise_stats noise_power_estimate_mad(const bland::ndarray &x, const bland::ndarray &mask) {

        throw std::runtime_error("masked noise power estimation with mad is not implemented yet");
        noise_stats estimated_stats;

        // // bland needs some TLC to do this Right (TM) and until then
        // // it's pretty hard to do the masked_median
        // estimated_stats._noise_floor = bland::median(x);
        // // median absolute deviation is median(|Xi - median|)
        // auto stddev                  = bland::median(bland::abs(x - estimated_stats._noise_floor));
        // estimated_stats._noise_power = std::pow(stddev, 2);

        return estimated_stats;
    }

} // namespace detail

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Routers)
// ============================================================================
namespace {

    /**
     * @brief Validates and corrects a flag mask before statistical estimation.
     * @details Detects if a mask is completely saturated (100% flagged), making noise
     * estimation mathematically impossible. 
     * @param mask The input mask tensor.
     * @return The validated mask tensor.
     * @throws std::runtime_error if all samples are masked.
     */
    bland::ndarray correct_mask(const bland::ndarray &mask) {
        auto unmasked_samples = bland::count_true(bland::ndarray(mask) == 0);
        if (unmasked_samples == 0) {
            // TODO: issue warning & "correct" the mask in some intelligent way
            auto err = fmt::format("correct_mask: the mask is completely flagged, so a flagged noise estimate is not possible.");
            /*
            * This is a pretty strange condition where the entire scan is flagged which makes estimating noise using
            * unflagged samples pretty awkward... There's not an obviously right way to handle this and anyone caring
            * about this pipeline output should probably be made aware of it, but it's also not fatal.
            * Known instances of this condition occurring:
            * * Voyager 2020 data from GBT experiences a sudden increase in noise floor/power of ~ 3dB in the B target scan
            * which generates high spectral kurtosis across the entire band
            * filename w/in BL: single_coarse_guppi_59046_80354_DIAG_VOYAGER-1_0012.rawspec.0000.h5
            */
            // TODO: we can attempt to correct this, since it's only been known to occur based on SK with high thresholds of
            // ~5 that threshold can be increased. Some ideas:
            // * have an "auto" mode for SK that starts with threshold of 5 and iteratively increases until the whole
            // channel
            //   is not falled
            // * ignore high SK here (or try to identify what flag is causing issues and ignore it)
            // * warn earlier in flagging step
            throw std::runtime_error(err);
        }

        return mask;
    }

    /**
     * @brief Routes the estimation execution for masked data.
     * @param data The input data tensor.
     * @param mask The boolean flag mask tensor.
     * @param method The selected statistical estimator method.
     * @return The computed noise statistics.
     */
    noise_stats route_masked_estimation(const bland::ndarray& data, const bland::ndarray& mask, noise_power_estimator method) {
        switch (method) {
            case noise_power_estimator::MEAN_ABSOLUTE_DEVIATION:
                return detail::noise_power_estimate_mad(data, mask);
            case noise_power_estimator::STDDEV:
            default:
                return detail::noise_power_estimate_stddev(data, mask);
        }
    }

    /**
     * @brief Routes the estimation execution for unmasked data.
     * @param data The input data tensor.
     * @param method The selected statistical estimator method.
     * @return The computed noise statistics.
     */
    noise_stats route_unmasked_estimation(const bland::ndarray& data, noise_power_estimator method) {
        switch (method) {
            case noise_power_estimator::MEAN_ABSOLUTE_DEVIATION:
                return detail::noise_power_estimate_mad(data);
            case noise_power_estimator::STDDEV:
            default:
                return detail::noise_power_estimate_stddev(data);
        }
    }
}

// ============================================================================
// PUBLIC API IMPLEMENTATIONS
// ============================================================================

noise_stats bliss::estimate_noise_power(bland::ndarray x, noise_power_estimate_options options) {
    if (options.masked_estimate) {
        fmt::print("WARN: Requested a masked noise estimate, but calling without a mask. This may be an error in the future.\n");
    }
    return route_unmasked_estimation(x, options.estimator_method);
}

noise_stats bliss::estimate_noise_power(coarse_channel cc_data, noise_power_estimate_options options) {
    if (options.masked_estimate) {
        auto mask = cc_data.mask();
        cc_data.set_mask(correct_mask(mask));
        return route_masked_estimation(cc_data.data(), mask, options.estimator_method);
    } else {
        return route_unmasked_estimation(cc_data.data(), options.estimator_method);
    }
}

scan bliss::estimate_noise_power(scan sc, noise_power_estimate_options options) {
    sc.add_coarse_channel_transform([options](coarse_channel cc) {
        auto noise_stats = estimate_noise_power(cc, options);
        cc.set_noise_estimate(noise_stats);
        return cc;
    });

    return sc;
}

observation_target bliss::estimate_noise_power(observation_target observations, noise_power_estimate_options options) {
    for (auto &target_scan : observations._scans) {
        target_scan = estimate_noise_power(target_scan, options);
    }
    return observations;
}

cadence bliss::estimate_noise_power(cadence observations, noise_power_estimate_options options) {
    for (auto &obs_target : observations._observations) {
        obs_target = estimate_noise_power(obs_target, options);
    }
    return observations;
}