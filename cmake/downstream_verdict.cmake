# DownstreamBuild_1.2 (development/WBS.md, SafetyNet_1): one consumer cell of
# downstream.yml. A header change is correct only relative to what consumes it, so this builds the
# consumer against the change, and against the base when that fails (the control run):
#   change green                                  -> pass
#   change red, base green                        -> this change broke it: fail
#   base red at configure                         -> the cell's tooling is broken: fail
#   change red at configure, base red at build    -> this change broke its configure: fail
#   both red at build or test                     -> already broken: warn, pass
# A configure failure against the base alone marks broken tooling, since a header the change
# removes also fails at configure (IFE's CMakeLists.txt names each header as a source).
#
# A breaking change is made in each repository on a branch of the same name (the paired branch),
# so the consumer is built from its branch named like the pull request's head when it has one,
# else from main. ReviewBot_1.5 (development/ERRATA.md, 2026-11-24): a pull request that is green
# only on a paired branch breaks the consumer's main the moment it merges, so when a paired branch
# was used and the change is green on it, the consumer's main is built against the change as well:
#   main green                             -> pass
#   main red, main green against the base  -> a coordinated change: fail, naming the consumer's
#                                             open pull request, so both merge together
#   main red against the base too          -> main already broken: warn, pass
#
# Iris-File-Extension takes this checkout through FETCHCONTENT_SOURCE_DIR_IRISHEADERS. Iris-Slide
# (VcpkgCaching_2) takes Iris-Headers and Iris-File-Extension as vcpkg ports, so its
# scripts/local_ports.cmake writes overlay ports whose sources are local checkouts; its IFE is
# paired by the same rule.
#
# Usage, from the workspace (the consumers are cloned under the working directory):
#   HEAD_REF=<pull request head branch, empty on a push> [GH_TOKEN=<token>]
#   cmake -DCONSUMER=<Iris-File-Extension|Iris-Slide> -DCHANGE=<checkout> -DBASE=<checkout>
#         [-DOPTIONS=<-D...;-D...>] [-DTEST=ON] [-DGENERATOR=<-G;...>]
#         [-DOWNER_URL=<clone root>] [-DAPI_URL=<REST root, or none>] -P cmake/downstream_verdict.cmake
# The branch name arrives through the environment, since it is text the pull request's author
# chose; it reaches git and the API only as one argument, never through a shell.
cmake_minimum_required(VERSION 3.27)
foreach(required CONSUMER CHANGE BASE)
    if(NOT ${required})
        message(FATAL_ERROR "downstream_verdict: -D${required}=<value> is required")
    endif()
endforeach()
foreach(checkout CHANGE BASE)
    cmake_path(ABSOLUTE_PATH ${checkout} NORMALIZE)
endforeach()
if(NOT CONSUMER MATCHES "^(Iris-File-Extension|Iris-Slide)$")
    message(FATAL_ERROR "downstream_verdict: unknown consumer '${CONSUMER}'; expected Iris-File-Extension or Iris-Slide")
endif()
if(NOT OWNER_URL)
    set(OWNER_URL "https://github.com/Aptix-Bio")
endif()
if(NOT API_URL)
    set(API_URL "https://api.github.com")
endif()
cmake_path(GET OWNER_URL FILENAME owner)
# IFE builds with Clang everywhere: Ninja with clang++, and on Windows the default Visual Studio
# generator with its ClangCL toolset. Iris-Slide builds with Ninja and the compiler its vcpkg
# ports are built with.
if(NOT GENERATOR)
    if(CMAKE_HOST_WIN32)
        set(GENERATOR "-T;ClangCL")
    elseif(CONSUMER STREQUAL "Iris-Slide")
        set(GENERATOR "-G;Ninja")
    else()
        set(GENERATOR "-G;Ninja;-DCMAKE_CXX_COMPILER=clang++")
    endif()
endif()
set(head_ref "$ENV{HEAD_REF}")
set(work "${CMAKE_CURRENT_BINARY_DIR}")

# The consumer's paired branch when it has one, else main; `paired` is the branch or empty.
function(paired_branch repository paired_variable)
    set(${paired_variable} "" PARENT_SCOPE)
    if(head_ref STREQUAL "")
        return()
    endif()
    execute_process(COMMAND git ls-remote --exit-code --heads "${OWNER_URL}/${repository}.git" "refs/heads/${head_ref}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(${paired_variable} "${head_ref}" PARENT_SCOPE)
    endif()
endfunction()

function(clone repository ref directory)
    if(EXISTS "${directory}")
        return()
    endif()
    message(STATUS "${owner}/${repository} at ${ref}")
    execute_process(COMMAND git clone --quiet --depth 1 "--branch=${ref}" "${OWNER_URL}/${repository}.git" "${directory}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "downstream_verdict: cloning ${owner}/${repository} at ${ref} failed (exit ${result})")
    endif()
endfunction()

# 0 green, 1 red at build or test, 2 red at configure.
function(downstream_build headers consumer ife out result_variable)
    set(configure "${CMAKE_COMMAND}" -S "${consumer}" -B "${out}" ${GENERATOR} -DCMAKE_BUILD_TYPE=Release ${OPTIONS})
    if(CONSUMER STREQUAL "Iris-Slide")
        set(overlay "${out}-overlay")
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DOUT=${overlay}" "-DIRIS_HEADERS=${headers}" "-DIRIS_FILE_EXTENSION=${ife}"
                                -P "${consumer}/scripts/local_ports.cmake" RESULT_VARIABLE result)
        list(APPEND configure "-DVCPKG_OVERLAY_PORTS=${overlay}")
    else()
        set(result 0)
        list(APPEND configure "-DFETCHCONTENT_SOURCE_DIR_IRISHEADERS=${headers}")
    endif()
    if(result EQUAL 0)
        execute_process(COMMAND ${configure} RESULT_VARIABLE result)
    endif()
    if(NOT result EQUAL 0)
        set(${result_variable} 2 PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${out}" --config Release --parallel RESULT_VARIABLE result)
    if(result EQUAL 0 AND TEST)
        execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${out}" -C Release --output-on-failure --no-tests=error
                        RESULT_VARIABLE result)
    endif()
    if(result EQUAL 0)
        set(${result_variable} 0 PARENT_SCOPE)
    else()
        set(${result_variable} 1 PARENT_SCOPE)
    endif()
endfunction()

# "<repository>#<number>" for the open pull request from the paired branch, or a description of
# where it should be when the lookup finds none. The branch reaches the URL only when it is made
# of the characters a URL query carries unescaped.
function(pull_request repository branch text_variable)
    set(${text_variable} "${repository}'s pull request from ${branch}" PARENT_SCOPE)
    if(API_URL STREQUAL "none" OR NOT branch MATCHES "^[A-Za-z0-9._/-]+$")
        return()
    endif()
    set(headers HTTPHEADER "Accept: application/vnd.github+json")
    if(NOT "$ENV{GH_TOKEN}" STREQUAL "")
        list(APPEND headers HTTPHEADER "Authorization: Bearer $ENV{GH_TOKEN}")
    endif()
    set(json "${work}/pulls-${repository}.json")
    file(DOWNLOAD "${API_URL}/repos/${owner}/${repository}/pulls?state=open&head=${owner}:${branch}" "${json}"
         ${headers} STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        message(WARNING "downstream_verdict: looking up ${repository}'s pull request from ${branch} failed: ${status}")
        return()
    endif()
    file(READ "${json}" pulls)
    string(JSON count ERROR_VARIABLE error LENGTH "${pulls}")
    if(error OR count EQUAL 0)
        return()
    endif()
    string(JSON number GET "${pulls}" 0 number)
    set(${text_variable} "${repository}#${number}" PARENT_SCOPE)
endfunction()

# The consumer, and Iris-Slide's IFE, each at its paired branch or main.
paired_branch(${CONSUMER} consumer_paired)
set(consumer_ref main)
if(consumer_paired)
    set(consumer_ref "${consumer_paired}")
endif()
clone(${CONSUMER} "${consumer_ref}" "${work}/consumer-${consumer_ref}")
set(consumer "${work}/consumer-${consumer_ref}")
set(paired_repositories "")
if(consumer_paired)
    list(APPEND paired_repositories ${CONSUMER})
endif()
set(ife "")
set(ife_main "")
if(CONSUMER STREQUAL "Iris-Slide")
    paired_branch(Iris-File-Extension ife_paired)
    set(ife_ref main)
    if(ife_paired)
        set(ife_ref "${ife_paired}")
        list(APPEND paired_repositories Iris-File-Extension)
    endif()
    clone(Iris-File-Extension "${ife_ref}" "${work}/ife-${ife_ref}")
    set(ife "${work}/ife-${ife_ref}")
    set(ife_main "${work}/ife-main")
endif()

downstream_build("${CHANGE}" "${consumer}" "${ife}" "${work}/build-change" change)
if(NOT change EQUAL 0)
    message(STATUS "change: red (${change}: 1 at build or test, 2 at configure)")
    downstream_build("${BASE}" "${consumer}" "${ife}" "${work}/build-base" base)
    if(base EQUAL 0)
        message(FATAL_ERROR "This change breaks the consumer: it builds against the base and fails against the change.")
    endif()
    if(base EQUAL 2)
        message(FATAL_ERROR "The consumer does not configure against the base: the cell's tooling is broken, "
                            "or the consumer's main already fails against the base (a coordinated change merged before its consumer's).")
    endif()
    if(change EQUAL 2)
        message(FATAL_ERROR "This change breaks the consumer's configure: the base configures and the change does not.")
    endif()
    message(WARNING "The consumer fails against the base too: already broken, not blocked by this change.")
    return()
endif()
message(STATUS "change: green")
if(NOT paired_repositories)
    return()
endif()

# Green on a paired branch: the same change against each paired repository's main. A configure
# failure counts as red here, since a removed header fails at configure (IFE's CMakeLists.txt).
clone(${CONSUMER} main "${work}/consumer-main")
if(ife_main)
    clone(Iris-File-Extension main "${ife_main}")
endif()
downstream_build("${CHANGE}" "${work}/consumer-main" "${ife_main}" "${work}/build-main-change" main_change)
if(main_change EQUAL 0)
    message(STATUS "main: green")
    return()
endif()
downstream_build("${BASE}" "${work}/consumer-main" "${ife_main}" "${work}/build-main-base" main_base)
if(NOT main_base EQUAL 0)
    message(WARNING "main: red against the base too: already broken, not blocked by this change.")
    return()
endif()
set(blockers "")
foreach(repository IN LISTS paired_repositories)
    pull_request(${repository} "${head_ref}" blocker)
    list(APPEND blockers "${blocker}")
endforeach()
list(JOIN blockers " and " blockers)
list(JOIN paired_repositories " and " repositories)
message(FATAL_ERROR "a coordinated change: merging this breaks ${repositories} main until ${blockers} merges; "
                    "merge them together. Green on the paired branch ${head_ref}; red on main, which builds against the base.")
