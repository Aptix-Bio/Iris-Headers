# VcpkgCaching_2 (paired with Aptix-Bio/Iris-Slide, feature/VcpkgCaching_2): the Iris-Slide cell of
# downstream.yml. Iris-Slide takes Iris-Headers and Iris-File-Extension as vcpkg ports pinned by
# commit, so FETCHCONTENT_SOURCE_DIR_<NAME> no longer reaches them. Iris-Slide's
# scripts/local_ports.cmake writes overlay ports whose source is a local checkout; this builds
# Iris-Slide against the change through them, and against the base when that fails:
#   change green                           -> pass
#   any configure failure                  -> the cell's tooling is broken: fail
#   change red, base green                 -> this change broke it: fail
#   change red, base red at build or test  -> already broken: warn, pass
#
# Usage: cmake -DCONSUMER=<Iris-Slide checkout> -DCHANGE=<checkout> -DBASE=<checkout>
#              [-DIFE=<Iris-File-Extension checkout>] [-DOPTIONS=<-D...;-D...>] [-DTEST=ON]
#              -P cmake/downstream_verdict.cmake
cmake_minimum_required(VERSION 3.27)
foreach(required CONSUMER CHANGE BASE)
    if(NOT ${required})
        message(FATAL_ERROR "downstream_verdict: -D${required}=<checkout> is required")
    endif()
    cmake_path(ABSOLUTE_PATH ${required} NORMALIZE)
endforeach()
if(IFE)
    cmake_path(ABSOLUTE_PATH IFE NORMALIZE)
    set(ife_argument "-DIRIS_FILE_EXTENSION=${IFE}")
endif()

# 0 green, 1 red at build or test, 2 red at configure.
function(downstream_build headers out result_variable)
    set(overlay "${out}-overlay")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DOUT=${overlay}" "-DIRIS_HEADERS=${headers}" ${ife_argument}
                            -P "${CONSUMER}/scripts/local_ports.cmake" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        set(${result_variable} 2 PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CONSUMER}" -B "${out}" -G Ninja -DCMAKE_BUILD_TYPE=Release
                            "-DVCPKG_OVERLAY_PORTS=${overlay}" ${OPTIONS} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        set(${result_variable} 2 PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${out}" --parallel 4 RESULT_VARIABLE result)
    if(result EQUAL 0 AND TEST)
        execute_process(COMMAND ctest --test-dir "${out}" --output-on-failure --no-tests=error RESULT_VARIABLE result)
    endif()
    if(result EQUAL 0)
        set(${result_variable} 0 PARENT_SCOPE)
    else()
        set(${result_variable} 1 PARENT_SCOPE)
    endif()
endfunction()

downstream_build("${CHANGE}" "${CMAKE_CURRENT_BINARY_DIR}/build-change" change)
if(change EQUAL 0)
    message(STATUS "change: green")
    return()
endif()
if(change EQUAL 2)
    message(FATAL_ERROR "The consumer does not configure against the change: the cell's tooling is broken.")
endif()
message(STATUS "change: red at build or test")
downstream_build("${BASE}" "${CMAKE_CURRENT_BINARY_DIR}/build-base" base)
if(base EQUAL 0)
    message(FATAL_ERROR "This change breaks the consumer: it builds against the base and fails against the change.")
endif()
if(base EQUAL 2)
    message(FATAL_ERROR "The consumer does not configure against the base: the cell's tooling is broken.")
endif()
message(WARNING "The consumer fails against the base too: already broken, not blocked by this change.")
