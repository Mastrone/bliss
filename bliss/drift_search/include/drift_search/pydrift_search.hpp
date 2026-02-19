#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/vector.h>

#include "connected_components.hpp"
#include "event_search.hpp"
#include "hit_search.hpp"
#include "integrate_drifts.hpp"
#include "local_maxima.hpp"
#include "filter_hits.hpp"

/**
 * @brief Python bindings for the drift search module.
 */
void bind_pydrift_search(nb::module_ m) {

    // --- Integration / Dedoppler Bindings ---
    // Expose overloads for all hierarchy levels (channel, scan, target, cadence)
    m.def("integrate_drifts",
        nb::overload_cast<bliss::coarse_channel, bliss::integrate_drifts_options>(&bliss::integrate_drifts));

    m.def("integrate_drifts",
          nb::overload_cast<bliss::scan, bliss::integrate_drifts_options>(&bliss::integrate_drifts));

    m.def("integrate_drifts",
          nb::overload_cast<bliss::observation_target, bliss::integrate_drifts_options>(&bliss::integrate_drifts));

    m.def("integrate_drifts",
          nb::overload_cast<bliss::cadence, bliss::integrate_drifts_options>(&bliss::integrate_drifts));

    // --- Data Structures ---
    
    // Protohit
    nb::class_<bliss::protohit>(m, "protohit")
            .def_rw("locations", &bliss::protohit::locations)
            .def_rw("index_max", &bliss::protohit::index_max)
            .def_rw("max_integration", &bliss::protohit::max_integration)
            .def_rw("rfi_counts", &bliss::protohit::rfi_counts);

    // Hit (Full physical description)
    nb::class_<bliss::hit>(m, "hit")
            .def_rw("start_freq_index", &bliss::hit::start_freq_index)
            .def_rw("start_freq_MHz", &bliss::hit::start_freq_MHz)
            .def_rw("start_time_sec", &bliss::hit::start_time_sec)
            .def_rw("duration_sec", &bliss::hit::duration_sec)
            .def_rw("rate_index", &bliss::hit::rate_index)
            .def_rw("drift_rate_Hz_per_sec", &bliss::hit::drift_rate_Hz_per_sec)
            .def_rw("power", &bliss::hit::power)
            .def_rw("time_span_steps", &bliss::hit::time_span_steps)
            .def_rw("snr", &bliss::hit::snr)
            .def_rw("bandwidth", &bliss::hit::bandwidth)
            .def_rw("binwidth", &bliss::hit::binwidth)
            .def_rw("rfi_counts", &bliss::hit::rfi_counts)
            .def("__repr__", &bliss::hit::repr)
            // Pickling support
            .def("__getstate__", &bliss::hit::get_state)
            .def("__setstate__", [](bliss::hit &self, const bliss::hit::state_tuple &state) {
                new (&self) bliss::hit;
                self.set_state(state);
            });

    // --- Hit Finding Algorithms ---

    // Low-level component finding
    m.def("find_components_above_threshold", &bliss::find_components_above_threshold);
    m.def("find_components_in_binary_mask", &bliss::find_components_in_binary_mask);
    m.def("find_components_in_binary_mask",
          [](nb::ndarray<> threshold_mask, std::vector<bland::nd_coords> neighborhood) {
              return bliss::find_components_in_binary_mask(nb_to_bland(threshold_mask), neighborhood);
          });
    m.def("find_local_maxima_above_threshold", &bliss::find_local_maxima_above_threshold);

    // Options enums and structs
    nb::enum_<bliss::hit_search_methods>(m, "hit_search_methods")
            .value("connected_components", bliss::hit_search_methods::CONNECTED_COMPONENTS)
            .value("local_maxima", bliss::hit_search_methods::LOCAL_MAXIMA);

    nb::class_<bliss::hit_search_options>(m, "hit_search_options")
            .def(nb::init<>())
            .def_rw("method", &bliss::hit_search_options::method)
            .def_rw("snr_threshold", &bliss::hit_search_options::snr_threshold)
            .def_rw("neighbor_l1_dist", &bliss::hit_search_options::neighbor_l1_dist);

    // High-level Hit Search Pipeline
    m.def("hit_search", nb::overload_cast<bliss::scan, bliss::hit_search_options>(&bliss::hit_search));
    m.def("hit_search", nb::overload_cast<bliss::observation_target, bliss::hit_search_options>(&bliss::hit_search));
    m.def("hit_search", nb::overload_cast<bliss::cadence, bliss::hit_search_options>(&bliss::hit_search));

    // --- Event Search ---
    nb::class_<bliss::event>(m, "event")
            .def_rw("hits", &bliss::event::hits)
            .def_rw("starting_frequency_Hz", &bliss::event::starting_frequency_Hz)
            .def_rw("average_power", &bliss::event::average_power)
            .def_rw("average_bandwidth", &bliss::event::average_bandwidth)
            .def_rw("average_snr", &bliss::event::average_snr)
            .def_rw("average_drift_rate_Hz_per_sec", &bliss::event::average_drift_rate_Hz_per_sec)
            .def_rw("event_start_seconds", &bliss::event::event_start_seconds)
            .def_rw("event_end_seconds", &bliss::event::event_end_seconds)
            .def("__repr__", &bliss::event::repr)
            .def("__getstate__", [](const bliss::event &self) { return self.hits; });
            
    m.def("event_search", &bliss::event_search);

    // --- Filtering ---
    nb::class_<bliss::filter_options>(m, "filter_options")
    .def(nb::init<>())
    .def_rw("filter_zero_drift", &bliss::filter_options::filter_zero_drift)
    .def_rw("filter_sigmaclip", &bliss::filter_options::filter_sigmaclip)
    .def_rw("minimum_percent_sigmaclip", &bliss::filter_options::minimum_percent_sigmaclip)
    .def_rw("filter_high_sk", &bliss::filter_options::filter_high_sk)
    .def_rw("minimum_percent_high_sk", &bliss::filter_options::minimum_percent_high_sk)
    .def_rw("filter_low_sk", &bliss::filter_options::filter_low_sk)
    .def_rw("maximum_percent_low_sk", &bliss::filter_options::maximum_percent_low_sk);

    m.def("filter_hits", nb::overload_cast<std::list<bliss::hit>, bliss::filter_options>(&bliss::filter_hits));
    m.def("filter_hits", nb::overload_cast<bliss::coarse_channel, bliss::filter_options>(&bliss::filter_hits));
    m.def("filter_hits", nb::overload_cast<bliss::scan, bliss::filter_options>(&bliss::filter_hits));
    m.def("filter_hits", nb::overload_cast<bliss::observation_target, bliss::filter_options>(&bliss::filter_hits));
    m.def("filter_hits", nb::overload_cast<bliss::cadence, bliss::filter_options>(&bliss::filter_hits));
}