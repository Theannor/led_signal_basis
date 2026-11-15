# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Theannor
#
# Setzt PROJECT_VER nach PEP 440 aus dem letzten Git-Tag vYYYY.N.
# Releases heißen YYYY.N, alles danach ist eine Entwicklerversion der
# nächsten Release (N = Commits seit dem Tag):
#   genau auf Tag v2026.1               -> 2026.1
#   3 Commits danach                    -> 2026.2.dev3+gabc1234
#   mit lokalen Änderungen              -> 2026.2.dev3+gabc1234.dirty
#   Tag aus 2026, gebaut 2027           -> 2027.1.dev3+gabc1234
#   noch kein Tag (42 Commits)          -> <Jahr>.1.dev42+gabc1234
#   kein Git                            -> 0+unknown
# Der Teil nach '+' ist eine PEP-440-"local version" (Commit, Änderungen).
# ESP-IDF schreibt PROJECT_VER in die App-Beschreibung (esp_app_get_description()).

set(_ver_fallback "0+unknown")
set(_ver_tag_glob "v[0-9][0-9][0-9][0-9].[0-9]*")
set(_ver_root "${CMAKE_CURRENT_LIST_DIR}/..")

# Jahr des Builds: ist es neuer als das letzte Release, beginnt die Zählung bei 1
string(TIMESTAMP _ver_year "%Y")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty --abbrev=7
                --match ${_ver_tag_glob}
        WORKING_DIRECTORY ${_ver_root}
        OUTPUT_VARIABLE _ver_describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _ver_result)
    if(_ver_result EQUAL 0 AND
       _ver_describe MATCHES "^v([0-9][0-9][0-9][0-9])\\.(0|[1-9][0-9]*)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
        set(_ver_tag_year "${CMAKE_MATCH_1}")
        set(_ver_tag_num "${CMAKE_MATCH_2}")
        set(_ver_distance "${CMAKE_MATCH_3}")
        set(_ver_hash "${CMAKE_MATCH_4}")
        set(_ver_dirty "${CMAKE_MATCH_5}")
        if(_ver_distance EQUAL 0 AND NOT _ver_dirty)
            # Sauberer Stand genau auf dem Tag: die Release selbst
            set(PROJECT_VER "${_ver_tag_year}.${_ver_tag_num}")
        else()
            # Entwicklerversion der nächsten Release
            if(_ver_year GREATER _ver_tag_year)
                set(_ver_next "${_ver_year}.1")
            else()
                math(EXPR _ver_next_num "${_ver_tag_num} + 1")
                set(_ver_next "${_ver_tag_year}.${_ver_next_num}")
            endif()
            set(PROJECT_VER "${_ver_next}.dev${_ver_distance}+g${_ver_hash}")
        endif()
    else()
        # Noch kein passender Tag: Entwicklerversion der ersten Release im Build-Jahr
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${_ver_root}
            OUTPUT_VARIABLE _ver_count
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _ver_count_result)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --always --dirty --abbrev=7
            WORKING_DIRECTORY ${_ver_root}
            OUTPUT_VARIABLE _ver_describe
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _ver_result)
        if(_ver_count_result EQUAL 0 AND _ver_count MATCHES "^[0-9]+$" AND
           _ver_result EQUAL 0 AND _ver_describe MATCHES "^([0-9a-f]+)(-dirty)?$")
            set(_ver_hash "${CMAKE_MATCH_1}")
            set(_ver_dirty "${CMAKE_MATCH_2}")
            set(PROJECT_VER "${_ver_year}.1.dev${_ver_count}+g${_ver_hash}")
        endif()
    endif()
    if(PROJECT_VER AND _ver_dirty)
        string(APPEND PROJECT_VER ".dirty")
    endif()
endif()

if(NOT PROJECT_VER)
    set(PROJECT_VER "${_ver_fallback}")
endif()
message(STATUS "Projektversion (PEP 440): ${PROJECT_VER}")
