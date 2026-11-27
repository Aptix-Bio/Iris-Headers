# BlacksmithRunners_1.4 (development/PIPELINE.md, rule 2): every job in
# .github/workflows runs on a pinned Blacksmith runner (decided 2026-11-24).
# A GitHub-hosted label
# (ubuntu-24.04, macos-15, windows-2025) would bill GitHub minutes, which
# stopped starting jobs in Aptix-Bio's private repositories that day; a
# -latest label lets the image move under a passing build, so every label
# names its image's version (development/PIPELINE.md, rule 6, pin what can move).
#
#   cmake -P cmake/check_runner_labels.cmake      (from the repository root)
#
# Exits non-zero naming each offending file and line. Labels are matched as
# text wherever they appear outside a comment, so a label in a matrix entry is
# checked the same as one written after runs-on.
#
# The accepted labels are those Blacksmith documents
# (https://docs.blacksmith.sh/blacksmith-runners/overview, read 2026-11-24):
#   blacksmith-{2,4,8,16,32}vcpu-ubuntu-{2204,2404,2604}[-arm]
#   blacksmith-{2,4,8,16,32}vcpu-windows-2025
#   blacksmith-{6,12}vcpu-macos-{15,26,27}
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED WORKFLOW_DIR)
    set(WORKFLOW_DIR "${CMAKE_CURRENT_LIST_DIR}/../.github/workflows")
endif()
file(GLOB workflows "${WORKFLOW_DIR}/*.yml" "${WORKFLOW_DIR}/*.yaml")
if(NOT workflows)
    message(FATAL_ERROR "no workflow files under ${WORKFLOW_DIR}")
endif()

set(linux   "blacksmith-(2|4|8|16|32)vcpu-ubuntu-(2204|2404|2604)(-arm)?")
set(windows "blacksmith-(2|4|8|16|32)vcpu-windows-2025")
set(macos   "blacksmith-(6|12)vcpu-macos-(15|26|27)")

set(violations 0)
foreach(workflow IN LISTS workflows)
    get_filename_component(name "${workflow}" NAME)
    file(READ "${workflow}" text)
    # A CMake list splits on ';' and refuses to split inside '[' ... ']', and
    # workflow shell lines carry both; replace them before splitting on lines.
    string(REPLACE ";" "<semicolon>" text "${text}")
    string(REPLACE "[" "<open>" text "${text}")
    string(REPLACE "]" "<close>" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        # Comments may name the labels a job used to run on.
        string(REGEX REPLACE "^[ \t]*#.*$" "" code "${line}")
        string(REGEX REPLACE "[ \t]#.*$" "" code "${code}")

        set(problems "")
        if(code MATCHES "-latest")
            list(APPEND problems "a -latest label moves with the image")
        endif()
        string(REGEX MATCHALL "blacksmith-[A-Za-z0-9.-]+" labels "${code}")
        foreach(label IN LISTS labels)
            if(NOT label MATCHES "^(${linux}|${windows}|${macos})$")
                list(APPEND problems "'${label}' is not a documented Blacksmith label")
            endif()
        endforeach()
        string(REGEX REPLACE "blacksmith-[A-Za-z0-9.-]+" "" rest "${code}")
        # A GitHub-hosted label is a whole word: ubuntu-24.04, macos-15,
        # windows-2025, ubuntu-latest; cmake-macos-CI.yml is a file name.
        if(rest MATCHES "(^|[^A-Za-z0-9_.-])((ubuntu|macos|windows)-([0-9]|latest)[A-Za-z0-9.-]*)")
            list(APPEND problems "'${CMAKE_MATCH_2}' is a GitHub-hosted label")
        endif()
        if(rest MATCHES "runs-on:[ \t]*([^ \t$].*)$")
            list(APPEND problems "runs-on '${CMAKE_MATCH_1}' is not a Blacksmith label")
        endif()

        foreach(problem IN LISTS problems)
            message(SEND_ERROR "${name}:${number}: ${problem}")
            math(EXPR violations "${violations} + 1")
        endforeach()
    endforeach()
endforeach()

if(violations GREATER 0)
    message(FATAL_ERROR "${violations} runner label problem(s); see above")
endif()
list(LENGTH workflows count)
message(STATUS "runner labels: ${count} workflow(s), every job on a pinned Blacksmith runner")
