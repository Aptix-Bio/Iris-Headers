/**
 * @file IrisCorePython.hpp
 * @author Ryan Landvater
 * @brief  Iris Core API Python Bindings.
 * @version 2025.1.0
 * @date 2025-01-14
 *
 * @copyright Copyright (c) 2025
 * Created by Ryan Landvater on 1/14/2025
 *
 */
#include "IrisCorePriv.hpp"
namespace Iris {
bool _success (const Result& result)
{
    return result == IRIS_SUCCESS;
}
Version _get_version ()
{
    return Version {
        IRIS_MAJOR_VERSION,
        IRIS_MINOR_VERSION,
        IRIS_BUILD_NUMBER,
    };
}
}
static inline void DEFINE_IRIS_CORE_SUBMODULE (pybind11::module_& base)
{
    using namespace Iris;
    using namespace pybind11::literals;
    
    auto m = base.def_submodule
    ("Core", "This is the Core Iris rendering submodule that allows access to the Iris Rendering System");
    
    py::class_<Iris::Version>                               (m, "Version")
        .def_readonly("major",      &Iris::Version::major)
        .def_readonly("minor",      &Iris::Version::minor)
        .def_readonly("build",      &Iris::Version::build)
        .def("__repr__",[](const Iris::Version &v) {
            std::stringstream version_str;
            version_str<<v.major<<"."<<v.minor<<"."<<v.build;
            return version_str.str();
        })
        .doc() = "Iris version (major, minor, and build number)";
    py::enum_<Iris::ResultFlag>                             (m, "ResultFlag")
        .value("Success",           IRIS_SUCCESS)
        .value("Failure",           IRIS_FAILURE)
        .value("Uninitialized",     IRIS_UNINITIALIZED)
        .value("ValidationFailure", IRIS_VALIDATION_FAILURE)
        .value("UNDEFINED_ERROR",   RESULT_MAX_ENUM);
    
    py::class_<Iris::Result>                                (m, "Result")
        .def         ("success",    &_success)
        .def_readonly("value",      &Result::flag)
        .def_readonly("message",    &Result::message);
    py::enum_<Iris::Format>                                 (m, "Format")
        .value("FORMAT_B8G8R8",     Iris::FORMAT_B8G8R8)
        .value("FORMAT_R8G8B8",     Iris::FORMAT_R8G8B8)
        .value("FORMAT_B8G8R8A8",   Iris::FORMAT_B8G8R8A8)
        .value("FORMAT_R8G8B8A8",   Iris::FORMAT_R8G8B8A8);
    py::class_<Iris::LayerExtent>                           (m, "LayerExtent")
        .def_readonly("x_tiles",    &LayerExtent::xTiles)
        .def_readonly("y_tiles",    &LayerExtent::yTiles)
        .def_readonly("scale",      &LayerExtent::scale)
        .def_readonly("downsample", &LayerExtent::downsample);
    py::class_<Iris::Extent>                                (m, "Extent")
        .def_readonly("width",      &Extent::width)
        .def_readonly("height",     &Extent::height)
        .def_readonly("layers",     &Extent::layers);
    py::enum_<Iris::BufferReferenceStrength>                (m, "BufferStrength")
        .value("REFERENCE_WEAK",    REFERENCE_WEAK)
        .value("REFERENCE_STRONG",  REFERENCE_STRONG);
    
    m.def("get_version",            &Iris::_get_version,
          "Get the Iris Core module version associated with this package");
    
    py::class_<Iris::Buffer_t, Iris::Buffer>      (m, "Buffer")
        .def(py::init<BufferReferenceStrength>())
        .def(py::init<BufferReferenceStrength,size_t>())
        .def("get_strength",        &Buffer_t::get_strength);
//        .def("data",                &Buffer_t::data);

}
