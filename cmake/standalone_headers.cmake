# StandaloneHeaderBuild_1.1 (development/MODERNIZATION.md, SafetyNet_1):
# every header compiles on its own and all of them together, so a header that
# relies on another being included first fails here instead of in a consumer.
# CONFIGURE_DEPENDS globbing means a new header is checked without editing
# this file.
file(GLOB _iris_headers_public CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
file(GLOB _iris_headers_private CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/priv/*.hpp")

# Excluded, each until the plan step that removes it from this repository:
#   IrisCorePython.hpp includes IrisV2's IrisCorePriv.hpp, which is not here
#     (Narrow_3, PythonGlueMoved_3.4).
#   IrisCoreVulkan.hpp needs the Vulkan and VMA headers (Narrow_3,
#     CodecApiMoved_3.2).
set(_iris_headers_excluded IrisCorePython.hpp IrisCoreVulkan.hpp)

set(_all_includes "")
foreach(_header IN LISTS _iris_headers_public _iris_headers_private)
    get_filename_component(_name "${_header}" NAME)
    if(_name IN_LIST _iris_headers_excluded)
        continue()
    endif()
    get_filename_component(_stem "${_header}" NAME_WE)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/standalone_headers/${_stem}.cpp")
    file(WRITE "${_source}" "#include <${_name}>\n")
    add_library(iris_headers_standalone_${_stem} OBJECT "${_source}")
    target_include_directories(iris_headers_standalone_${_stem} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/priv")
    target_compile_features(iris_headers_standalone_${_stem} PRIVATE cxx_std_20)
    string(APPEND _all_includes "#include <${_name}>\n")
endforeach()

set(_all_source "${CMAKE_CURRENT_BINARY_DIR}/standalone_headers/all_headers.cpp")
file(WRITE "${_all_source}" "${_all_includes}")
add_library(iris_headers_standalone_all OBJECT "${_all_source}")
target_include_directories(iris_headers_standalone_all PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/priv")
target_compile_features(iris_headers_standalone_all PRIVATE cxx_std_20)
