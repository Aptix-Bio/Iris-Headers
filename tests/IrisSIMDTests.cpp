/**
 * @file IrisSIMDTests.cpp
 * @brief Checks the SIMD tile downsample kernels against a scalar box average.
 *
 * Every (factor, channels, sub_y, sub_x) combination is run over several
 * source patterns. The destination is pre-filled with noise and compared in
 * full, so the test also catches writes outside the addressed sub-region.
 *
 * Build with -DIRIS_HEADERS_BUILD_TESTS=ON (requires Google Highway), then
 * run ctest. Also worth running under -fsanitize=address,undefined.
 */
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "IrisCore.hpp"
#include "IrisBuffer.hpp"
#include "IrisSIMD.hpp"

using namespace Iris;

// Rounded (half-up) mean of each factor x factor block, written into the
// (sub_y, sub_x) sub-region of a full destination tile.
static void reference_downsample (const uint8_t* src, uint8_t* dst, unsigned factor,
                                  unsigned sub_y, unsigned sub_x, unsigned channels)
{
    const unsigned out    = TILE_PIX_LENGTH / factor;
    const unsigned stride = TILE_PIX_LENGTH * channels;
    const unsigned area   = factor * factor;
    for (unsigned y = 0; y < out; ++y)
        for (unsigned x = 0; x < out; ++x)
            for (unsigned c = 0; c < channels; ++c) {
                unsigned sum = 0;
                for (unsigned dy = 0; dy < factor; ++dy)
                    for (unsigned dx = 0; dx < factor; ++dx)
                        sum += src[(y * factor + dy) * stride + (x * factor + dx) * channels + c];
                dst[(sub_y * out + y) * stride + (sub_x * out + x) * channels + c] =
                    static_cast<uint8_t>((sum + area / 2) / area);
            }
}

enum Pattern { PATTERN_RANDOM, PATTERN_ZERO, PATTERN_MAX, PATTERN_COORDINATES };
static const char* pattern_name (Pattern p)
{
    switch (p) {
        case PATTERN_RANDOM:        return "random";
        case PATTERN_ZERO:          return "zero";
        case PATTERN_MAX:           return "max";
        case PATTERN_COORDINATES:   return "coordinates";
    } return "?";
}
static void fill_source (uint8_t* src, unsigned channels, Pattern pattern, std::mt19937& rng)
{
    const unsigned bytes = TILE_PIX_AREA * channels;
    for (unsigned i = 0; i < bytes; ++i) {
        const unsigned pixel = i / channels, c = i % channels;
        const unsigned x = pixel % TILE_PIX_LENGTH, y = pixel / TILE_PIX_LENGTH;
        switch (pattern) {
            case PATTERN_RANDOM:        src[i] = static_cast<uint8_t>(rng()); break;
            case PATTERN_ZERO:          src[i] = 0x00; break;
            case PATTERN_MAX:           src[i] = 0xFF; break;
            // Distinct per channel and column so a lane reading the wrong
            // pixel or channel changes the average.
            case PATTERN_COORDINATES:   src[i] = static_cast<uint8_t>(x * 7 + y * 3 + c * 64); break;
        }
    }
}

int main ()
{
    std::mt19937 rng (0x1415);
    unsigned failures = 0, cases = 0;
    for (unsigned factor : {2u, 4u})
    for (unsigned channels : {3u, 4u})
    for (Pattern pattern : {PATTERN_RANDOM, PATTERN_ZERO, PATTERN_MAX, PATTERN_COORDINATES})
    for (unsigned sub_y = 0; sub_y < factor; ++sub_y)
    for (unsigned sub_x = 0; sub_x < factor; ++sub_x) {
        const size_t bytes  = TILE_PIX_AREA * channels;
        Buffer src          = Create_strong_buffer(bytes);
        Buffer dst          = Create_strong_buffer(bytes);
        auto src_data       = static_cast<uint8_t*>(src->data());
        auto dst_data       = static_cast<uint8_t*>(dst->data());
        fill_source(src_data, channels, pattern, rng);
        for (size_t i = 0; i < bytes; ++i)
            dst_data[i] = static_cast<uint8_t>(rng());

        std::vector<uint8_t> expected (dst_data, dst_data + bytes);
        reference_downsample(src_data, expected.data(), factor, sub_y, sub_x, channels);

        if (factor == 2) SIMD::Downsample_into_tile_2x_avg(src, dst, sub_y, sub_x, channels);
        else             SIMD::Downsample_into_tile_4x_avg(src, dst, sub_y, sub_x, channels);

        ++cases;
        size_t mismatches = 0, first = 0;
        for (size_t i = 0; i < bytes; ++i)
            if (dst_data[i] != expected[i] && mismatches++ == 0) first = i;
        if (mismatches) {
            ++failures;
            const size_t pixel = first / channels;
            std::printf("FAIL %ux avg, %u ch, %-11s sub (%u,%u): %zu bytes differ; "
                        "first at pixel (x=%zu, y=%zu) ch %zu: got %u, expected %u\n",
                        factor, channels, pattern_name(pattern), sub_y, sub_x, mismatches,
                        pixel % TILE_PIX_LENGTH, pixel / TILE_PIX_LENGTH, first % channels,
                        dst_data[first], expected[first]);
        }
    }
    std::printf("%u / %u downsample cases match the scalar reference\n", cases - failures, cases);
    return failures ? 1 : 0;
}
