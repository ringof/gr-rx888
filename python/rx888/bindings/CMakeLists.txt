# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT rx888_sources)
    message(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT rx888_sources)

########################################################################
# Check for pygccxml
########################################################################
gr_python_check_module_raw("pygccxml" "import pygccxml" PYGCCXML_FOUND)

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND rx888_python_files
    source_python.cc python_bindings.cc)

gr_pybind_make_oot(rx888 ../../.. gr::rx888 "${rx888_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(
    TARGET rx888_python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rx888_python>
            ${PROJECT_BINARY_DIR}/test_modules/gnuradio/rx888/)

install(
    TARGETS rx888_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/rx888
    COMPONENT pythonapi)
