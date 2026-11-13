/**
 * @file IrisTypes.hpp
 * @author Ryan Landvater
 * @brief Iris Core API Types and Structure Definitions
 * @version 2024.0.3
 * @date 2023-08-26
 * 
 * @copyright Copyright (c) 2023-24
 * Created by Ryan Landvater on 8/26/23.
 * 
 * \note ALL STRUCTURE Variables SHALL have variables named in cammelCase
 * \note ALL CLASSES Variables SHALL have underscores with _cammelCase
 * \note ALL LOCAL variables SHALL use lower-case snake_case
 * 
 */

#ifndef IrisTypes_h
#define IrisTypes_h
#include <set>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <cstring>
#include <stdint.h>
#include <cstdint>
#include <source_location>
#include <type_traits>
#include <utility>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// windows.h declares `min` and `max` as macros unless NOMINMAX is set first,
// and this header is pulled into nearly every translation unit in the
// ecosystem. Left unguarded, the macros rewrite std::min / std::max / any
// std::numeric_limits<T>::max() into a bare `::(` and fail the build (MSVC
// C2589/C2059). Define it here, once, before the include; a consumer that has
// already asked for it keeps its own definition.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#ifndef IRIS_EXPORT_API
#define IRIS_EXPORT_API     false
#endif
#if     IRIS_EXPORT_API
    #ifndef IRIS_EXPORT
    #if defined(_MSC_VER)
    #define IRIS_EXPORT     __declspec(dllexport)
    #else
    #define IRIS_EXPORT     __attribute__ ((visibility ("default")))
    #endif 
    #endif
#else
    #ifndef IRIS_EXPORT
    #if defined(_MSC_VER)
    #define IRIS_EXPORT     __declspec(dllimport)
    #else
    #define IRIS_EXPORT
    #endif
    #endif
#endif
#define TILE_PIX_LENGTH     256U
#define TILE_PIX_FLOAT      256.f
#define TILE_PIX_AREA       65536U
#define TILE_PIX_BYTES_RGB  196608U
#define TILE_PIX_BYTES_RGBA 262144U
namespace Iris {
using BYTE                  = uint8_t;
using BYTE_ARRAY            = std::vector<BYTE>;
using CString               = std::vector<char>;
using CStringList           = std::vector<const char*>;
using atomic_bool           = std::atomic<bool>;
using atomic_byte           = std::atomic<uint8_t>;
using atomic_sint8          = std::atomic<int8_t>;
using atomic_uint8          = std::atomic<uint8_t>;
using atomic_sint16         = std::atomic<int16_t>;
using atomic_uint16         = std::atomic<uint16_t>;
using atomic_sint32         = std::atomic<int32_t>;
using atomic_uint32         = std::atomic<uint32_t>;
using atomic_sint64         = std::atomic<int64_t>;
using atomic_uint64         = std::atomic<uint64_t>;
using atomic_size           = std::atomic<size_t>;
using atomic_float          = std::atomic<float>;
using Threads               = std::vector<std::thread>;
using Mutex                 = std::mutex;
using MutexLock             = std::unique_lock<Mutex>;
using SharedMutexLock       = std::shared_ptr<MutexLock>;
using SharedMutex           = std::shared_mutex;
using ExclusiveLock         = std::unique_lock<SharedMutex>;
using SharedLock            = std::shared_lock<SharedMutex>;
using ReadLock              = std::shared_lock<SharedMutex>;
using WriteLock             = std::unique_lock<SharedMutex>;
using Notification          = std::condition_variable;
using FilePaths             = std::vector<const char*>;
using LambdaPtr             = std::function<void()>;
using CallbackDict          = std::unordered_map<std::string, LambdaPtr>;
using ViewerWeak            = std::weak_ptr<class Viewer_t>;
using LayerIndex            = uint32_t;
using TileIndex             = uint32_t;
using ImageIndex            = uint32_t;
using TileIndicies          = std::vector<TileIndex>;
using TileIndexSet          = std::unordered_set<TileIndex>;
using ImageIndicies         = std::vector<ImageIndex>;
using TimePoint             = std::chrono::time_point<std::chrono::system_clock>;

/// How a Result came out. A warning is success with something worth knowing.
enum class Severity : std::uint8_t {
    SUCCESS,
    WARNING,
    FAILURE,
};
/// The project whose error enumeration a Result's code belongs to.
enum class Domain : std::uint8_t {
    IRIS_HEADERS,
    IRIS_FILE_EXTENSION,
    IRIS_CODEC,
    IRIS_CORE,
};
/// Iris-Headers' own failures (Domain::IRIS_HEADERS).
enum class HeadersError : std::uint32_t {
    BUFFER_WEAK_REFERENCE_RESIZE,
    BUFFER_REALLOCATION_FAILED,
    BUFFER_SIZE_NOT_GROWN,
    MEMORY_INVALID_ARGUMENT,
    MEMORY_SYSTEM_ERROR,
    MEMORY_EXCEPTION,
};
/// A project's error enumeration: scoped, and fitting in a Result's 32-bit code.
template <class Code>
concept ResultCode = std::is_enum_v<Code>
    && !std::is_convertible_v<Code, std::underlying_type_t<Code>>
    && sizeof(Code) <= sizeof(std::uint32_t);
/**
 * @brief The outcome of an Iris call (ResultRedesigned_4.1).
 *
 * A Result says how the call came out (severity), whose error enumeration
 * its code belongs to (domain), which error it was (code, that project's own
 * scoped enumeration, so every project keeps its codes without this header
 * knowing them), the offending value (message), and where it was made
 * (where). It is checked by name, succeeded() or failed(); it does not
 * convert to bool, so a warning can never be mistaken for, or silently pass
 * as, a plain success. A default Result is an unassigned failure, so one
 * nobody set fails safe.
 */
struct Result {
    Severity                severity    = Severity::FAILURE;
    Domain                  domain      = Domain::IRIS_HEADERS;
    std::uint32_t           code        = 0;
    std::string             message;
    std::source_location    where;

    [[nodiscard]] static Result success (std::source_location made = std::source_location::current()) noexcept {
        Result result;
        result.severity = Severity::SUCCESS;
        result.where    = made;
        return result;
    }
    template <ResultCode Code>
    [[nodiscard]] static Result warning (Domain domain, Code code, std::string message,
                                         std::source_location made = std::source_location::current()) noexcept {
        return make(Severity::WARNING, domain, code, std::move(message), made);
    }
    template <ResultCode Code>
    [[nodiscard]] static Result failure (Domain domain, Code code, std::string message,
                                         std::source_location made = std::source_location::current()) noexcept {
        return make(Severity::FAILURE, domain, code, std::move(message), made);
    }

    /// True for success and for a warning.
    [[nodiscard]] bool succeeded () const noexcept { return severity != Severity::FAILURE; }
    /// True only for a failure: the check for a caller that must stop.
    [[nodiscard]] bool failed () const noexcept { return severity == Severity::FAILURE; }
    /// True only for a warning; message carries the detail.
    [[nodiscard]] bool is_warning () const noexcept { return severity == Severity::WARNING; }
    /// The code as the project's own enumeration. Pre: domain is that project's.
    template <ResultCode Code>
    [[nodiscard]] Code code_as () const noexcept { return static_cast<Code>(code); }

private:
    template <ResultCode Code>
    static Result make (Severity severity, Domain domain, Code code, std::string&& message,
                        std::source_location made) noexcept {
        Result result;
        result.severity = severity;
        result.domain   = domain;
        result.code     = static_cast<std::uint32_t>(static_cast<std::underlying_type_t<Code>>(code));
        result.message  = std::move(message);
        result.where    = made;
        return result;
    }
};
struct IRIS_EXPORT Version {
    uint32_t        major   = 0;
    uint32_t        minor   = 0;
    uint32_t        build   = 0;
};
/**
 * @brief Iris Buffer ownership strength to underlying data. A weak reference
 * only wraps data blocks by reference but has no responsibility over the 
 * creation or freeing of that datablock. Strong references have responsibility
 * over the data backing the buffer and will free the memory on buffer destruction.
 * 
 * \note A weak buffer explicitly is forbidden from resizing the buffer as it *may*
 * invalidate the original pointer.
 * \warning Changing a strong to weak buffer **requires** the calling program
 * take responsibility for the buffer data pointer. It is now that program's
 * responsibility to free that data once finished or a memory leak will ensue.
 * 
 */
enum IRIS_EXPORT BufferReferenceStrength {
    /// @brief Only wraps access to the data. No ownership or ability to resize underlying pointer.
    REFERENCE_WEAK      = 0,
    /// @brief Full ownership. Will free data on buffer destruction. Can resize underlying pointer.
    REFERENCE_STRONG    = 1,
};
/**
 * @brief Reference counted data object used to wrap datablocks.
 *
 * It can either strong reference or weak reference the underlying data.
 * The buffer can also shift between weak and strong referrences if chosen;
 * however, this is very dangerous obviously and you need to ensure you
 * are tracking if you have switched from weak to strong or vice versa.
 *
 * \note Buffer_t is an internally defined class. You may optionally
 *  include it in your implementation; however, many class methods are unsafe
 *  as they were created for exclusive use by Iris developers and use of these methods
 *  comes with risk.
 *
 *  \warning Buffer is currently NOT safe for concurrent use on muplitple threads.
 *  This will be fixed in future updates.
 */
using Buffer = std::shared_ptr<class Buffer_t>;
/**
 * @brief Access point to Iris API and controls all elements of Iris viewspace
 * 
 * The viewer the the primary control class that interfaces between external
 * applications and their views, and the iris rendering system. It contains interface
 * capabilities between external controllers, coordinates display presentations between
 * external surfaces, and creates any user interface functionalities. It is created using
 * the Iris::create_viewer(const Iris::ViewerCreateInfo&) method and initialized using
 * the Iris::viewer_bind_external_surface(const Iris::ViewerBindExternalSurfaceInfo&) method.
 * \sa ViewerCreateInfo and ViewerBindExternalSurfaceInfo
 * 
 * \note Viewer_t is an internally defined class and not externally exposed.
 */
using Viewer = std::shared_ptr <class Viewer_t>;
/**
 * @brief Handle to Slide File and Slide Loading Routines (Slide Loader)
 * 
 * The Slide object represents a mapped slide file and high-performance loading
 * routines to bring slide data into RAM with limited overhead
 */
using Slide  = std::shared_ptr<class  Slide_t>;

/**
 * @brief Defines necesary runtime parameters for starting the Iris rendering engine.
 * 
 * These runtime parameters will be forwarded to the GPU for certain task tracking
 * and the application bundle path (term from Apple's OS) is important for loading
 * referenced / included runtime files.
 * 
 * Additional runtime parameters will be added as needed in the future.
 */
struct IRIS_EXPORT ViewerCreateInfo {
    /// @brief Informs the rendering engine of the calling application's name
    const char*         ApplicationName;
    /// @brief Informs the engine of the calling application version
    uint32_t            ApplicationVersion;
    /// @brief provides the executable location. This is needed for runtime
    /// loading of application files such as UI markup files and shader code.
    const char*         ApplicationBundlePath;
};
/**
 * @brief  System specific binding information to configure Iris' rendering engine
 * for the given operating system draw surface. 
 * 
 * Compilier macros control the structure's definition and backend implementation
 * and thus define the nature of the OS draw surface handles.
 *  - Windows: requires HINSTANCE and HWND handles from the WIN32 API
 *  - Apple: macOS and iOS require a __bridge pointer to a CAMetalLayer
 *  - Web (Emscripten): the CSS selector of the page's <canvas>, e.g. "#iris"
 * 
 */
struct IRIS_EXPORT ViewerBindExternalSurfaceInfo {
    const Viewer        viewer      = nullptr;  
#if defined _WIN32
    HINSTANCE           instance    = NULL;    
    HWND                window      = NULL;    
#elif defined __EMSCRIPTEN__
    const char*         canvas      = nullptr;
#elif defined __APPLE__
    const void*         layer       = nullptr; 
#endif
};

struct IRIS_EXPORT ViewerResizeSurfaceInfo {
    const Viewer        viewer      = nullptr;
    const uint32_t      width       = 0;
    const uint32_t      height      = 0;
};

/**
 * @brief  Information to translate the rendered scope view as a fraction of the active
 * view space with direction given by the sign.
 * 
 * An x translation value of 0.5 will shift the view to the right by half of the current
 * view sapce while -1.0 will shift the scope view to the left by an entire screen.
 * 
 */
struct IRIS_EXPORT ViewerTranslateScope {
    // The slide follows the sign, as a drag moves it: +x right, +y down. The
    // velocities are not applied yet: the engine has no inertia.
    /// @brief Fraction of *horizontal* viewspace to translate [-1,1](-left, +right)
    float               x_translate = 0.f;
    /// @brief Fraction of *vertical* viewspace to translate [-1,1](-left, +right)
    float               y_translate = 0.f;
    /// @brief Horizontal translation velocity (suggested [0,2])
    float               x_velocity  = 0.f;
    /// @brief Velocity of translation (suggested [0,2]) in the horizontal
    float               y_velocity  = 0.f;
};
/**
 * @brief Information to change the zoom objective.
 * 
 * A positive zoom increment will increase the scope view 
 * zoom while a negative increment will decrease the current zoom.
 * The zoom origin (x_location and y_location) defines the region
 * around which to zoom. This is best set as either the cursor location
 * or view center (0.5, 0.5).
 * 
 */
struct IRIS_EXPORT ViewerZoomScope {
    /// @brief Fraction of current zoom amount by which to increase or decrease
    float               increment   = 0.f;
    /// @brief Horizontal location of zoom origin (towards or way from this point)
    float               x_location  = 0.5f;
    /// @brief Vertical location of zoom origin
    float               y_location  = 0.5f;
};
/**
 * @brief Information to turn the rendered scope view about a point in it.
 *
 * A positive angle turns the slide counterclockwise as seen on screen. The
 * slide point under the origin (x_location, y_location, fractions of the view
 * from its top left) stays where it is, as a zoom's does; the cursor location
 * or the view centre (0.5, 0.5) suits it best.
 *
 */
struct IRIS_EXPORT ViewerRotateScope {
    /// @brief Angle to turn the slide, in radians, counterclockwise on screen [-pi, pi]
    float               radians     = 0.f;
    /// @brief Horizontal location of the turn's origin [0, 1]
    float               x_location  = 0.5f;
    /// @brief Vertical location of the turn's origin [0, 1]
    float               y_location  = 0.5f;
};
/**
 * @brief Defines the image encoding format for an image annotation.
 * 
 */
/** \def SlideAnnotation::format
 * The AnnotationFormat of the image data to be rendered
 */
enum IRIS_EXPORT AnnotationTypes : uint8_t {
    ANNOTATION_UNDEFINED                    = 0,
    ANNOTATION_PNG                          = 1,
    ANNOTATION_JPEG                         = 2,
    ANNOTATION_SVG                          = 3,
    ANNOTATION_TEXT                         = 4,
};

struct IRIS_EXPORT Annotation {
    using           Identifier              = uint32_t;
    Slide           slide                   = NULL;
    AnnotationTypes type                    = Iris::ANNOTATION_UNDEFINED;
    Buffer          data                    = NULL;
    float           xLocation               = 0.f;
    float           yLocation               = 0.f;
    float           xSize                   = 0.f;
    float           ySize                   = 0.f;
    uint32_t        width                   = 0;
    uint32_t        height                  = 0;
};

using Annotations = std::unordered_map<Annotation::Identifier, Annotation>;

struct IRIS_EXPORT AnnotationGroup :
public std::unordered_set<Annotation::Identifier> {
    std::string     label;
};
/**
 * @brief Structure defining requirements to create an image-based
 * slide annotation.
 * 
 * The required information includes the location of the slide annotation
 * on the slide and the size of the annotation. The offset locations are 
 * fractions of the current view window (for example an annotation that
 * starts in the middle of the current view would have an offset of 0.5)
 * The engine will immediately begin rendering the image on top of the 
 * rendered slide layers.
 */
struct IRIS_EXPORT AnnotateSlideInfo {
    /// @brief AnnotationFormat of the image data to be rendered
    AnnotationTypes    format      = ANNOTATION_UNDEFINED;
    /// @brief x-offset of the current scope view window where the image starts [0,1.f]
    float               x_offset    = 0.f;
    /// @brief y-offset of the current scope view window where the image starts [0,1.f]
    float               y_offset    = 0.f;
    /// @brief Number of horizontal (x) pixels in the image annotation
    uint32_t            width       = 0U;
    /// @brief Number of vertical (y) pixels in the image annotation
    uint32_t            height      = 0U;
    /// @brief Encoded pixel data that comprises the image, width wide and hight tall
    Buffer              data;
};
/**
 * @brief  Slide objective layer extent detailing the extent of each objective layer in
 * the number of 256 pixel tiles in each dimension.  
 * 
 * The relative scale (zoom amount) as well as how downsampled the layer is relative to 
 * the highest zoom layer (the reciprocal of the scale).
 */
struct IRIS_EXPORT LayerExtent {
    /// @brief Number of horizontal 256 pixel tiles
    uint32_t            xTiles      = 1; 
    /// @brief Number of vertical 256 pixel tiles
    uint32_t            yTiles      = 1;
    /// @brief Number of Z-stacked planes
//    uint32_t            zPlanes     = 1;
    /// @brief How magnified this level is relative to the unmagnified size of the tissue
    float               scale       = 1.f;
    /// @brief Reciprocal scale factor relative to the most zoomed level (for OpenSlide compatibility)
    float               downsample  = 1.f;
};
using LayerExtents = std::vector<LayerExtent>;
/**
 * @brief The extent, in pixels, of a whole side image file. 
 * 
 * These are in terms of the initial layer presented (most zoomed out layer).
 */
struct IRIS_EXPORT Extent {
    /// @brief Top (lowest power) layer width extent in screen pixels
    uint32_t            width       = 1; 
    /// @brief Top (lowest power) layer height in screen pixels
    uint32_t            height      = 1; 
    /// @brief Slide objective layer extent list
    LayerExtents        layers; 
};
/**
 * @brief Image channel byte order in little-endian format
 * 
 * Assign this format to match the image source bits per
 * pixel and bit-ordering. 
 */
enum IRIS_EXPORT Format : uint8_t {
    /// @brief Invalid format indicating a format was not selected
    FORMAT_UNDEFINED    = 0 ,
    /// @brief 8-bit blue, 8-bit green, 8-bit red, no alpha
    FORMAT_B8G8R8       = 1,
    /// @brief 8-bit red, 8-bit green, 8-bit blue, no alpha
    FORMAT_R8G8B8       = 2,
    /// @brief 8-bit blue, 8-bit green, 8-bit red, 8-bit alpha
    FORMAT_B8G8R8A8     = 3,
    /// @brief 8-bit red, 8-bit green, 8-bit blue, 8-bit alpha
    FORMAT_R8G8B8A8     = 4,
};
/**
 * @brief Information to open a slide file located on a local volume.
 * 
 * Provide the file location and the 
 * 
 */
struct IRIS_EXPORT LocalSlideOpenInfo {
    const char*         filePath;
    /**
     * @brief Local slide file encoding type
     * 
     * This informs the Iris::Slide object how it should
     * attempt to open and map the slide file. If unknown,
     * it will attempt both encoding sequences. OpenSlide
     * is not supported on all platforms (iOS for example).
     * 
     */
    enum : uint8_t {
        SLIDE_TYPE_UNKNOWN,         //*< Unknown file encoding
        SLIDE_TYPE_IRIS,            //*< Iris Codec File
        SLIDE_TYPE_OPENSLIDE,       //*< Vendor specific file (ex SVS)
    }                   type        = SLIDE_TYPE_UNKNOWN;
    
};
/**
 * @brief Information needed to open a server-hosted slide file.
 * 
 * This requires use of the Iris Networking module.
 * 
 */
struct IRIS_EXPORT NetworkSlideOpenInfo {
    const char*         slideID;
};
/**
 * @brief Parameters required to create an Iris::Slide WSI file handle.
 * 
 * This parameter structure is a wrapped union of either
 * a local slide file open information struct or a network hosted
 * slide file open information struct. To allow the system to access
 * the correct union member, a type enumeration must also be defined
 * prior to passing this information stucture to the calling method
 * Iris::create_slide(const SlideOpenInfo&) or
 * Iris::viewer_open_slide(const Viewer&, const SlideOpenInfo&)
 * 
 * Optional parameters that can be used to optimize performance
 * characteristics are also included in the struct. Some are used interally
 * by the Iris rendering engine, and these are invoked when using 
 * Iris::viewer_open_slide(const Viewer&, const SlideOpenInfo&)
 * rather than the more generic Iris::create_slide(const SlideOpenInfo&),
 * so the former should be preferred when available.
 * 
 */
struct IRIS_EXPORT SlideOpenInfo {
    enum : uint8_t {
        SLIDE_OPEN_UNDEFINED,           // Default / invalid file
        SLIDE_OPEN_LOCAL,               // Locally accessible / Mapped File
        SLIDE_OPEN_NETWORK,             // Sever hosted slide file
    }                   type            = SLIDE_OPEN_UNDEFINED;
    union {
    /**
     * @brief Information for opening a file on the local machine
     */
    LocalSlideOpenInfo   local;
    /**
     * @brief Information for opening a network hosted file
     */
    NetworkSlideOpenInfo network;
    };
    // ~~~~~~~~~~~~~ OPTIONAL FEATURES ~~~~~~~~~~~~~~~ //
    /**
     * @brief This is the default slide cache capacity
     *
     * The capacity determines the number of allowed cached tiles.
     * This is the primary way in which Iris consumes RAM.
     * Greater values cache more in-memory decompressed tile data
     * for greater performance. Less require more pulls from
     * disk (which is slower)
     * The default 1000 for RGBA images consumes 2 GB of RAM.
     */
    size_t               capacity       = 1000;
};
using LambdaPtr         = std::function<void()>;
using LambdaPtrs        = std::vector<LambdaPtr>;
} // END IRIS NAMESPACE

#endif /* IrisTypes_h */
