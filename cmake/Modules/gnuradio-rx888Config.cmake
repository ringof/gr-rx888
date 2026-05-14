find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_RX888 gnuradio-rx888)

FIND_PATH(
    GR_RX888_INCLUDE_DIRS
    NAMES gnuradio/rx888/api.h
    HINTS $ENV{RX888_DIR}/include
        ${PC_RX888_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_RX888_LIBRARIES
    NAMES gnuradio-rx888
    HINTS $ENV{RX888_DIR}/lib
        ${PC_RX888_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-rx888Target.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_RX888 DEFAULT_MSG GR_RX888_LIBRARIES GR_RX888_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_RX888_LIBRARIES GR_RX888_INCLUDE_DIRS)
