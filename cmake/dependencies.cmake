# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Pinned third-party versions for every Iris repository — the ONE place a
# version lives. Here because every consumer fetches Iris-Headers first and
# includes ${irisheaders_SOURCE_DIR}/cmake/dependencies.cmake, so no two Iris
# repositories can pin different versions of one library. (openslide-bin had
# drifted: the log announced 4.0.0.6 while the download fetched 4.0.0.5.)
# Iris-Headers and Iris-File-Extension are not here: they track their owner's tips.
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# ---- FetchContent (archive tarballs; the hash pins integrity) --------------
set(IRIS_DEP_VULKAN_SDK     "vulkan-sdk-1.4.363.0")
set(IRIS_DEP_VMA            "v3.4.0")
set(IRIS_DEP_VMA_SHA256     "822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1")
set(IRIS_DEP_GLM            "1.0.3")
set(IRIS_DEP_GLM_SHA256     "6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001")
set(IRIS_DEP_PYBIND11       "v3.1.0")

# ---- ExternalProject (git tags) --------------------------------------------
# Highway: the SIMD downsample kernels use SumsOf2 / SumsOf4 /
# RepartitionToWideX2, added in 1.1 — so the find floor is 1.2, and the
# source-build fallback is a release that has them. An unversioned find_package
# silently accepts Ubuntu's 1.0.7, which does not.
set(IRIS_DEP_HWY_MIN        "1.2")        # find_package(hwy <floor>)
set(IRIS_DEP_HWY            "1.2.0")      # source-build fallback tag
set(IRIS_DEP_ZLIB_NG        "2.3.3")
set(IRIS_DEP_LIBPNG         "v1.6.50")
set(IRIS_DEP_JPEG_TURBO     "3.2.0")
set(IRIS_DEP_LIBAVIF        "v1.4.2")
set(IRIS_DEP_LIBDICOM       "v1.3.0")

# ---- Prebuilt binary release (decoded in cmake/openslide.cmake) ------------
set(IRIS_DEP_OPENSLIDE_BIN  "4.0.1.2")
