cmake_minimum_required(VERSION 3.5)
project(rastertoricohpjl)

set (CUPS_SRC_DIR ${SRC_DIR}/cups-2.1.1)
set (UNICODE_SRC_DIR ${SRC_DIR}/unicode)

find_library(CUPS_LIB cups)
find_library(CUPSIMAGE_LIB cupsimage)

include_directories(${CUPS_SRC_DIR})

set(SOURCES
	rastertoricohpjl.c
	libjbig/jbig.c
	libjbig/jbig_ar.c
)

add_executable(rastertoricohpjl ${SOURCES})
target_link_libraries(rastertoricohpjl ${CUPS_LIB} ${CUPSIMAGE_LIB})
