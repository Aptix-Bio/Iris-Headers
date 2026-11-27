# ReviewBot_1.5 (SafetyNet_1; development/PIPELINE.md, rule 3):
# cmake/downstream_verdict.cmake fails a pull request whose consumer builds only from its paired
# branch. After the rename merged, every consumer cell of `downstream` failed on main, because
# the pull request's own run had built Iris-File-Extension's paired branch and IFE's main still
# named the removed header.
#
#   cmake -P cmake/tests/downstream_verdict_test.cmake      (from the repository root; needs git)
#
# Hermetic: the consumer is a local git repository whose configure step requires one header,
# and the two Iris-Headers trees differ only in that header's name, so each case runs in
# seconds with no compiler. Each case states the verdict it expects and a phrase of its message.
cmake_minimum_required(VERSION 3.27)

set(script "${CMAKE_CURRENT_LIST_DIR}/../downstream_verdict.cmake")
set(root "${CMAKE_CURRENT_BINARY_DIR}/downstream-verdict-test")
file(REMOVE_RECURSE "${root}")
set(consumer Iris-File-Extension)
set(paired feature/HeaderRenamed)
if(CMAKE_HOST_WIN32)
    set(generator "-G;Ninja")
else()
    set(generator "-G;Unix Makefiles")
endif()

# Iris-Headers before and after a rename of one header.
file(WRITE "${root}/base/include/OldName.hpp" "#pragma once\n")
file(WRITE "${root}/change/include/NewName.hpp" "#pragma once\n")

function(git_in directory)
    execute_process(COMMAND git -c user.name=test -c user.email=test@example.invalid -c commit.gpgsign=false ${ARGN}
                    WORKING_DIRECTORY "${directory}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} in ${directory}: ${error}")
    endif()
endfunction()

# The consumer's repository, cloned as <OWNER_URL>/<name>.git.
set(repository "${root}/owner/${consumer}.git")

# A consumer that configures only when the headers carry the named file.
function(consumer_requires header)
    file(WRITE "${repository}/CMakeLists.txt"
         "cmake_minimum_required(VERSION 3.27)\n"
         "project(consumer LANGUAGES NONE)\n"
         "if(NOT EXISTS \"\${FETCHCONTENT_SOURCE_DIR_IRISHEADERS}/include/${header}\")\n"
         "    message(FATAL_ERROR \"Cannot find source file: ${header}\")\n"
         "endif()\n")
endfunction()

file(MAKE_DIRECTORY "${repository}")
git_in("${repository}" init --quiet --initial-branch=main)
consumer_requires(OldName.hpp)
git_in("${repository}" add CMakeLists.txt)
git_in("${repository}" commit --quiet -m "main names the old header")
git_in("${repository}" switch --quiet -c ${paired})
consumer_requires(NewName.hpp)
git_in("${repository}" commit --quiet -am "the paired branch names the new header")
git_in("${repository}" switch --quiet main)

set(failures 0)
# expect: PASS or FAIL; phrase: text the script's output must contain.
function(verdict_case name head_ref expect phrase)
    set(ENV{HEAD_REF} "${head_ref}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DCONSUMER=${consumer}" "-DCHANGE=${root}/change" "-DBASE=${root}/base"
                            "-DOWNER_URL=file://${root}/owner" -DAPI_URL=none "-DGENERATOR=${generator}"
                            -P "${script}"
                    WORKING_DIRECTORY "${root}/work-${name}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
    set(outcome PASS)
    if(NOT result EQUAL 0)
        set(outcome FAIL)
    endif()
    # CMake wraps a message's lines; compare with every run of whitespace as one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
    string(FIND "${flat}" "${phrase}" at)
    if(NOT outcome STREQUAL expect OR at EQUAL -1)
        message(SEND_ERROR "${name}: expected ${expect} with '${phrase}', got ${outcome}:\n${out}")
        math(EXPR failures "${failures} + 1")
        set(failures ${failures} PARENT_SCOPE)
    else()
        message(STATUS "${name}: ${outcome} ('${phrase}')")
    endif()
endfunction()

foreach(work coordinated unpaired)
    file(MAKE_DIRECTORY "${root}/work-${work}")
endforeach()
# The failure the errata row records: green on the paired branch, red on the consumer's main.
verdict_case(coordinated "${paired}" FAIL
             "a coordinated change: merging this breaks ${consumer} main until ${consumer}'s pull request from ${paired} merges")
# No paired branch: the consumer's main against the change, then the base, as before.
verdict_case(unpaired "feature/NoSuchBranch" FAIL "This change breaks the consumer")

# Once the consumer's main takes the new header, the paired branch no longer blocks.
git_in("${repository}" switch --quiet main)
consumer_requires(NewName.hpp)
git_in("${repository}" commit --quiet -am "main names the new header")
file(MAKE_DIRECTORY "${root}/work-merged")
verdict_case(merged "${paired}" PASS "main: green")

if(failures GREATER 0)
    message(FATAL_ERROR "${failures} downstream verdict case(s) failed")
endif()
message(STATUS "downstream verdict: 3 cases as expected")
