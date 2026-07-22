cmake_minimum_required(VERSION 3.20)

foreach(required CAPABILITIES PUBLIC_HEADER)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing fingerprint contract setting: ${required}")
    endif()
endforeach()

file(SHA256 "${CAPABILITIES}" actual_sha256)
string(TOLOWER "${actual_sha256}" actual_sha256)
file(READ "${PUBLIC_HEADER}" public_header)
string(FIND "${public_header}" "\"${actual_sha256}\"" digest_position)
if(digest_position EQUAL -1)
    message(FATAL_ERROR
        "include/nazm.h does not fingerprint the current capabilities document: "
        "${actual_sha256}")
endif()

message(STATUS "nazm-api-v1 capability digest is current: ${actual_sha256}")
