# ReviewBot_1.5 (development/MODERNIZATION.md, SafetyNet_1; development/ERRATA.md, 2026-11-24): a
# job that needs repository variables or secrets fails first, naming each one that is missing,
# instead of failing later inside an action whose message does not say which setting to create.
#
#   env  <NAME>: ${{ secrets.<NAME> != '' }}     (or vars.<NAME>), one per setting
#   run: cmake -DREQUIRED="<NAME>;<NAME>" -P cmake/require_settings.cmake
#
# The workflow passes only whether each setting is set ('true' or 'false'), so no secret's value
# reaches this script or the step's environment.
cmake_minimum_required(VERSION 3.20)
if(NOT REQUIRED)
    message(FATAL_ERROR "require_settings: -DREQUIRED=<NAME;NAME> is required")
endif()
set(missing "")
foreach(name IN LISTS REQUIRED)
    if("$ENV{${name}}" STREQUAL "true")
        continue()
    endif()
    if(NOT "$ENV{${name}}" STREQUAL "false")
        message(FATAL_ERROR "require_settings: ${name} is '$ENV{${name}}' in the environment; "
                            "the workflow sets it to whether the setting is set, 'true' or 'false'")
    endif()
    list(APPEND missing "${name}")
endforeach()
if(missing)
    list(JOIN missing ", " names)
    message(FATAL_ERROR "missing repository setting(s): ${names}. Create each under Settings, "
                        "Secrets and variables, Actions (development/ERRATA.md says how).")
endif()
message(STATUS "settings present: ${REQUIRED}")
