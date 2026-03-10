#pragma once

#include <bland/ndarray.hpp>
#include <core/cadence.hpp>
#include <core/coarse_channel.hpp>
#include <core/scan.hpp>

namespace bliss {

    /**
     * @brief Generates FIR filter coefficients using the window method.
     * @param num_taps Number of filter taps.
     * @param fc Cutoff frequency (normalized 0..1).
     * @param window Window function name (e.g., "hamming").
     * @return Array of coefficients.
     */
    bland::ndarray firdes(int num_taps, float fc, std::string_view window);

    /**
     * @brief Simulates the frequency response of the polyphase filterbank used to channelize the data.
     * @details This response is used to invert (flatten) the bandpass shape.
     * @param fine_per_coarse Number of fine channels per coarse channel.
     * @param num_coarse_channels Total coarse channels.
     * @param taps_per_channel PFB taps per channel (usually 4 or 8).
     * @param window Window function.
     * @param device_str Compute device.
     * @return The magnitude squared response of the channel.
     */
    bland::ndarray gen_coarse_channel_response(int fine_per_coarse, int num_coarse_channels=2048, int taps_per_channel=4, std::string window="hamming", std::string device_str="cpu");

    /**
     * @brief Equalizes the spectrum by dividing out the static passband response.
     * @param cc The coarse channel to equalize.
     * @param h The filter response to divide by (should match channel shape).
     * @param validate If true, performs heuristic checks to ensure the correction is valid.
     * @return The flattened coarse channel.
     */
    coarse_channel equalize_passband_filter(coarse_channel cc, bland::ndarray h, bool validate=false);

    /**
     * @brief Equalizes using a response loaded from a file.
     */
    coarse_channel equalize_passband_filter(coarse_channel cc, std::string_view h_resp_filepath, bland::ndarray::datatype dtype=bland::ndarray::datatype::float32, bool validate=false);

    // Overloads for Scan, ObservationTarget, Cadence...
    scan equalize_passband_filter(scan sc, bland::ndarray h, bool validate=false);
    scan equalize_passband_filter(scan sc, std::string_view h_resp_filepath, bland::ndarray::datatype dtype=bland::ndarray::datatype::float32, bool validate=false);

    observation_target equalize_passband_filter(observation_target ot, bland::ndarray h, bool validate=false);
    observation_target equalize_passband_filter(observation_target ot, std::string_view h_resp_filepath, bland::ndarray::datatype dtype=bland::ndarray::datatype::float32, bool validate=false);

    cadence equalize_passband_filter(cadence ca, bland::ndarray h, bool validate=false);
    cadence equalize_passband_filter(cadence ca, std::string_view h_resp_filepath, bland::ndarray::datatype dtype=bland::ndarray::datatype::float32, bool validate=false);

}