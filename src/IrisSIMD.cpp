/**
 * @file IrisSIMD.cpp
 * @author ryanlandvater (ryanlandvater [at] gmail [dot] com)
 * @brief Iris SIMD operations implementation file using
 * google highway library.
 * @version 0.1
 * @date 2023-10-05
 *
 * @copyright Copyright (c) 2023-2025
 *
 */
#include <stddef.h>
#include <assert.h>

#include "hwy/highway.h"
#include "IrisCore.hpp"
#include "IrisBuffer.hpp"
struct uint8x3_t {
    uint8_t v0;
    uint8_t v1;
    uint8_t v2;
    uint8x3_t (uint8_t* p) {
        *this = *reinterpret_cast<uint8x3_t*>(p);
    }
};
struct uint8x4_t {
    uint8_t v0;
    uint8_t v1;
    uint8_t v2;
    uint8_t a;
    uint8x4_t (uint8_t* p) {
        *this = *reinterpret_cast<uint8x4_t*>(p);
    }
};
HWY_BEFORE_NAMESPACE();
namespace Iris {
namespace SIMD {
using namespace hwy;
using namespace hwy::HWY_NAMESPACE;
namespace HWY_NAMESPACE {

// Helper functions to resolve overload ambiguity
template<typename D>
HWY_INLINE void LoadInterleaved3Helper(D d, const uint8_t* HWY_RESTRICT src,
                                     Vec<D>& v0, Vec<D>& v1, Vec<D>& v2) {
    LoadInterleaved3(d, src, v0, v1, v2);
}

template<typename D>
HWY_INLINE void LoadInterleaved4Helper(D d, const uint8_t* HWY_RESTRICT src,
                                     Vec<D>& v0, Vec<D>& v1, Vec<D>& v2, Vec<D>& v3) {
    LoadInterleaved4(d, src, v0, v1, v2, v3);
}

template<typename D>
HWY_INLINE void StoreInterleaved3Helper(D d, const Vec<D>& v0, const Vec<D>& v1, const Vec<D>& v2,
                                      uint8_t* HWY_RESTRICT dst) {
    StoreInterleaved3(v0, v1, v2, d, dst);
}

template<typename D>
HWY_INLINE void StoreInterleaved4Helper(D d, const Vec<D>& v0, const Vec<D>& v1,
                                      const Vec<D>& v2, const Vec<D>& v3,
                                      uint8_t* HWY_RESTRICT dst) {
    StoreInterleaved4(v0, v1, v2, v3, d, dst);
}

// Channel-count generic forms: CH interleaved 8-bit channels <-> one vector
// per channel (one lane per pixel). v3 is untouched / ignored when CH == 3.
template<uint8_t CH, typename D>
HWY_INLINE void LoadPixelsHelper(D d, const uint8_t* HWY_RESTRICT src,
                                 Vec<D>& v0, Vec<D>& v1, Vec<D>& v2, Vec<D>& v3) {
    if constexpr (CH == 3) LoadInterleaved3Helper(d, src, v0, v1, v2);
    else                   LoadInterleaved4Helper(d, src, v0, v1, v2, v3);
}

template<uint8_t CH, typename D>
HWY_INLINE void StorePixelsHelper(D d, const Vec<D>& v0, const Vec<D>& v1,
                                  const Vec<D>& v2, const Vec<D>& v3,
                                  uint8_t* HWY_RESTRICT dst) {
    if constexpr (CH == 3) StoreInterleaved3Helper(d, v0, v1, v2, dst);
    else                   StoreInterleaved4Helper(d, v0, v1, v2, v3, dst);
}

HWY_API void EXPAND_TILE_ADD_ALPHA_8bit (const uint8_t* src, uint8_t* dst)
{
    // This is done BACKWARDS so that it is safe
    // when src and dst are the same pointers (IE can be done from same buffer)
    const ScalableTag<uint8_t> d8;
    const auto N = static_cast<int32_t>(Lanes(d8));
    const auto a = Set(d8, 0xFF);
    Vec<ScalableTag<uint8_t>> v0,v1,v2;
    int32_t i = TILE_PIX_AREA-(N+1);
    for (; i - N >= 0; i -= N) {
        LoadInterleaved3Helper(d8, src + i * 3, v0, v1, v2);
        StoreInterleaved4Helper(d8, v0, v1, v2, a, dst + i * 4);
    }
    for (; i >= 0; --i) {
        memcpy(dst + i * 4, src + i * 3, 3);
        dst[i * 4 + 3] = 0xFF;
    }
}
HWY_API void SHRINK_TILE_RM_ALPHA_8bit (const uint8_t* src, uint8_t* dst)
{
    // This is done FORWARDS so that it is safe
    // when src and dst are the same pointers (IE can be done from same buffer)
    const ScalableTag<uint8_t> d8;
    const auto N = Lanes(d8);
    uint32_t   i = 0;
    Vec<ScalableTag<uint8_t>> v0,v1,v2,a;
    for (; i + N < TILE_PIX_AREA; i+=N) {
        LoadInterleaved4Helper(d8, src + i * 4, v0, v1, v2, a);
        StoreInterleaved3Helper(d8, v0, v1, v2, dst + i * 3);
    } for (; i < TILE_PIX_AREA; ++i)
        memcpy(dst + i * 3, src + i * 4, 3);
    
}
HWY_API void SWAP_TILE_3_CHANNELS_0_2_8bit (uint8_t* src)
{
    const ScalableTag<uint8_t> d8;
    const auto N = Lanes(d8);
    uint32_t   i = 0;
    Vec<ScalableTag<uint8_t>> v0,v1,v2;
    for (; i + N < TILE_PIX_AREA * 3; i += N * 3) {
        LoadInterleaved3Helper(d8, src + i, v0, v1, v2);
        StoreInterleaved3Helper(d8, v2, v1, v0, src + i);
    } for (; i < TILE_PIX_AREA * 3; i += 3) {
        uint8x3_t _ (src + i);
        src[i]      = _.v2;
        src[i + 2]  = _.v0;
    }
}
HWY_API void SWAP_TILE_4_CHANNELS_0_2_8bit (uint8_t* src)
{
    const ScalableTag<uint8_t> d8;
    const auto N = Lanes(d8);
    uint32_t   i = 0;
    Vec<ScalableTag<uint8_t>> v0,v1,v2, a;
    for (; i + N < TILE_PIX_AREA * 4; i += N * 4) {
        LoadInterleaved4Helper(d8, src + i, v0, v1, v2, a);
        StoreInterleaved4Helper(d8, v2, v1, v0, a, src + i);
    } for (; i < TILE_PIX_AREA * 4; i += 4) {
        uint8x3_t _ (src + i);
        src[i]      = _.v2;
        src[i + 2]  = _.v0;
    }
}

template<uint8_t CH>
HWY_API void DOWNSAMPLE_INTO_TILE_2X_AVG(const uint8_t* HWY_RESTRICT src,
                                       uint8_t* HWY_RESTRICT dst,
                                       const uint16_t s_y,
                                       const uint16_t s_x) {
    static_assert(CH == 3 || CH == 4, "Only 3 (RGB) or 4 (RGBA) channels supported");
    const uint8_t o_y = s_y << 7;   // sub-x region [0,1] * 128 pixels
    const uint8_t o_x = s_x << 7;   // sub-y region [0,1] * 128 pixels
    constexpr auto stride = TILE_PIX_LENGTH * CH;
    
    // Channels are de-interleaved on load so each u8 lane is one source pixel;
    // SumsOf2 then adds each horizontal pixel pair into one u16 lane. Every N
    // source pixels of a row therefore produce N/2 destination pixels (d8_dst).
    const ScalableTag<uint8_t> d8;
    const RepartitionToWide<decltype(d8)> d16;
    const Rebind<uint8_t, decltype(d16)> d8_dst;
    const size_t N = Lanes(d8);
    // Vectors must hold whole pixel pairs; only HWY_SCALAR (1 lane) does not,
    // and there the scalar loop below does all of the work.
    const bool simd = N % 2 == 0;
    
    const auto round = Set(d16, 2);         // Round by adding 2 (then divide by 4)
    
    for (auto y = 0; y < 128; ++y) {
        const auto row0 = src + (2 * y) * stride;
        const auto row1 = src + (2 * y + 1) * stride;
        auto orow = dst + (y + o_y) * stride + o_x * CH;
        
        size_t x = 0;   // Source pixel column
        for (; simd && x + N <= TILE_PIX_LENGTH; x += N) {
            // Per-channel sums of each 2x2 block (one u16 lane per destination pixel)
            auto s0 = Zero(d16), s1 = Zero(d16), s2 = Zero(d16), s3 = Zero(d16);
            for (const auto row : {row0, row1}) {
                Vec<decltype(d8)> c0, c1, c2, c3;
                LoadPixelsHelper<CH>(d8, row + x * CH, c0, c1, c2, c3);
                s0 = Add(s0, SumsOf2(c0));
                s1 = Add(s1, SumsOf2(c1));
                s2 = Add(s2, SumsOf2(c2));
                if constexpr (CH == 4) s3 = Add(s3, SumsOf2(c3));
            }
            StorePixelsHelper<CH>(d8_dst,
                                  DemoteTo(d8_dst, ShiftRight<2>(Add(s0, round))),
                                  DemoteTo(d8_dst, ShiftRight<2>(Add(s1, round))),
                                  DemoteTo(d8_dst, ShiftRight<2>(Add(s2, round))),
                                  DemoteTo(d8_dst, ShiftRight<2>(Add(s3, round))),
                                  orow + (x / 2) * CH);
        }
        
        // Scalar cleanup (remainder of the row when N does not divide it evenly)
        for (; x < TILE_PIX_LENGTH; x += 2) {
            for (int c = 0; c < CH; ++c) {
                const uint16_t sum =
                    row0[x * CH + c] + row0[(x + 1) * CH + c] +
                    row1[x * CH + c] + row1[(x + 1) * CH + c];
                orow[(x / 2) * CH + c] = static_cast<uint8_t>((sum + 2) >> 2);
            }
        }
    }
}
HWY_API void DOWNSAMPLE_INTO_TILE_2X_AVG_3(const uint8_t* HWY_RESTRICT src,
                                                 uint8_t* HWY_RESTRICT dst,
                                                 const uint16_t s_y,
                                                 const uint16_t s_x) {
    DOWNSAMPLE_INTO_TILE_2X_AVG<3>(src, dst, s_y, s_x);
}

HWY_API void DOWNSAMPLE_INTO_TILE_2X_AVG_4(const uint8_t* HWY_RESTRICT src,
                                                 uint8_t* HWY_RESTRICT dst,
                                                 const uint16_t s_y,
                                                 const uint16_t s_x) {
    DOWNSAMPLE_INTO_TILE_2X_AVG<4>(src, dst, s_y, s_x);
}

template<uint8_t CH>
inline void DOWNSAMPLE_INTO_TILE_4X_AVG(const uint8_t* HWY_RESTRICT src,
                                       uint8_t* HWY_RESTRICT dst,
                                       const uint16_t s_y,
                                       const uint16_t s_x) {
    static_assert(CH == 3 || CH == 4, "Only 3 (RGB) or 4 (RGBA) channels supported");
    
    const uint8_t o_y = s_y << 6;   // sub-x region [0,3] * 64 pixels
    const uint8_t o_x = s_x << 6;   // sub-y region [0,3] * 64 pixels
    constexpr auto stride = TILE_PIX_LENGTH * CH;
    
    // Channels are de-interleaved on load so each u8 lane is one source pixel;
    // SumsOf4 then adds each horizontal run of 4 pixels into one u32 lane. Every
    // N source pixels of a row therefore produce N/4 destination pixels (d8_dst).
    const ScalableTag<uint8_t> d8;
    const RepartitionToWideX2<decltype(d8)> d32;
    const Rebind<uint8_t, decltype(d32)> d8_dst;
    const size_t N = Lanes(d8);
    // Vectors must hold whole 4-pixel runs; only HWY_SCALAR (1 lane) does not,
    // and there the scalar loop below does all of the work.
    const bool simd = N % 4 == 0;
    
    const auto round = Set(d32, 8);         // Round by adding 8 (then divide by 16)
    
    for (auto y = 0; y < 64; ++y) {
        const auto row0 = src + (4 * y) * stride;
        const auto row1 = src + (4 * y + 1) * stride;
        const auto row2 = src + (4 * y + 2) * stride;
        const auto row3 = src + (4 * y + 3) * stride;
        auto orow = dst + (y + o_y) * stride + o_x * CH;
        
        size_t x = 0;   // Source pixel column
        for (; simd && x + N <= TILE_PIX_LENGTH; x += N) {
            // Per-channel sums of each 4x4 block (one u32 lane per destination pixel)
            auto s0 = Zero(d32), s1 = Zero(d32), s2 = Zero(d32), s3 = Zero(d32);
            for (const auto row : {row0, row1, row2, row3}) {
                Vec<decltype(d8)> c0, c1, c2, c3;
                LoadPixelsHelper<CH>(d8, row + x * CH, c0, c1, c2, c3);
                s0 = Add(s0, SumsOf4(c0));
                s1 = Add(s1, SumsOf4(c1));
                s2 = Add(s2, SumsOf4(c2));
                if constexpr (CH == 4) s3 = Add(s3, SumsOf4(c3));
            }
            StorePixelsHelper<CH>(d8_dst,
                                  DemoteTo(d8_dst, ShiftRight<4>(Add(s0, round))),
                                  DemoteTo(d8_dst, ShiftRight<4>(Add(s1, round))),
                                  DemoteTo(d8_dst, ShiftRight<4>(Add(s2, round))),
                                  DemoteTo(d8_dst, ShiftRight<4>(Add(s3, round))),
                                  orow + (x / 4) * CH);
        }
        
        // Scalar cleanup (remainder of the row when N does not divide it evenly)
        for (; x < TILE_PIX_LENGTH; x += 4) {
            for (int c = 0; c < CH; ++c) {
                uint16_t sum = 0;
                for (const auto row : {row0, row1, row2, row3})
                    for (size_t k = 0; k < 4; ++k)
                        sum += row[(x + k) * CH + c];
                orow[(x / 4) * CH + c] = static_cast<uint8_t>((sum + 8) >> 4);
            }
        }
    }
}
// Aliases for dynamic dispatch
HWY_API void DOWNSAMPLE_INTO_TILE_4X_AVG_3
 (const uint8_t* HWY_RESTRICT src, uint8_t* HWY_RESTRICT dst, const uint16_t s_y,const uint16_t s_x) {
    DOWNSAMPLE_INTO_TILE_4X_AVG<3>(src, dst, s_y, s_x);
}

HWY_API void DOWNSAMPLE_INTO_TILE_4X_AVG_4
 (const uint8_t* HWY_RESTRICT src, uint8_t* HWY_RESTRICT dst, const uint16_t s_y, const uint16_t s_x) {
    DOWNSAMPLE_INTO_TILE_4X_AVG<4>(src, dst, s_y, s_x);
}
} // END HWY_NAMESPACE
 HWY_AFTER_NAMESPACE();

#if HWY_ONCE
Buffer Convert_tile_format (const Buffer &src, Format s_fmt, Format d_fmt, const Buffer& __dst)
{
    // Destination buffer (assign it to the optional)
    Buffer dst              = __dst;
    
    // No need to convert...
    if (s_fmt == d_fmt) {
        if (!dst || dst->capacity() < src->size())
            dst = src;
        else memcpy(dst->data(), src->data(), src->size());
        dst->set_size(src->size());
        return dst;
    }
    
    // Bitmask the tasks that will be needed
    // ..this will grow. Make sure to account below...
    enum ConversionBits {
        TASK_EXPAND_ALPHA   = 0x01,
        TASK_STRIP_ALPHA    = 0x02,
        TASK_SWAP_0_2       = 0x10,
    };
    // Bitmask
    uint32_t tasks          = 0;
    // Source and dst bits per pixel
    uint8_t s_bpp = 0, d_bpp = 0;
    
    // Tile Sizing
    switch (s_fmt) {
        case FORMAT_UNDEFINED:
            throw std::runtime_error
            ("Convert_tile_format failed due to undefined source format");
        case FORMAT_B8G8R8:
        case FORMAT_R8G8B8:
            s_bpp = 3;
            break;
        case FORMAT_B8G8R8A8:
        case FORMAT_R8G8B8A8:
            s_bpp = 4;
            break;
        default: throw std::runtime_error
            ("Convert_tile_format unsupported bits-per-pixel source format (not 3 or 4 bpp)");
    }
    switch (d_fmt) {
        case FORMAT_UNDEFINED:
            throw std::runtime_error
            ("Convert_tile_format failed due to undefined source format");
        case FORMAT_B8G8R8:
        case FORMAT_R8G8B8:
            d_bpp = 3;
            break;
        case FORMAT_B8G8R8A8:
        case FORMAT_R8G8B8A8:
            d_bpp = 4;
            break;
        default: throw std::runtime_error
            ("Convert_tile_format unsupported bits-per-pixel destination format (not 3 or 4 bpp)");
    } if (!dst || dst->capacity() < TILE_PIX_AREA * d_bpp)
        dst = Create_strong_buffer(TILE_PIX_AREA * d_bpp);
    
    // Task Selection
    // 1) Task number of channels
    switch (s_fmt) {
        case FORMAT_UNDEFINED:
        case FORMAT_B8G8R8:
        case FORMAT_R8G8B8:
            switch (d_fmt) {
                case FORMAT_B8G8R8A8:
                case FORMAT_R8G8B8A8:
                    tasks |= TASK_EXPAND_ALPHA;
                default:break;
            } break;
        case FORMAT_B8G8R8A8:
        case FORMAT_R8G8B8A8:
            switch (d_fmt) {
                case FORMAT_B8G8R8:
                case FORMAT_R8G8B8:
                    tasks |= TASK_STRIP_ALPHA;
                default:break;
            }
    }
    // 2) Task channel ordering
    switch (s_fmt) {
        case FORMAT_UNDEFINED:
        case FORMAT_B8G8R8:
        case FORMAT_B8G8R8A8:
            switch (d_fmt) {
                case FORMAT_R8G8B8:
                case FORMAT_R8G8B8A8:
                    tasks |= TASK_SWAP_0_2;
                default:break;
            } break;
        case FORMAT_R8G8B8:
        case FORMAT_R8G8B8A8:
            switch (d_fmt) {
                case FORMAT_B8G8R8:
                case FORMAT_B8G8R8A8:
                    tasks |= TASK_SWAP_0_2;
                default:break;
            }
    }
    assert(tasks && "Convert_tile_format undefined conversion.");

    // Resizing functions
    assert((tasks & (TASK_EXPAND_ALPHA|TASK_STRIP_ALPHA)) != (TASK_EXPAND_ALPHA|TASK_STRIP_ALPHA) &&
           "Convert_tile_format cannot TASK_EXPAND_ALPHA and TASK_STRIP_ALPHA");
    if (tasks & TASK_EXPAND_ALPHA) {
        HWY_STATIC_DISPATCH(EXPAND_TILE_ADD_ALPHA_8bit)((uint8_t*)src->data(), (uint8_t*)dst->data());
    } else if (tasks & TASK_STRIP_ALPHA) {
        HWY_STATIC_DISPATCH(SHRINK_TILE_RM_ALPHA_8bit)((uint8_t*)src->data(), (uint8_t*)dst->data());
    } else {
        if (src->data() != dst->data())
            memcpy(dst->data(), src->data(), dst->size());
    }
    // byte-swap functions
    if (tasks & TASK_SWAP_0_2) {
        switch (d_bpp) {
            case 3:
                HWY_STATIC_DISPATCH(SWAP_TILE_3_CHANNELS_0_2_8bit)((uint8_t*)dst->data());
                break;
            case 4:
                HWY_STATIC_DISPATCH(SWAP_TILE_4_CHANNELS_0_2_8bit)((uint8_t*)dst->data());
                break;
            default: break;
        }
    }
    
    // Ensure the size is correct before returning
    dst->set_size(TILE_PIX_AREA * d_bpp);
    return dst;
}
namespace N = hwy::HWY_NAMESPACE;
void Downsample_into_tile_2x_avg(const Buffer &src, const Buffer &dst,
                                 uint16_t sub_y, uint16_t sub_x, uint8_t channels)
{
    assert (src->size() <= TILE_PIX_AREA * channels && "Insufficiently sized source tile for 2x downsample");
    assert (dst->size() <= TILE_PIX_AREA * channels && "Insufficiently sized destination tile for 2x downsample");
    switch (channels) {
        case 3: return HWY_STATIC_DISPATCH(DOWNSAMPLE_INTO_TILE_2X_AVG_3)
            (static_cast<uint8_t*>(src->data()),static_cast<uint8_t*>(dst->data()),sub_y, sub_x);
        case 4: return HWY_STATIC_DISPATCH(DOWNSAMPLE_INTO_TILE_2X_AVG_4)
            (static_cast<uint8_t*>(src->data()),static_cast<uint8_t*>(dst->data()),sub_y, sub_x);
        default: throw std::runtime_error("Downsample_into_tile_2x_avg Unsupported channel count");
    }
}
void Downsample_into_tile_4x_avg(const Buffer &src, const Buffer &dst,
                                             uint16_t sub_y, uint16_t sub_x, uint8_t channels)
{
    assert (src->size() <= TILE_PIX_AREA * channels && "Insufficiently sized source tile for 4x downsample");
    assert (dst->size() <= TILE_PIX_AREA * channels && "Insufficiently sized destination tile for 4x downsample");
    switch (channels) {
        case 3: return HWY_STATIC_DISPATCH(DOWNSAMPLE_INTO_TILE_4X_AVG<3>)
            (static_cast<uint8_t*>(src->data()),static_cast<uint8_t*>(dst->data()),sub_y, sub_x);
        case 4: return HWY_STATIC_DISPATCH(DOWNSAMPLE_INTO_TILE_4X_AVG<4>)
            (static_cast<uint8_t*>(src->data()),static_cast<uint8_t*>(dst->data()),sub_y, sub_x);
        default: throw std::runtime_error("Downsample_into_tile_4x_avg Unsupported channel count");
    }
}
} // SIMD
} // Iris
# endif // HWY_ONCE

