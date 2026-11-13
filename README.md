# Iris-Headers

This repository contains the Iris Digital Pathology public header files for all available modules(*see below*). These headers are not exhaustive; rather they allow for integration of the simplified public Iris application programming interface (API) without defining the internal stuctures within each module. These are the preferred headers if you wish to use Iris without compiling Iris modules individually.

Iris Digital Pathology API Modules:
  - IrisCore
  - IrisCodec
  - ~~IrisNetworking~~ (*not yet available -- internal use only*)
  - ~~IrisUserInterface~~ (*not yet available -- internal use only*)

Additionally, we provide access to some core internal components if you would like to link against them for ease of integrating with the Iris API. For example, Iris Buffers, designed for quick zero-copy data transfers and Iris Networking interfaces, can be linked against if you would like to wrap data or retrieve data in a way that is compatible with with Iris' internals. These optionally available components are set up as static libraries in the *CMakeLists.txt* configuration and are as follows:
  - IrisBuffers.cpp

## Incorporating this repository
**To use Iris Headers in your project** it is preferred that you use the CMake system and fetch this content:
```CMake 
FetchContent_Declare (
    IrisHeaders
    GIT_REPOSITORY https://github.com/IrisDigitalPathology/Iris-Headers.git
    GIT_TAG "origin/main"
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(IrisHeaders)
```
and included (without needing to add IrisHeaders to include_directories) as follows:
```CMake
target_link_libraries (
    YOUR_TARGET PUBLIC
    IrisHeaders # Use headers
    IrisBuffer  # (optional) Buffers
)
```
