cmake_minimum_required(VERSION 3.15)
project(poc C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(lib)

find_program(LEX_EXE
        flex
        )
if (LEX_EXE STREQUAL "LEX_EXE-NOTFOUND")
    message(FATAL_ERROR "dear user, please install flex!")
endif (LEX_EXE STREQUAL "LEX_EXE-NOTFOUND")

find_program(YACC_EXE
        bison
        )
if (YACC_EXE STREQUAL "YACC_EXE-NOTFOUND")
    message(FATAL_ERROR "dear user, please install bison!")
endif (YACC_EXE STREQUAL "YACC_EXE-NOTFOUND")

# reuseable cmake macro for yacc
MACRO(YACC_FILE _filename)
    GET_FILENAME_COMPONENT(_basename ${_filename} NAME_WE)
    ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.c
            ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.h
            COMMAND ${YACC_EXE} -d
            --output=${_basename}.c
            ${CMAKE_CURRENT_SOURCE_DIR}/${_filename}
            DEPENDS ${_filename}
    )
ENDMACRO(YACC_FILE)

# reuseable cmake macro for lex
MACRO(LEX_FILE _filename)
    GET_FILENAME_COMPONENT(_basename ${_filename} NAME_WE)
    ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.c
            COMMAND ${LEX_EXE}
            -o${_basename}.c
            ${CMAKE_CURRENT_SOURCE_DIR}/${_filename}
            DEPENDS ${_filename})
ENDMACRO(LEX_FILE)

add_compile_options(-Wall)

set(MP3_SRC
        lib/mp3/mp3-read.c
        lib/mp3/mp3-write.c
        lib/mp3/mp3.c
        lib/mp3/aq.c
        lib/mp3/id3.c
        )

set(NETWORK_SRC
        lib/system/network.c
        lib/system/network4.c
        lib/system/network6.c
        )

set(RTP_SRC
        lib/rtp/rtp.c
        lib/rtp/rtp-rb.c
        )
set(UTILS_SRC
        lib/pack.c
        lib/data-structures/bv.c
        lib/system/sig_set_handler.c
        lib/data-structures/dlist.c
        lib/system/file.c
        lib/data-structures/buf.c
        lib/math/crc32.c
        lib/system/misc.c
        )
set(FEC_SRC
        lib/math/galois.c
        lib/math/matrix.c
        lib/fec/fec.c
        lib/fec/fec-pkt.c
        lib/fec/fec-rb.c
        lib/fec/fec-group.c
        )
set(OGG_SRC
        lib/ogg/ogg.c
        lib/ogg/vorbis.c
        lib/ogg/ogg-read.c
        lib/ogg/ogg-write.c
        lib/ogg/vorbis-read.c
        )

set(MP3RTP_SRC
        ${NETWORK_SRC}
        ${MP3_SRC}
        ${RTP_SRC}
        ${UTILS_SRC}
        )

YACC_FILE(tools/mp3cue/mp3cue_yacc.y)
LEX_FILE(tools/mp3cue/mp3cue_lex.l)

add_executable(mp3cue
        ${MP3_SRC}
        ${UTILS_SRC}
        tools/mp3cue/mp3cue-main.c
        mp3cue_yacc.c
        mp3cue_lex.c
        )
# add tools/mp3cue to include path for mp3cue_yacc.c
target_include_directories(mp3cue PRIVATE tools/mp3cue)

add_executable(mp3cut
        ${MP3_SRC}
        ${UTILS_SRC}
        tools/mp3cut.c)

add_executable(mp3length
        ${MP3_SRC}
        ${UTILS_SRC}
        tools/mp3length.c)

add_executable(poc-2250
        ${MP3RTP_SRC}
        servers/poc-2250.c)

add_executable(poc-3119
        ${MP3RTP_SRC}
        servers/poc-3119.c)

add_executable(poc-fec
        ${MP3RTP_SRC}
        ${FEC_SRC}
        servers/poc-fec.c)

add_executable(poc-http
        ${MP3RTP_SRC}
        lib/system/http.c
        servers/poc-http.c)

add_executable(pogg-http
        ${NETWORK_SRC}
        ${UTILS_SRC}
        ${OGG_SRC}
        lib/system/http.c
        servers/pogg-http.c)

add_executable(pob-2250
        ${MP3RTP_SRC}
        clients/pob-2250-rb.c)

add_executable(pob-3119
        ${MP3RTP_SRC}
        clients/pob-3119-rb.c)

add_executable(pob-fec
        ${MP3RTP_SRC}
        ${FEC_SRC}
        clients/pob-fec.c)
