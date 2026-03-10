#pragma once

#include <core/hit.hpp>
#include <core/scan.hpp>
#include <core/cadence.hpp>

#include <string_view>
#include <vector>
#include <list>
#include <memory> 

namespace bliss {

/**
 * @brief Abstract base class for hit writers (Strategy Pattern).
 */
struct IHitsWriter {
    virtual ~IHitsWriter() = default;
    virtual void write(scan scan_with_hits, std::string_view file_path, double max_drift_rate = 0.0) = 0;
};

/**
 * @brief Concrete writer for the legacy .dat format (TurboSETI compatible).
 */
struct DatHitsWriter : public IHitsWriter {
    void write(scan scan_with_hits, std::string_view file_path, double max_drift_rate = 0.0) override;
};

/**
 * @brief Concrete writer for the modern Cap'n Proto binary format.
 */
struct CapnpHitsWriter : public IHitsWriter {
    void write(scan scan_with_hits, std::string_view file_path, double max_drift_rate = 0.0) override;
};

/**
 * @brief Factory for creating hit writers based on format strings or file extensions.
 */
struct HitsWriterFactory {
    static std::unique_ptr<IHitsWriter> create_writer(std::string_view format);
    static std::unique_ptr<IHitsWriter> create_writer_from_path(std::string_view file_path);
};

// --- Legacy Wrapper Functions ---
// These are maintained for backward compatibility but delegate to the Factory/Strategy classes.

template<template<typename> class Container>
void write_hits_to_file(Container<hit> hits, std::string_view file_path, std::string format="");

std::list<hit> read_hits_from_file(std::string_view file_path, std::string format="");

void write_scan_hits_to_file(scan scan_with_hits, std::string_view file_path, std::string format="", double max_drift_rate=0.0);

scan read_scan_hits_from_file(std::string_view file_path, std::string format="");

} // namespace bliss