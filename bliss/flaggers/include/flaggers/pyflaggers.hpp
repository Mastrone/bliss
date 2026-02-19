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

namespace nb = nanobind;
using namespace nb::literals;

/**
 * @brief Python bindings for the Flaggers module.
 */
void bind_pyflaggers(nb::module_ m) {

    // --- Spectral Kurtosis ---
    m.def("flag_spectral_kurtosis",
          nb::overload_cast<bliss::coarse_channel, float, float, float>(&bliss::flag_spectral_kurtosis),
          "cc_data"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0,
          "Flag non-gaussian samples in a channel using Spectral Kurtosis.");

    m.def("flag_spectral_kurtosis",
          nb::overload_cast<bliss::scan, float, float, float>(&bliss::flag_spectral_kurtosis),
          "scan"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0,
          "Flag non-gaussian samples in a scan.");

    m.def("flag_spectral_kurtosis",
          nb::overload_cast<bliss::observation_target, float, float, float>(&bliss::flag_spectral_kurtosis),
          "observation"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a = 2.0);

    m.def("flag_spectral_kurtosis",
          nb::overload_cast<bliss::cadence, float, float, float>(&bliss::flag_spectral_kurtosis),
          "cadence"_a, "lower_threshold"_a, "upper_threshold"_a, "d"_a=2.0);

    // --- Filter Rolloff ---
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

    // --- Magnitude ---
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

    // --- Sigma Clipping ---
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

    // --- Flag Values Enum ---
    static auto flag_names = std::vector<std::string>{
        "unflagged", "filter_rolloff", "low_spectral_kurtosis", "high_spectral_kurtosis",
        "RESERVED_0", "magnitude", "sigmaclip", "RESERVED_1", "RESERVED_2"
    };

    nb::enum_<bliss::flag_values>(m, "flag_values")
            .value("unflagged", bliss::flag_values::unflagged)
            .value("filter_rolloff", bliss::flag_values::filter_rolloff)
            .value("low_spectral_kurtosis", bliss::flag_values::low_spectral_kurtosis)
            .value("high_spectral_kurtosis", bliss::flag_values::high_spectral_kurtosis)
            .value("magnitude", bliss::flag_values::magnitude)
            .value("sigma_clip", bliss::flag_values::sigma_clip)
            .def("__getstate__",
                 [](const bliss::flag_values &self) {
                     std::cout << "get state is " << static_cast<int>(self) << std::endl;
                     return std::make_tuple(static_cast<uint8_t>(self));
                 })
            .def("__setstate__", [](bliss::flag_values &self, const std::tuple<uint8_t> &state) {
                std::cout << "called setstate" << std::endl;
                self = static_cast<bliss::flag_values>(std::get<0>(state));
            });
}