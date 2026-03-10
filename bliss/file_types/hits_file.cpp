#include "file_types/hits_file.hpp"

#include "file_types/dat_file.hpp"
#include "file_types/cpnp_files.hpp"

#include <fmt/format.h>

#include <string_view>
#include <string>
#include <stdexcept>

using namespace bliss;

// Simple helper for string suffix checking
static bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

// --- Concrete Writer Implementations ---

void DatHitsWriter::write(scan scan_with_hits, std::string_view file_path, double max_drift_rate) {
    // Delegate to existing function in dat_file.cpp
    write_scan_hits_to_dat_file(scan_with_hits, file_path, max_drift_rate);
}

void CapnpHitsWriter::write(scan scan_with_hits, std::string_view file_path, double max_drift_rate) {
    // Note: Cap'n Proto format might not explicitly use max_drift_rate in the same way as the header of .dat files,
    // but the interface requires it for uniformity.
    write_scan_hits_to_capnp_file(scan_with_hits, file_path);
}

// --- Factory Implementation ---

std::unique_ptr<IHitsWriter> HitsWriterFactory::create_writer(std::string_view format) {
    if (format == "dat") {
        return std::make_unique<DatHitsWriter>();
    } else if (format == "capnp") {
        return std::make_unique<CapnpHitsWriter>();
    } else {
        // Default Fallback
        fmt::print("INFO: Unknown format '{}' specified. Defaulting to capnp serialization\n", format);
        return std::make_unique<CapnpHitsWriter>();
    }
}

std::unique_ptr<IHitsWriter> HitsWriterFactory::create_writer_from_path(std::string_view file_path) {
    std::string format = "capnp"; // Default
    if (ends_with(file_path, ".dat")) {
        format = "dat";
    } else if (ends_with(file_path, ".capnp") || ends_with(file_path, ".cp")) {
        format = "capnp";
    }
    return create_writer(format);
}

// --- Helper Functions (Refactored) ---

template<template<typename> class Container>
void bliss::write_hits_to_file(Container<hit> hits, std::string_view file_path, std::string format) {
    // Auto-detect format if not provided
    if (format.empty()) {
        if (ends_with(file_path, ".dat")) {
            format = "dat";
        } else if (ends_with(file_path, ".capnp") || ends_with(file_path, ".cp")) {
            format = "capnp";
        }
    }

    if (format == "dat") {
        // Dat writer currently requires a Scan object, not just a container of hits.
        // Assuming there is an overload or logic elsewhere for raw containers to .dat
        // For now, this branch might need attention if dat_file only supports scan objects.
        // write_hits_to_dat_file(hits, file_path); // Hypothetical function
        fmt::print("WARN: Writing raw hit container to .dat not fully supported via this path yet.\n");
    } else if (format == "capnp") {
        write_hits_to_capnp_file(hits, file_path);
    } else {
        fmt::print("INFO: No format specified while writing hits to disk. Defaulting to capnp serialization\n");
        write_hits_to_capnp_file(hits, file_path);
    }
}

std::list<hit> bliss::read_hits_from_file(std::string_view file_path, std::string format) {
    if (format.empty() || format == "capnp") {
        return read_hits_from_capnp_file(file_path);
    } else if (format == "dat" || format == "turboseti") {
        throw std::runtime_error("read_hits_from_dat_file not implemented yet");
        // return read_hits_from_dat_file(file_path);
    } else {
        fmt::print("ERROR: read_hits_from_file got format '{}' which is unknown. Expect one of 'capnp', 'dat', 'turboseti'\n", format);
        throw std::invalid_argument("Unknown file format to read_hits_from_file");
    }
}

// Refactored to use the Factory
void bliss::write_scan_hits_to_file(scan scan_with_hits, std::string_view file_path, std::string format, double max_drift_rate) {
    std::unique_ptr<IHitsWriter> writer;

    if (format.empty()) {
        writer = HitsWriterFactory::create_writer_from_path(file_path);
    } else {
        writer = HitsWriterFactory::create_writer(format);
    }

    if (writer) {
        writer->write(scan_with_hits, file_path, max_drift_rate);
    }
}

scan bliss::read_scan_hits_from_file(std::string_view file_path, std::string format) {
    if (format.empty() || format == "capnp") {
        return read_scan_hits_from_capnp_file(file_path);
    } else if (format == "dat" || format == "turboseti") {
        throw std::runtime_error("read_hits_from_dat_file not implemented yet");
        // return read_scan_hits_from_dat_file(file_path);
    } else {
        fmt::print("ERROR: read_scan_hits_from_file got format '{}' which is unknown. Expect one of 'capnp', 'dat', 'turboseti'\n", format);
        throw std::invalid_argument("Unknown file format to read_scan_hits_from_file");
    }
}