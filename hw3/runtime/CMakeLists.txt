# Detect architecture for macOS (use i386 for 32-bit instead of x86_64)
if(APPLE)
    set(ARCH_FLAGS "-arch x86_64")
endif()

# Common compiler flags
set(DISABLE_WARNINGS "-Wno-shift-negative-value")
set(COMMON_FLAGS "${DISABLE_WARNINGS} -g -fstack-protector-all ${ARCH_FLAGS}")

# Production flags (used by default)
set(PROD_FLAGS "${COMMON_FLAGS} -DLAMA_ENV")

# Set C standard
set(CMAKE_C_STANDARD 11)

# Create a static library 'runtime'
add_library(runtime STATIC
        gc.c
        gc.h
        runtime.c
        runtime.h
        runtime_common.h
        printf.S
)

# Apply compiler flags to the library
target_compile_options(runtime PRIVATE
        ${PROD_FLAGS}
)

# Special handling for assembly file
set_source_files_properties(printf.S PROPERTIES
        COMPILE_FLAGS "-Wa,--noexecstack -x assembler-with-cpp -g"
)

# Set output name to match original (optional, as linking will work either way)
set_target_properties(runtime PROPERTIES
        OUTPUT_NAME "runtime"
        ARCHIVE_OUTPUT_NAME "runtime"
)