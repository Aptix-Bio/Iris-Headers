//
//  IrisSlideCore.hpp
//  Iris
//
//  Created by Ryan Landvater on 10/14/23.
//


#ifndef IrisSlideCore_h
#define IrisSlideCore_h
#include "IrisSlideTypes.hpp"

namespace   IrisSlide {
IRIS_EXPORT Version get_codec_version       () noexcept;

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//      Compression Context                                                 //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

/// Create a default compression context (see Iris Codex Context description).
IRIS_EXPORT Context create_context          () noexcept;

/// Create a IrisSlide Context that is responsible for decoding an Iris encoded file.
IRIS_EXPORT Context create_context          (const ContextCreateInfo&) noexcept;

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//      Slide File Access                                                   //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

/// Check basic file header signatures to see if it is an Iris Slide File
IRIS_EXPORT Result is_iris_slide_file       (const std::string& file_path) noexcept;

/// Validate a slide file encoded by Iris Slide
IRIS_EXPORT Result validate_slide           (const SlideOpenInfo&) noexcept;

/// Open an IrisSlide encoded slide file
IRIS_EXPORT Slide open_slide                (const SlideOpenInfo&) noexcept;

/// Pull the slide extent from a valid Iris Slide File
IRIS_EXPORT Result get_slide_info           (const Slide&, SlideInfo&) noexcept;

/// Retrieve a slide tile pixel array
IRIS_EXPORT Buffer read_slide_tile          (const SlideTileReadInfo&) noexcept;

/// Retrieve information about an associatiated slide image with a specific tag
IRIS_EXPORT Result get_associated_image_info(const Slide&, AssociatedImageInfo&) noexcept;

/// Retrieve a slide associated image, if present
IRIS_EXPORT Buffer read_associated_image    (const AssociatedImageReadInfo&) noexcept;

/// Add an annotation to a slide object
IRIS_EXPORT Result annotate_slide           (const Annotation&) noexcept;

/// Return a list of slide annotation names
IRIS_EXPORT Result get_slide_annotations    (const Slide&, Annotations&) noexcept;

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//      Slide Temporary Cache                                               //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

/// Create a cache / temporary codec slide file for stashing image data
IRIS_EXPORT Cache create_cache              (const CacheCreateInfo&) noexcept;

/// Read data from a Iris File Cache disk entry. It can be returned via decompression or simply copied from disk.
IRIS_EXPORT Buffer read_cache_entry         (const SlideTileReadInfo&) noexcept;

/// Write data into a Iris File Cache disk entry. It can be compressed to disk or simply copied in a bytestream form.
IRIS_EXPORT Result cache_store_entry        (const CacheStoreInfo&) noexcept;

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//      Slide Encoder                                                       //
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
/// An encoder for desc.source, written through @p encoder. Refuses, naming the value: a source no
/// source kind reads, an encoding the encoder does not write, a quality outside 1-100, a destination
/// that is not a folder, a layer step that is not a power of two from 2, a memory budget below the
/// slide's smallest working set, and attributes IFE cannot hold or that are not METADATA_I2S.
IRIS_EXPORT Result create_encoder           (const EncoderDesc& desc, Encoder& encoder) noexcept;

/// Starts the encode on the encoder's own threads and returns. Refuses an encoder that is not READY.
IRIS_EXPORT Result dispatch_encoder         (const Encoder&) noexcept;

/// A snapshot of the encode.
IRIS_EXPORT Result encoder_progress         (const Encoder&, EncoderProgress& progress) noexcept;

/// Stops an ACTIVE encode; it ends INTERRUPTED and leaves no file.
IRIS_EXPORT Result interrupt_encoder        (const Encoder&) noexcept;

/// The file name an encode of @p source in @p encoding writes, <source stem>.<encoding>.iris.
IRIS_EXPORT Result encoder_output_name      (const std::filesystem::path& source, Encoding encoding,
                                             std::string& name) noexcept;
}
#endif /* IrisSlideCore_h */
