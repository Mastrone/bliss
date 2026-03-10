#include "estimators/spectral_kurtosis.hpp"

#include <bland/ops/ops.hpp>

#include <fmt/format.h>

using namespace bliss;

bland::ndarray bliss::estimate_spectral_kurtosis(const bland::ndarray &spectrum_grid, int32_t N, int32_t M, float d) {
    // fmt::print("INFO: spec kurtosis with M={}, N={}, d={}\n", M, N, d);
    
    // S1 = Sum(X)
    auto s1 = bland::square(bland::sum(spectrum_grid, {0}));
    
    // S2 = Sum(X^2)
    auto s2 = bland::sum(bland::square(spectrum_grid), {0});
    
    // Formula: SK = ((M*N*d + 1) / (M-1)) * ( (M * S2 / S1^2) - 1 )
    auto sk = ((M * N * d + 1) / (M - 1)) * (M * (s2 / s1) - 1);
    
    return sk;
}

bland::ndarray bliss::estimate_spectral_kurtosis(coarse_channel &cc_data, float d) {
    const bland::ndarray spectrum_grid = cc_data.data();

    // 1. Derive N and M from data shape and metadata
    auto M  = spectrum_grid.size(0); // Number of spectra in the block
    auto Fs = std::abs(1.0 / (1e6 * cc_data.foff())); // Sampling rate
    auto N  = std::round(cc_data.tsamp() / Fs); // Accumulation length (samples per bin)

    auto sk = estimate_spectral_kurtosis(spectrum_grid, N, M, d);

    return sk;
}