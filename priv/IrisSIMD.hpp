/**
 * @file IrisSIMD.hpp
 * @author ryanlandvater (ryanlandvater [at] gmail [dot] com)
 * @brief Iris SIMD operations header file using
 * google highway library as the SIMD backend.
 * @version 0.1
 * @date 2023-10-05
 * 
 * @copyright Copyright (c) 2023-2025
 * 
 */
#ifndef IrisSIMD_hpp
#define IrisSIMD_hpp

// Self-contained: compiles with no other header first (StandaloneHeaderBuild_1.1).
#include <cstdint>
#include <IrisTypes.hpp>

namespace Iris {
namespace SIMD {
/**
 * @brief Convert a tile pixel buffer from one pixel format to another.
 * @note This can accept the same source and destination buffer for
 * in-place conversion.
 * 
 * @param tile_pixel_buffer The tile pixel buffer to convert.
 * @param source_format The Iris pixel format of the source tile pixel buffer.
 * @param desired_format The desired Iris pixel format of the destination tile pixel buffer.
 * @param optional_destination An optional destination buffer to write the converted tile pixel data to.
 * If this is not provided, a new buffer will be created. This can be the same as tile_pixel_buffer for in-place conversion.
 * @return Buffer containing the converted tile pixel data, which may be the same as optional_destination.
 *
 */
Buffer Convert_tile_format          (const Buffer& tile_pixel_buffer,
                                     Format source_format,
                                     Format desired_format,
                                     const Buffer& optional_destination = NULL);
/**
 * @brief Downsample a tile pixel buffer by 2x using an average filter.
 * 
 * @param src source tile pixel buffer to downsample.
 * @param dst destination tile pixel buffer to write the downsampled data to.
 * @param sub_y sub-region y coordinate in dst buffer range [0,1] (corresponding to 128 pixel regions)
 * @param sub_x sub-region x coordinate in dst buffer range [0,1] (corresponding to 128 pixel regions)
 * @param channels number of channels in the src tile pixel buffer.
 */
void Downsample_into_tile_2x_avg    (const Buffer& src, const Buffer& dst,
                                     uint16_t sub_y, uint16_t sub_x, uint8_t channels);
/**
 * @brief Downsample a tile pixel buffer by 4x using an average filter.
 * 
 * @param src source tile pixel buffer to downsample.
 * @param dst destination tile pixel buffer to write the downsampled data to.
 * @param sub_y sub-region y coordinate in dst buffer range [0,3] (corresponding to 64 pixel regions)
 * @param sub_x sub-region x coordinate in dst buffer range [0,3] (corresponding to 64 pixel regions)
 * @param channels number of channels in the src tile pixel buffer.
 */
void Downsample_into_tile_4x_avg    (const Buffer& src, const Buffer& dst,
                                     uint16_t sub_y, uint16_t sub_x, uint8_t channels);
/**
 * @brief Downsample a tile pixel buffer by 2x using an integer unsharp mask.
 * 
 * @param src source tile pixel buffer to downsample. 
 * @param dst destination tile pixel buffer to write the downsampled data to.
 * @param sub_y sub-region y coordinate in dst buffer range [0,1] (corresponding to 128 pixel regions)
 * @param sub_x sub-region x coordinate in dst buffer range [0,1] (corresponding to 128 pixel regions)
 * @param channels number of channels in the src tile pixel buffer.
 */
void Downsample_into_tile_2x_sharp  (const Buffer& src, const Buffer& dst,
                                     uint16_t sub_y, uint16_t sub_x, uint8_t channels);
/**
 * @brief Downsample a tile pixel buffer by 4x using an integer unsharp mask.
 * 
 * @param src source tile pixel buffer to downsample.
 * @param dst destination tile pixel buffer to write the downsampled data to.
 * @param sub_y sub-region y coordinate in dst buffer range [0,3] (corresponding to 64 pixel regions)
 * @param sub_x sub-region x coordinate in dst buffer range [0,3] (corresponding to 64 pixel regions)
 * @param channels number of channels in the src tile pixel buffer.
 */
void Downsample_into_tile_4x_sharp  (const Buffer& src, const Buffer& dst,
                                     uint16_t sub_y, uint16_t sub_x, uint8_t channels);
} // END SIMD NAMESPACE
} // END IRIS NAMESPACE
#endif /* IrisSIMD_hpp */
