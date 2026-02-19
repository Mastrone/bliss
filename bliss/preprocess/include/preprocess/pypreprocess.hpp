#pragma once

#include "excise_dc.hpp"
#include "normalize.hpp"
#include "passband_static_equalize.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include <fmt/format.h>

namespace nb = nanobind;
using namespace nb::literals;

/**
 * @brief Binds the Preprocessing module functions to Python.
 * @details Exposes algorithms for signal conditioning: DC excision, normalization,
 * and polyphase filterbank (PFB) passband equalization.
 * @param m The nanobind module instance.
 */
void bind_pypreprocess(nb::module_ m) {

      // --- Filter Design Utilities ---
      m.def("firdes", bliss::firdes, 
            "Design a FIR filter using the window method.");

      m.def("gen_coarse_channel_response",
            bliss::gen_coarse_channel_response,
            "fine_per_coarse"_a,
            "num_coarse_channels"_a,
            "taps_per_channel"_a,
            "window"_a     = "hamming",
            "device_str"_a = "cpu",
            "Generates the expected frequency response of a PFB channel.");

      // --- Passband Equalization Bindings ---
      
      // Coarse Channel overloads
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::coarse_channel, bland::ndarray, bool>(bliss::equalize_passband_filter), 
            "coarse_channel"_a, "response"_a, "validate"_a=false,
            "Equalize a single coarse channel using an in-memory response array.");
            
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::coarse_channel, std::string_view, bland::ndarray::datatype, bool>(bliss::equalize_passband_filter), 
            "coarse_channel"_a, "response_path"_a, "response_dtype"_a=bland::ndarray::datatype("float"), "validate"_a=false,
            "Equalize a single coarse channel loading the response from a file.");

      // Scan overloads
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::scan, bland::ndarray, bool>(bliss::equalize_passband_filter), 
            "scan"_a, "response"_a, "validate"_a=false);
            
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::scan, std::string_view, bland::ndarray::datatype, bool>(bliss::equalize_passband_filter), 
            "scan"_a, "response_path"_a, "response_dtype"_a=bland::ndarray::datatype("float"), "validate"_a=false);

      // Observation Target overloads
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::observation_target, bland::ndarray, bool>(bliss::equalize_passband_filter), 
            "observation_target"_a, "response"_a, "validate"_a=false);
            
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::observation_target, std::string_view, bland::ndarray::datatype, bool>(bliss::equalize_passband_filter), 
            "observation_target"_a, "response_path"_a, "response_dtype"_a=bland::ndarray::datatype("float"), "validate"_a=false);

      // Cadence overloads
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::cadence, bland::ndarray, bool>(bliss::equalize_passband_filter), 
            "cadence"_a, "response"_a, "validate"_a=false);
            
      m.def("equalize_passband_filter", 
            nb::overload_cast<bliss::cadence, std::string_view, bland::ndarray::datatype, bool>(bliss::equalize_passband_filter), 
            "cadence"_a, "response_path"_a, "response_dtype"_a=bland::ndarray::datatype("float"), "validate"_a=false);

      // --- Normalization Bindings ---
      m.def("normalize", nb::overload_cast<bliss::coarse_channel>(bliss::normalize), 
            "Normalize a coarse channel (max-scaling).");
      m.def("normalize", nb::overload_cast<bliss::scan>(bliss::normalize));
      m.def("normalize", nb::overload_cast<bliss::observation_target>(bliss::normalize));
      m.def("normalize", nb::overload_cast<bliss::cadence>(bliss::normalize));


      // --- DC Excision Bindings ---
      m.def("excise_dc", nb::overload_cast<bliss::coarse_channel>(bliss::excise_dc),
            "Remove the central DC bin spike.");
      m.def("excise_dc", nb::overload_cast<bliss::scan>(bliss::excise_dc));
      m.def("excise_dc", nb::overload_cast<bliss::observation_target>(bliss::excise_dc));
      m.def("excise_dc", nb::overload_cast<bliss::cadence>(bliss::excise_dc));

}