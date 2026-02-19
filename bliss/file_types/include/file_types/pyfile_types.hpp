#pragma once

#include "events_file.hpp"
#include "h5_filterbank_file.hpp"
#include "cpnp_files.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

/**
 * @brief Binds the File Types module functions and classes to Python.
 * @details Exposes HDF5 reading capabilities and Cap'n Proto serialization functions.
 * @param m The nanobind module instance.
 */
void bind_pyfile_types(nb::module_ m) {

    // Bind HDF5 Filterbank File reader
    nb::class_<bliss::h5_filterbank_file>(m, "h5_filterbank_file")
            .def(nb::init<std::string>(), "Open an HDF5 filterbank file.")
            .def("__repr__", &bliss::h5_filterbank_file::repr)
            .def("read_data", &bliss::h5_filterbank_file::read_data, "Read a hyperslab of data.")
            .def("read_mask", &bliss::h5_filterbank_file::read_mask, "Read a hyperslab of the mask.")
            .def("get_file_path", &bliss::h5_filterbank_file::get_file_path);

    // Bind Cap'n Proto Serialization for Hits
    m.def("write_hits_to_capnp_file", &bliss::write_hits_to_capnp_file<std::list<bliss::hit>>);
    m.def("read_hits_from_capnp_file", &bliss::read_hits_from_capnp_file);
    
    // Bind Cap'n Proto Serialization for Scans
    m.def("write_scan_hits_to_file",
          &bliss::write_scan_hits_to_capnp_file,
          "scan_with_hits"_a,
          "file_path"_a,
          "Write scan metadata and associated hits as cap'n proto messages to binary file.");
          
    m.def("read_scan_hits_from_capnp_file",
          &bliss::read_scan_hits_from_capnp_file,
          "file_path"_a,
          "Read cap'n proto serialized scan from file.");

    // Bind Cap'n Proto Serialization for Observation Targets
    m.def("write_observation_target_hits_to_capnp_files",
          &bliss::write_observation_target_hits_to_capnp_files,
          "observation_target"_a,
          "file_path"_a,
          "Write an observation target's scan metadata and hits to multiple files.");
          
    m.def("read_observation_target_hits_from_capnp_files", 
          &bliss::read_observation_target_hits_from_capnp_files, 
          "file_path"_a);

    // Bind Cap'n Proto Serialization for Cadences
    m.def("write_cadence_hits_to_capnp_files",
          &bliss::write_cadence_hits_to_capnp_files,
          "cadence_with_hits"_a,
          "base_filename"_a,
          "Write all detected hits for all scans in a cadence to binary files.");
          
    m.def("read_cadence_hits_from_capnp_files",
          &bliss::read_cadence_hits_from_capnp_files,
          "base_filename"_a);

    // Bind Event Serialization
    m.def("write_events_to_file", &bliss::write_events_to_file);
    m.def("read_events_from_file", &bliss::read_events_from_file);
}