# Mock librx888 — same public ABI as the real library plus a small
# introspection surface for QA tests. Built only when WITH_RX888_MOCK=ON.
#
# Produces a shared library named librx888.so (matching the real lib's
# SONAME) so the gr-rx888 source block links and runs against it
# unchanged. Lives in the build tree under tests/mock/ — do NOT install.

# Use the public header from tests/librx888/ verbatim so any drift
# would be immediately visible.
add_library(rx888_mock SHARED
    librx888_mock.c
)

target_include_directories(rx888_mock
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/librx888
)

target_link_libraries(rx888_mock PRIVATE pthread m)

# Mock pretends to be librx888 at the file-name level so -lrx888 works.
# Force default symbol visibility — the parent project applies GR's
# -fvisibility=hidden globally, but the real librx888 ships without it,
# so the mock must match (otherwise rx888_open and friends aren't
# exported and the linked block hits "undefined symbol" at runtime).
set_target_properties(rx888_mock PROPERTIES
    OUTPUT_NAME rx888
    SOVERSION 0
    C_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)
# Belt and suspenders: also strip -fvisibility=hidden from this target's
# compile flags if the parent injected it via add_compile_options().
target_compile_options(rx888_mock PRIVATE -fvisibility=default)

# Surface the variables the top-level CMakeLists.txt expects, mirroring
# what pkg_check_modules(LIBRX888 ...) would set.
set(LIBRX888_LIBRARIES    rx888_mock                          PARENT_SCOPE)
set(LIBRX888_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/tests/librx888
                          ${CMAKE_CURRENT_SOURCE_DIR}          PARENT_SCOPE)
set(LIBRX888_LIBRARY_DIRS ${CMAKE_CURRENT_BINARY_DIR}          PARENT_SCOPE)
set(LIBRX888_CFLAGS_OTHER ""                                   PARENT_SCOPE)

# Build-tree path that ctypes can dlopen at test time.
set(LIBRX888_MOCK_SO_PATH "$<TARGET_FILE:rx888_mock>" PARENT_SCOPE)
