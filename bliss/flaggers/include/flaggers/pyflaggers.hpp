#pragma once

#include "filter_rolloff.hpp"
#include "magnitude.hpp"
#include "sigmaclip.hpp"
#include "spectral_kurtosis.hpp"

#include <core/flag_values.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <iostream>
#include <tuple>

namespace nb = nanobind;
using namespace nb::literals;

// ============================================================================
// ANONYMOUS NAMESPACE (Helper Functions for Binding Grouping)
// ============================================================================
namespace {

    /**
     * @brief Binds the Spectral Kurtosis flagging functions to the Python module.
     * @param m The target nanobind module.
     */
    void bind_spectral_kurtosis(nb::module_& m) {
        m.def("flag_spectral_kurtosis",
              nb::overload_cast<bliss::coarse_channel, float, float, float>(&bliss::flag_spectral_kurtosis),
              "cc_data"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0f,
              "Flag non-gaussian samples in a channel using Spectral Kurtosis.");

        m.def("flag_spectral_kurtosis",
              nb::overload_cast<bliss::scan, float, float, float>(&bliss::flag_spectral_kurtosis),
              "scan"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0f,
              "Flag non-gaussian samples in a scan.");

        m.def("flag_spectral_kurtosis",
              nb::overload_cast<bliss::observation_target, float, float, float>(&bliss::flag_spectral_kurtosis),
              "observation"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0f);

        m.def("flag_spectral_kurtosis",
              nb::overload_cast<bliss::cadence, float, float, float>(&bliss::flag_spectral_kurtosis),
              "cadence"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a=2.0f);
    }

    /**
     * @brief Binds the Filter Rolloff flagging functions to the Python module.
     * @param m The target nanobind module.
     */
    void bind_filter_rolloff(nb::module_& m) {
        m.def("flag_filter_rolloff",
              nb::overload_cast<bliss::coarse_channel, float>(&bliss::flag_filter_rolloff),
              "cc_data"_a, "rolloff_width"_a,
              "Flag band edges (percentage width).");

        m.def("flag_filter_rolloff",
              nb::overload_cast<bliss::scan, float>(&bliss::flag_filter_rolloff),
              "scan"_a, "rolloff_width"_a);

        m.def("flag_filter_rolloff",
              nb::overload_cast<bliss::observation_target, float>(&bliss::flag_filter_rolloff),
              "observation"_a, "rolloff_width"_a);

        m.def("flag_filter_rolloff",
              nb::overload_cast<bliss::cadence, float>(&bliss::flag_filter_rolloff),
              "cadence"_a, "rolloff_width"_a);
    }

    /**
     * @brief Binds the Magnitude flagging functions to the Python module.
     * @param m The target nanobind module.
     */
    void bind_magnitude(nb::module_& m) {
        m.def("flag_magnitude",
              nb::overload_cast<const bland::ndarray &, float>(&bliss::flag_magnitude),
              "scan"_a, "threshold"_a,
              "Flag samples exceeding absolute magnitude threshold.");

        m.def("flag_magnitude",
              nb::overload_cast<bliss::coarse_channel>(&bliss::flag_magnitude),
              "cc_data"_a,
              "Flag using auto-calculated threshold (mean + 10*std).");

        m.def("flag_magnitude",
              nb::overload_cast<bliss::coarse_channel, float>(&bliss::flag_magnitude),
              "cc_data"_a, "threshold"_a);

        m.def("flag_magnitude",
              nb::overload_cast<bliss::scan>(&bliss::flag_magnitude),
              "scan"_a);

        m.def("flag_magnitude",
              nb::overload_cast<bliss::observation_target>(&bliss::flag_magnitude),
              "observation"_a);

        m.def("flag_magnitude",
              nb::overload_cast<bliss::cadence>(&bliss::flag_magnitude),
              "cadence"_a);
    }

    /**
     * @brief Binds the Sigma Clipping flagging functions to the Python module.
     * @param m The target nanobind module.
     */
    void bind_sigmaclip(nb::module_& m) {
        m.def("flag_sigmaclip",
              nb::overload_cast<const bland::ndarray&, int, float, float>(&bliss::flag_sigmaclip),
              "x"_a, "max_iter"_a=3, "lower_threshold"_a=4.0f, "upper_threshold"_a=4.0f,
              "Flag outliers using iterative sigma clipping.");

        m.def("flag_sigmaclip",
              nb::overload_cast<bliss::coarse_channel, int, float, float>(&bliss::flag_sigmaclip),
              "cc_data"_a, "max_iter"_a=3, "lower_threshold"_a=4.0f, "upper_threshold"_a=4.0f);

        m.def("flag_sigmaclip",
              nb::overload_cast<bliss::scan, int, float, float>(&bliss::flag_sigmaclip),
              "scan"_a, "max_iter"_a=3, "lower_threshold"_a=4.0f, "upper_threshold"_a=4.0f);

        m.def("flag_sigmaclip",
              nb::overload_cast<bliss::observation_target, int, float, float>(&bliss::flag_sigmaclip),
              "observation"_a, "max_iter"_a=3, "lower_threshold"_a=4.0f, "upper_threshold"_a=4.0f);

        m.def("flag_sigmaclip",
              nb::overload_cast<bliss::cadence, int, float, float>(&bliss::flag_sigmaclip),
              "cadence"_a, "max_iter"_a=3, "lower_threshold"_a=4.0f, "upper_threshold"_a=4.0f);
    }

    /**
     * @brief Binds the Flag Values enumeration to the Python module.
     * @param m The target nanobind module.
     */
    void bind_flag_values_enum(nb::module_& m) {
        nb::enum_<bliss::flag_values>(m, "flag_values")
            .value("unflagged", bliss::flag_values::unflagged)
            .value("filter_rolloff", bliss::flag_values::filter_rolloff)
            .value("low_spectral_kurtosis", bliss::flag_values::low_spectral_kurtosis)
            .value("high_spectral_kurtosis", bliss::flag_values::high_spectral_kurtosis)
            .value("magnitude", bliss::flag_values::magnitude)
            .value("sigma_clip", bliss::flag_values::sigma_clip)
            .def("__getstate__",
                 [](const bliss::flag_values &self) {
                     return std::make_tuple(static_cast<uint8_t>(self));
                 })
            .def("__setstate__", [](bliss::flag_values &self, const std::tuple<uint8_t> &state) {
                 self = static_cast<bliss::flag_values>(std::get<0>(state));
            });
    }
}

// ============================================================================
// MAIN BINDING ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for Python bindings of the Flaggers module.
 * @details Delegates the binding process to isolated helper functions to maintain low cognitive complexity.
 * @param m The nanobind module to populate.
 */
void bind_pyflaggers(nb::module_ m) {
    bind_spectral_kurtosis(m);
    bind_filter_rolloff(m);
    bind_magnitude(m);
    bind_sigmaclip(m);
    bind_flag_values_enum(m);
}