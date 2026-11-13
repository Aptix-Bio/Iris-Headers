/**
 * @file IrisCodecTypes.hpp
 * @author Ryan Landvater (RyanLandvater@gmail.com)
 * @brief  Iris Codec C++ Types Predeclarations
 * @version 2025.1.0
 * @date 2024-01-9
 * @copyright Copyright (c) 2022-25 Ryan Landvater
 *
 * The Iris Codec Compression Module
 * Part of the Iris Whole Slide Imaging Project
 *
 * Codec elements are defined here for use  in the implementation of the
 * Iris Codec for GPU optimized decompression routines and to allow for
 * integration of decompression pipelines with the Iris Core viewer engine.
 *
 * The codec can be optionally used outside of / without the core engine
 * for non-Iris implementations that still wish access to the compression
 * and (or) .iris file extension structure
 *
 * Use of Iris Codec and the Iris File Extension (.iris) specification follows the
 * CC BY-ND 4.0 License outlined in the Iris Digital Slide Extension File Structure Techinical Specification
 * https://creativecommons.org/licenses/by-nd/4.0/
 *
 */

#ifndef IrisCodecTypes_h
#define IrisCodecTypes_h
// Codec types rely upon the Iris Core types
// and the Core types header must be included.
#include "IrisTypes.hpp"
namespace   Iris {
using       Instance        = std::shared_ptr<struct Instance_t>;
using       Device          = std::shared_ptr<struct Device_t>;
using       Queue           = std::shared_ptr<struct Queue_t>;
} // END IRIS CORE PREDECLARATIONS

namespace IrisCodec {
/**
 * @brief Why a codec call failed: the code of every failure Result the codec
 * returns, under `Iris::Domain::IRIS_CODEC`, read back with
 * `result.code_as<IrisCodec::CodecError>()`. The message carries the offending
 * value. A failure IFE reports while the codec reads a file keeps IFE's own
 * domain and code (`Iris::File::FileError`).
 */
enum class CodecError : std::uint32_t {
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
 * or the Iris Codec compression system. The context may be created using Iris GPU Vulkan
 * instance and device wrappers for direct pipeline integration with a rendering system.
 */
using       Context         = std::shared_ptr<class Context_t>;

/**
 * @brief Iris Codec Encoded slide convenience wrapper. This CANNOT wrap other
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
 * The encoder is an encapsulated encoding routine with multi-threaded WSI processing.
 * The encoder can encode a new Iris WSI file (IFE) from a vendor slide file or may
 * create a new WSI file de novo from a set of cached slide tiles (either compressed
 * or decompressed based upon Cache settings).
 * 
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
    TILE_ENCODING_AVIF                      = 3,
    TILE_ENCODING_DEFAULT                   = TILE_ENCODING_JPEG //v2025.1 - will change
};
enum IRIS_EXPORT MetadataType : uint8_t {
    METADATA_UNDEFINED                      = 0,
    METADATA_I2S                            = 1,
    METADATA_DICOM                          = 2,
    METADATA_FREE_TEXT                      = METADATA_I2S,
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
    
    /// Iris Codec Version used to encode the file, if Iris Codec encoded it. Otherwise leave this blank
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
    CACHE_ENCODING_AVIF                     = TILE_ENCODING_AVIF,
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
/// Image encoding quality [0-100]. Maps to JPEG and AVIF quality standard
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

/// Current status of the encoder object.
enum IRIS_EXPORT EncoderStatus {
    ENCODER_INACTIVE,
    ENCODER_ACTIVE,
    ENCODER_ERROR,
    ENCODER_SHUTDOWN,
};

/// If encoder derive enabled
struct IRIS_EXPORT EncoderDerivation {
    enum Layers {
        ENCODER_DERIVE_UNDEFINED,
        ENCODER_DERIVE_USE_SOURCE,
        ENCODER_DERIVE_2X_LAYERS,           // Will generate ~8 layers (256px->128,64,32,16,8,4,2,1)
        ENCODER_DERIVE_4X_LAYERS,           // Will generate ~4 layers (256px->64,16,4,1)
    }               layers                  = ENCODER_DERIVE_2X_LAYERS;
    enum Method {
        ENCODER_DOWNSAMPLE_UNDEFINED,
        ENCODER_DOWNSAMPLE_AVERAGE,         // Downsampling by simple averaging
        ENCODER_DOWNSAMPLE_SHARPEN,         // Downsampling while preserving high-freqency info
    }               method                  = ENCODER_DOWNSAMPLE_AVERAGE;
};
struct IRIS_EXPORT EncodeSlideInfo {
    using Derivation                        = EncoderDerivation;
    std::string     srcFilePath;
    std::string     dstFilePath;
    Format          srcFormat               = Iris::FORMAT_UNDEFINED;
    Encoding        desiredEncoding         = TILE_ENCODING_UNDEFINED;
    Format          desiredFormat           = Iris::FORMAT_UNDEFINED;
    bool            anonymize               = false;
    unsigned        concurrency             = std::thread::hardware_concurrency();
    Context         context                 = NULL;
    Derivation*     derivation              = NULL;
};
struct IRIS_EXPORT EncodeStreamInfo {
    using Derivation                        = EncoderDerivation;
    std::string     dstFilePath;
    uint32_t        width                   = 0;
    uint32_t        height                  = 0;
    Format          srcFormat               = Iris::FORMAT_UNDEFINED;
    Encoding        desiredEncoding         = TILE_ENCODING_UNDEFINED;
    Format          desiredFormat           = Iris::FORMAT_UNDEFINED;
    Context         conxtext                = NULL;
    Derivation      derivation;
};
struct IRIS_EXPORT EncoderProgress {
    EncoderStatus   status                  = ENCODER_INACTIVE;
    float           progress                = 0.f;
    std::string     dstFilePath;
    std::string     errorMsg;
};
} // END IRIS CODEC NAMESPACE
#endif /* IrisCodecTypes_h */
