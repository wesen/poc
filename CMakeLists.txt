cmake_minimum_required(VERSION 3.15)
project(poc C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

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

add_compile_options(-Wall -O2)

set(MP3_SRC
        mp3-read.c
        mp3-write.c
        mp3.c
        aq.c
        id3.c
        )

set(NETWORK_SRC
        network.c
        network4.c
        network6.c
        )

set(RTP_SRC
        rtp.c
        rtp-rb.c
        )
set(UTILS_SRC
        pack.c
        bv.c
        sig_set_handler.c
        dlist.c
        file.c
        buf.c
        crc32.c
        misc.c
        )
set(FEC_SRC
        galois.c
        matrix.c
        fec.c
        fec-pkt.c
        fec-rb.c
        fec-group.c
        )
set(OGG_SRC
        ogg.c
        vorbis.c
        ogg-read.c
        ogg-write.c
        vorbis-read.c
        )

set(MP3RTP_SRC
        ${NETWORK_SRC}
        ${MP3_SRC}
        ${RTP_SRC}
        ${UTILS_SRC}
        )

YACC_FILE(mp3cue_yacc.y)
LEX_FILE(mp3cue_lex.l)

add_executable(mp3cue
        ${MP3_SRC}
        ${UTILS_SRC}
        mp3cue-main.c
        mp3cue_yacc.c
        mp3cue_lex.c
        )

add_executable(mp3cut
        ${MP3_SRC}
        ${UTILS_SRC}
        mp3cut.c)

add_executable(mp3length
        ${MP3_SRC}
        ${UTILS_SRC}
        mp3length.c)

add_executable(poc-2250
        ${MP3RTP_SRC}
        poc-2250.c)

add_executable(poc-3119
        ${MP3RTP_SRC}
        poc-3119.c)

add_executable(poc-fec
        ${MP3RTP_SRC}
        ${FEC_SRC}
        poc-fec.c)

add_executable(poc-http
        ${MP3RTP_SRC}
        http.c
        poc-http.c)

add_executable(pogg-http
        ${NETWORK_SRC}
        ${UTILS_SRC}
        ${OGG_SRC}
        http.c
        pogg-http.c)

add_executable(pob-2250
        ${MP3RTP_SRC}
        pob-2250-rb.c)

add_executable(pob-3119
        ${MP3RTP_SRC}
        pob-3119-rb.c)

add_executable(pob-fec
        ${MP3RTP_SRC}
        ${FEC_SRC}
        pob-fec.c)
