if(NOT DEFINED COUNTER_FILE)
    message(FATAL_ERROR "COUNTER_FILE is required")
endif()
if(NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "OUTPUT_HEADER is required")
endif()
if(NOT DEFINED VERSION_MAJOR OR NOT DEFINED VERSION_MINOR OR NOT DEFINED VERSION_PATCH)
    message(FATAL_ERROR "VERSION_MAJOR/MINOR/PATCH are required")
endif()

set(next_build 1)
if(EXISTS "${COUNTER_FILE}")
    file(READ "${COUNTER_FILE}" counter_raw)
    string(STRIP "${counter_raw}" counter_raw)
    if(counter_raw MATCHES "^[0-9]+$")
        math(EXPR next_build "${counter_raw} + 1")
    endif()
endif()

file(WRITE "${COUNTER_FILE}" "${next_build}\n")

set(build_padded "${next_build}")
string(LENGTH "${build_padded}" build_len)
if(build_len EQUAL 1)
    set(build_padded "00${build_padded}")
elseif(build_len EQUAL 2)
    set(build_padded "0${build_padded}")
endif()

set(file_version_str "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${build_padded}")
set(file_version_num "${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},${next_build}")

set(header_content
"#pragma once

#define SECURE_RDP_BUILD_REV_W L\"${build_padded}\"
#define SECURE_RDP_BUILD_REV_A \"${build_padded}\"
#define SECURE_RDP_FILE_VERSION ${file_version_num}
#define SECURE_RDP_FILE_VERSION_STR \"${file_version_str}\"
")

file(WRITE "${OUTPUT_HEADER}" "${header_content}")
