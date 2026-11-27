/**
 * @file IrisSlideTypes.hpp
 * @author Ryan Landvater (RyanLandvater@gmail.com)
 * @brief  Iris Slide C++ Types Predeclarations
 * @version 2025.1.0
 * @date 2024-01-9
 * @copyright Copyright (c) 2022-25 Ryan Landvater
 *
 * Iris Slide: the slide file decoder and encoder
 * Part of the Iris Whole Slide Imaging Project
 *
 * Codec elements are defined here for use  in the implementation of the
 * Iris Slide for GPU optimized decompression routines and to allow for
 * integration of decompression pipelines with the Iris Core viewer engine.
 *
 * The codec can be optionally used outside of / without the core engine
 * for non-Iris implementations that still wish access to the compression
 * and (or) .iris file extension structure
 *
 * Use of Iris Slide and the Iris File Extension (.iris) specification follows the
 * CC BY-ND 4.0 License outlined in the Iris Digital Slide Extension File Structure Techinical Specification
 * https://creativecommons.org/licenses/by-nd/4.0/
 *
 */

#ifndef IrisSlideTypes_h
#define IrisSlideTypes_h
// Codec types rely upon the Iris Core types
// and the Core types header must be included.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include "IrisTypes.hpp"
namespace   Iris {
using       Instance        = std::shared_ptr<struct Instance_t>;
using       Device          = std::shared_ptr<struct Device_t>;
using       Queue           = std::shared_ptr<struct Queue_t>;
} // END IRIS CORE PREDECLARATIONS

namespace IrisSlide {
/**
 * @brief Why a codec call failed: the code of every failure Result the codec
 * returns, under `Iris::Domain::IRIS_SLIDE`, read back with
 * `result.code_as<IrisSlide::SlideError>()`. The message carries the offending
 * value. A failure IFE reports while the codec reads a file keeps IFE's own
 * domain and code (`Iris::File::FileError`).
 */
enum class SlideError : std::uint32_t {
    FILE_NOT_OPENED,                ///< the file could not be mapped or opened
    SLIDE_INFO_UNREADABLE,          ///< the slide's extent and metadata could not be read
    ASSOCIATED_IMAGE_UNREADABLE,    ///< an associated image's description could not be read
    ANNOTATION_NOT_WRITTEN,         ///< an annotation could not be added to the slide
    ANNOTATIONS_UNREADABLE,         ///< the slide's annotations could not be listed
    ANNOTATION_WRITE_UNSUPPORTED,   ///< writing an annotation to an open slide is not implemented
    ENCODER_FAILED,                 ///< the encoder stopped; the message says why
    ENCODER_NO_TILES,               ///< an active encoder is tracking no tiles
    ENCODER_SHUT_DOWN,              ///< the encoder is shut down and cannot be reset
    ENCODER_ALREADY_FAILED,         ///< the encoder had already stopped with an error
    ENCODER_STATUS_UNKNOWN,         ///< the encoder's status is not one this build knows
    REMOTE_NOT_IRIS_FILE,           ///< a hosted file is too short to be an Iris file
    REMOTE_RANGES_UNSUPPORTED,      ///< the host does not serve ranged reads
    REMOTE_FETCH_FAILED,            ///< the hosted file's blocks could not be fetched
    REMOTE_VALIDATION_FAILED,       ///< a hosted file's block failed validation
};
/**
 * @brief Compression Context used for performing CPU/GPU image codec implementations
 * 
 * The context provides a centrally accessible wrapper around image compression codecs that
 * exist on the CPU or that may exist on the GPU in the form of hardware decoders 
 * or the Iris Slide compression system. The context may be created using Iris GPU Vulkan
 * instance and device wrappers for direct pipeline integration with a rendering system.
 */
using       Context         = std::shared_ptr<class Context_t>;

/**
 * @brief Iris-encoded slide convenience wrapper. This CANNOT wrap other
 * file types (such as OpenSlide files).
 * 
 */
using       Slide           = std::shared_ptr<class Slide_t>;

/**
 * @brief Local temporary WSI file cache with a variety of uses. See description.
 * 
 * The Cache is a multi-purpose IFE structured temporary file. It is not linked 
 * with the underlying OS to ensure complete resource recycling if there's ever a
 * program crash. Data may be dumped into a cache in either a compressed or decompressed
 * form. For example, a cache may be set as CACHE_ENCODING_JPEG and raw data can 
 * be put into the cache using CACHE_ACCESS_COMPRESS_TILE for pixel data or 
 * CACHE_ACCESS_DIRECT_NO_CODEC for already compressed byte streams (say from a server).
 * Regardless, either may be accessed with CACHE_ACCESS_DECOMPRESS_TILE to get
 * pixel data. 
 * 
 * WARNING: It is up to you to ensure the byte stream type matches the CACHE_ACCESS
 * flag when writing to the cache.  
 */
using       Cache           = std::shared_ptr<class Cache_t>;

/**
 * @brief Encodes a single whole slide image from another WSI format or Cache at a time
 * using the maximal CPU cores. 
 * 
 * The encoder reads a slide and writes it as an Iris file (IFE): made by create_encoder from an
 * EncoderDesc, dispatched once, followed with encoder_progress.
 */
using       Encoder         = std::shared_ptr<class Encoder_t>;

// Additional types
using       Version         = Iris::Version;
using       Result          = Iris::Result;
using       Buffer          = Iris::Buffer;
using       Extent          = Iris::Extent;
using       Format          = Iris::Format;
using       AnnotationTypes = Iris::AnnotationTypes;
using       Annotation      = Iris::Annotation;
using       Annotations     = Iris::Annotations;
using       AnnotationGroup = Iris::AnnotationGroup;
using       BYTE            = Iris::BYTE;
using       Mutex           = std::mutex;
using       Offset          = uint64_t;
using       Size            = uint64_t;
struct IRIS_EXPORT ContextCreateInfo {
    Iris::Device    device                  = nullptr;
};
/// Form of encoding used to generate compressed tile bytestreams
enum IRIS_EXPORT Encoding : uint8_t {
    TILE_ENCODING_UNDEFINED                 = 0,
    TILE_ENCODING_IRIS                      = 1,
    TILE_ENCODING_JPEG                      = 2,
    /// A bare AV1 sequence in the Low Overhead Bitstream Format (AV1 bitstream
    /// specification, "Low overhead bitstream format"), with no container: one
    /// shown frame per focal plane, so a single-plane tile is one key frame.
    /// Replaced AVIF tiles at value 3 (decided 2026-11-25, with IFE's matching
    /// spec change); no .iris file in use carried AVIF tiles, so the value was
    /// reassigned instead of retired. Associated images keep IMAGE_ENCODING_AVIF.
    TILE_ENCODING_AV1                       = 3,
    /// GPU block-compressed UASTC LDR 4x4 (Basis Universal), stored bare: one
    /// Zstandard frame whose content is the tile's 16-byte blocks in raster
    /// order, one focal plane and one mip level. Transcoded by the reader to
    /// ASTC 4x4 (lossless) or BC7 instead of decoded to pixels. Added
    /// 2026-11-25 with IFE's matching spec change (tile_encodings 1.1).
    TILE_ENCODING_UASTC                     = 4,
    /// GPU block-compressed XUASTC LDR (Basis Universal), stored bare: one
    /// XUASTC LDR bitstream, one focal plane and one mip level. Its block
    /// footprint and whether its bitstream is fixed across Basis Universal
    /// versions are open in IFE's specification. Added 2026-11-25.
    TILE_ENCODING_XUASTC                    = 5,
    TILE_ENCODING_DEFAULT                   = TILE_ENCODING_JPEG //v2025.1 - will change
};
// Values equal IFE's metadata_formats (spec/ife_constants.json). METADATA_I2S
// promises the pairs identify no individual, so free text has its own value
// and never claims I2S (IFE 1.1 erratum: 1.0 aliased the two).
enum IRIS_EXPORT MetadataType : uint8_t {
    METADATA_UNDEFINED                      = 0,
    METADATA_I2S                            = 1,
    METADATA_DICOM                          = 2,
    METADATA_FREE_TEXT                      = 3,
};
struct IRIS_EXPORT Attributes :
public std::unordered_map<std::string, std::u8string> {
    MetadataType    type                    = METADATA_UNDEFINED;
    uint16_t        version                 = 0;
};
enum IRIS_EXPORT ImageEncoding : uint8_t {
    IMAGE_ENCODING_UNDEFINED                = 0,
    IMAGE_ENCODING_PNG                      = 1,
    IMAGE_ENCODING_JPEG                     = 2,
    IMAGE_ENCODING_AVIF                     = 3,
    IMAGE_ENCODING_DEFAULT                  = IMAGE_ENCODING_JPEG // v2025.1 - will change
};
enum IRIS_EXPORT ImageOrientation : uint16_t {
    ORIENTATION_0                           = 0x0000, // Half-precision 0.0
    ORIENTATION_90                          = 0x55A0, // Half-precision 90.0
    ORIENTATION_180                         = 0x59A0, // Half-precision 180.0
    ORIENTATION_270                         = 0x5C38, // Half-precision 270.0
    ORIENTATION_minus_90                    = ORIENTATION_270,
    ORIENTATION_minus_180                   = ORIENTATION_180,
    ORIENTATION_minus_270                   = ORIENTATION_90,
};
struct IRIS_EXPORT Image {
    using           Orientation             = ImageOrientation;
    std::string     title;
    Buffer          bytes                   = nullptr;
    uint32_t        width                   = 0;
    uint32_t        height                  = 0;
    Format          format                  = Iris::FORMAT_UNDEFINED;
    Orientation     orientation             = ORIENTATION_0;
};
/// Slide metadata containing information about the Iris File Extension slide file.
struct IRIS_EXPORT Metadata {
    /// List of associated / ancillary image labels describing the associated image (*eg. Label, Thumbnail*)
    using ImageLabels                       = std::set<std::string>;
    /// List of annotation identifiers within the slide (unique annotation identifiers)
    using AnnotationIDs                     = std::set<uint32_t>;
    /// List of annotation grouping names describing the group of annotations (*eg. handwriting; nuclei*)
    using AnnotationGroups                  = std::set<std::string>;
    
    /// Iris Slide Version used to encode the file, if Iris Slide encoded it. Otherwise leave this blank
    Version         codec                   = {0,0,0};
    /// Metadata Attributes containing key-value pairs
    Attributes      attributes;
    /// List of associated images
    ImageLabels     associatedImages;
    /// ICC color profile, if contained within the file
    std::string     ICC_profile;
    /// Iris slide annotations
    AnnotationIDs   annotations;
    /// Iris slide annotation groups
    AnnotationGroups annotationGroups;
    /// Microns per pixel (um per pixel) at layer 0 / the lowest resolution layer (0.f if no um scale)
    float           micronsPerPixel         = 0.f;
    /// Magnification coefficient used to convert scale to physical microscopic magnfication (0.f if not included)
    float           magnification           = 0.f;
};

/// Information needed to open a local Iris Encoded (.iris) file with optional defaults assigned.
struct IRIS_EXPORT SlideOpenInfo {
    std::string     filePath;
    Context         context                 = nullptr;
    bool            writeAccess             = false;
};

/// Iris Encoded File (.iris) metadata information
struct IRIS_EXPORT SlideInfo {
    Format          format                  = Iris::FORMAT_UNDEFINED;
    Encoding        encoding                = TILE_ENCODING_UNDEFINED;
    Extent          extent;
    Metadata        metadata;
};
struct IRIS_EXPORT SlideTileReadInfo {
    Slide           slide                   = NULL;
    uint32_t        layerIndex              = 0;
    uint32_t        tileIndex               = 0;
    Buffer          optionalDestination     = NULL;
    Format          desiredFormat           = Iris::FORMAT_R8G8B8A8;
};
struct IRIS_EXPORT AssociatedImageInfo {
    using           Encoding                = ImageEncoding;
    using           Orientation             = ImageOrientation;
    
    std::string     imageLabel;
    uint32_t        width                   = 0;
    uint32_t        height                  = 0;
    Encoding        encoding                = IMAGE_ENCODING_UNDEFINED;
    Format          sourceFormat            = Iris::FORMAT_UNDEFINED;
    Orientation     orientation             = ORIENTATION_0;
};
struct IRIS_EXPORT AssociatedImageReadInfo {
    Slide           slide                   = NULL;
    std::string     imageLabel;
    Buffer          optionalDestination     = NULL;
    Format          desiredFormat           = Iris::FORMAT_R8G8B8A8;
};
// MARK: - CACHE TYPE DEFINITIONS
enum IRIS_EXPORT CacheEncoding : uint8_t {
    CACHE_ENCODING_UNDEFINED                = 0,
    CACHE_ENCODING_IRIS                     = TILE_ENCODING_IRIS,
    CACHE_ENCODING_JPEG                     = TILE_ENCODING_JPEG,
    CACHE_ENCODING_AV1                      = TILE_ENCODING_AV1,
    // Cache values stop mirroring tile values here: LZ and no compression hold
    // 4 and 5, which TILE_ENCODING_UASTC and TILE_ENCODING_XUASTC also hold.
    CACHE_ENCODING_LZ,
    CACHE_ENCODING_NO_COMPRESSION
};
enum IRIS_EXPORT CacheDataAccess {
    CACHE_ACCESS_COMPRESS_TILE              = 0, /* Apply codec to compress tile bytes */
    CACHE_ACCESS_DECOMPRESS_TILE            = 0, /* Apply codec to decompress tile bytes */
    CACHE_ACCESS_DIRECT_NO_CODEC            = 1, /* Do not apply any compression codec */
};
struct IRIS_EXPORT CacheCreateInfo {
    bool            unlink                  = true; /* Unlink to automatically free on close*/
    Context         context                 = nullptr;
    CacheEncoding   encodingType            = CACHE_ENCODING_UNDEFINED;
};
struct IRIS_EXPORT CacheTileReadInfo {
    Cache           cache                   = NULL;
    uint32_t        layerIndex              = 0;
    uint32_t        tileIndex               = 0;
    Buffer          optionalDestination     = NULL;
    Format          desiredFormat           = Iris::FORMAT_R8G8B8A8;
    CacheDataAccess accessType              = CACHE_ACCESS_DECOMPRESS_TILE;
};
struct IRIS_EXPORT CacheStoreInfo {
    Cache           cache                   = NULL;
    uint32_t        layerIndex              = 0;
    uint32_t        tileIndex               = 0;
    Buffer          source                  = NULL;
    CacheDataAccess accessType              = CACHE_ACCESS_COMPRESS_TILE;
};

// MARK: - ENCODER TYPE DEFINITIONS
/// Encoding quality [0-100]: JPEG quality for JPEG tiles and images, and the
/// encoder's quality scale for AV1 tiles and AVIF associated images
using               Quality                 = uint16_t;
constexpr Quality   QUALITY_DEFAULT         = 90;

/// Available Chroma-Subsampling options. More information at:
/// https://en.wikipedia.org/wiki/Chroma_subsampling
enum IRIS_EXPORT Subsampling : uint8_t {
    SUBSAMPLE_444, // Lossless
    SUBSAMPLE_422,
    SUBSAMPLE_420,
    SUBSAMPLE_DEFAULT                       = SUBSAMPLE_422,
};

/// What to encode, where and how (EncoderSurface_24.1, decided 2026-11-27): the rebuilt encoder reads
/// the slide at `source`, derives every lower layer by a 2x bicubic reduction, and writes
/// <source stem>.<encoding>.iris into `destination`. Made into an Encoder by create_encoder.
struct IRIS_EXPORT EncoderDesc {
    std::filesystem::path source;                         ///< the slide: a DICOM whole-slide instance (JPEG, JPEG 2000 or HTJ2K frames)
    std::filesystem::path destination;                    ///< the folder the .iris file is written to
    Encoding        encoding      = TILE_ENCODING_JPEG;   ///< the IFE tile encoding written
    Quality         quality       = QUALITY_DEFAULT;      ///< JPEG quality, 1-100
    Subsampling     subsampling   = SUBSAMPLE_DEFAULT;    ///< JPEG chroma subsampling
    std::uint64_t   memory_budget = 0;                    ///< bytes held at once; 0 is a quarter of physical memory
    std::uint32_t   concurrency   = std::max(1u, std::thread::hardware_concurrency());   ///< worker threads
    /// The scale between written layers: 2, 4, 8 … Layers are derived 2x at a time and every
    /// log2(layer_step)-th is written, down to one that fits one tile.
    std::uint32_t   layer_step    = 2;
    /// The slide's attribute pairs as the caller states them: key text, value text, their format
    /// and that format's version, written as the slide's one attributes block. The encoder checks
    /// only what IFE requires of the bytes (ASCII keys, UTF-8 values, sizes) and refuses a format
    /// other than METADATA_I2S or version 0; whether a key belongs to I2S is I2S's check. Nothing
    /// is read from the source's own attributes.
    Attributes      attributes;
};

/// An encoder is used once: dispatched from READY, it ends DONE, FAILED or INTERRUPTED.
enum class IRIS_EXPORT EncoderState : std::uint8_t {
    READY,
    ACTIVE,
    DONE,
    FAILED,
    INTERRUPTED,
};

/// A snapshot of an encode (encoder_progress).
struct IRIS_EXPORT EncoderProgress {
    EncoderState          state               = EncoderState::READY;
    float                 fraction            = 0.f;   ///< tiles written over tiles in the file
    std::filesystem::path file;                        ///< the .iris file it writes
    std::string           error;                       ///< why it FAILED, naming the offending value
    std::uint64_t         resident_bytes      = 0;     ///< tile bytes held now
    std::uint64_t         peak_resident_bytes = 0;     ///< the most ever held
    std::uint64_t         memory_budget       = 0;     ///< the budget it keeps to
};
} // END IRIS SLIDE NAMESPACE
#endif /* IrisSlideTypes_h */
