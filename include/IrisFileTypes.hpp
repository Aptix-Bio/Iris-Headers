/**
 * @file IrisFileTypes.hpp
 * @brief The shared vocabulary of what an Iris file (.iris) holds: its tile and
 * associated-image encodings, its metadata and attributes, an associated image's
 * description, and the offset and size types of its byte ranges.
 * @copyright Copyright (c) 2022-26 Ryan Landvater
 *
 * Iris-File-Extension writes and reads these types and Iris-Slide encodes and
 * decodes them, so they live in namespace Iris, where neither library depends on
 * the other for them (FileTypesShared_3.5, decided 2026-11-28). The enumerator
 * values equal the Iris File Extension specification's (spec/ife_constants.json)
 * value for value; a renumbered value is a breaking change to the file format
 * (CONTRIBUTING.md, rule 6). The earlier IrisSlide names in IrisSlideTypes.hpp
 * alias these until every consumer has moved to them.
 */

#ifndef IrisFileTypes_h
#define IrisFileTypes_h
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include "IrisTypes.hpp"

// RTM Format_08
namespace Iris {
/// A byte offset into an Iris file.
using       Offset          = uint64_t;
/// A byte count within an Iris file.
using       Size            = uint64_t;

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
/// A slide's attribute pairs (key text, value text), with their format and the format's version.
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
/// An associated image's description: its label, dimensions, encoding, source format and orientation.
struct IRIS_EXPORT AssociatedImageInfo {
    using           Encoding                = ImageEncoding;
    using           Orientation             = ImageOrientation;

    std::string     imageLabel;
    uint32_t        width                   = 0;
    uint32_t        height                  = 0;
    Encoding        encoding                = IMAGE_ENCODING_UNDEFINED;
    Format          sourceFormat            = FORMAT_UNDEFINED;
    Orientation     orientation             = ORIENTATION_0;
};

// MARK: - ON-DISK WIDTHS
// The specification stores each encoding and metadata format in one byte, the
// orientation as a half-precision float in two, and offsets and sizes in eight,
// so a change of underlying type fails here before it changes a file.
static_assert(sizeof(Encoding)          == 1);
static_assert(sizeof(MetadataType)      == 1);
static_assert(sizeof(ImageEncoding)     == 1);
static_assert(sizeof(ImageOrientation)  == 2);
static_assert(sizeof(Offset)            == 8);
static_assert(sizeof(Size)              == 8);
} // END IRIS FILE TYPES
#endif /* IrisFileTypes_h */
