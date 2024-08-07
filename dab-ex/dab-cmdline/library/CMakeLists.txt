cmake_minimum_required( VERSION 2.8.11 )

project(dab_lib)
set (objectName dab_lib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
    add_definitions(-D_USE_MATH_DEFINES -DDAB_API_EXPORT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_definitions ( -Wall -O3 -DDAB_API_EXPORT)
endif()

if(MINGW)
    add_definitions ( -municode)
endif()

if (X64_DEFINED)
   set(X64_DEFINED true)
endif ()

if (RPI_DEFINED)
   set(RPI_DEFINED true)
endif ()

########################################################################
# select the release build type by default to get optimization flags
########################################################################
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

### make sure our local CMake Modules path comes first
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake/Modules)

######################################################################
#
########################################################################
find_package (PkgConfig)

find_package(FFTW3f)
if (NOT FFTW3F_FOUND)
    message(FATAL_ERROR "please install FFTW3")
endif ()

find_package(Faad)
if (NOT FAAD_FOUND )
    message(FATAL_ERROR "please install libfaad")
endif ()

find_package(zlib)
if (NOT ZLIB_FOUND)
    message(FATAL_ERROR "please install libz")
endif ()
list(APPEND extraLibs ${ZLIB_LIBRARY})

find_library (PTHREADS pthread)
if (NOT(PTHREADS))
    message (FATAL_ERROR "please install libpthread")
else (NOT(PTHREADS))
    set (extraLibs ${extraLibs} ${PTHREADS})
endif (NOT(PTHREADS))

#######################################################################
#
#    Here we really start

include_directories (
    ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
    .
    ./
    ../
    ./includes
    ./includes/ofdm
    ./includes/backend
    ./includes/backend/audio
    ./includes/backend/data
    ./includes/backend/data/mot
    ./includes/backend/data/journaline
    ./includes/support
    ./includes/support/viterbi-spiral
    /usr/include/
)

set (${objectName}_HDRS
    ${${objectName}_HDRS}
    ../dab-api.h
    ../device-handler.h
    ../ringbuffer.h
    ./includes/dab-constants.h
    ./includes/dab-processor.h
    ./includes/ofdm/phasereference.h
    ./includes/ofdm/phasetable.h
    ./includes/ofdm/freq-interleaver.h
    ./includes/ofdm/ofdm-decoder.h
    ./includes/ofdm/sample-reader.h
    ./includes/ofdm/timesyncer.h
    ./includes/ofdm/fic-handler.h
    ./includes/ofdm/fib-processor.h
    ./includes/ofdm/tii-detector.h
    ./includes/backend/firecode-checker.h
    ./includes/backend/backend-base.h
    ./includes/backend/charsets.h
    ./includes/backend/galois.h
    ./includes/backend/reed-solomon.h
    ./includes/backend/msc-handler.h
    ./includes/backend/virtual-backend.h
    ./includes/backend/audio-backend.h
    ./includes/backend/data-backend.h
    ./includes/backend/audio/faad-decoder.h
    ./includes/backend/audio/mp4processor.h
    ./includes/backend/audio/mp2processor.h
    ./includes/backend/data/virtual-datahandler.h
    ./includes/backend/data/pad-handler.h
    ./includes/backend/data/tdc-datahandler.h
    ./includes/backend/data/data-processor.h
    ./includes/backend/data/mot/mot-handler.h
    ./includes/backend/data/mot/mot-dir.h
    ./includes/backend/data/mot/mot-object.h
    ./includes/support/band-handler.h
    ./includes/support/protTables.h
    #	 ./includes/support/viterbi-handler.h
    ./includes/support/protection.h
    ./includes/support/dab-semaphore.h
    ./includes/support/uep-protection.h
    ./includes/support/eep-protection.h
    ./includes/support/fft-handler.h
    ./includes/support/dab-params.h
    ./includes/support/tii_table.h
    ./includes/support/viterbi-spiral/viterbi-spiral.h
)

set (${objectName}_SRCS
    ${${objectName}_SRCS}
    ./dab-api.cpp
    ./src/dab-processor.cpp
    ./src/ofdm/ofdm-decoder.cpp
    ./src/ofdm/phasereference.cpp
    ./src/ofdm/phasetable.cpp
    ./src/ofdm/freq-interleaver.cpp
    ./src/ofdm/ofdm-decoder.cpp
    ./src/ofdm/sample-reader.cpp
    ./src/ofdm/timesyncer.cpp
    ./src/ofdm/fic-handler.cpp
    ./src/ofdm/fib-processor.cpp
    ./src/ofdm/tii-detector.cpp
    ./src/backend/firecode-checker.cpp
    ./src/backend/backend-base.cpp
    ./src/backend/charsets.cpp
    ./src/backend/galois.cpp
    ./src/backend/reed-solomon.cpp
    ./src/backend/msc-handler.cpp
    ./src/backend/virtual-backend.cpp
    ./src/backend/audio-backend.cpp
    ./src/backend/data-backend.cpp
    ./src/backend/audio/mp4processor.cpp
    ./src/backend/audio/mp2processor.cpp
    ./src/backend/data/virtual-datahandler.cpp
    ./src/backend/data/tdc-datahandler.cpp
    ./src/backend/data/pad-handler.cpp
    ./src/backend/data/data-processor.cpp
    ./src/backend/data/mot/mot-handler.cpp
    ./src/backend/data/mot/mot-dir.cpp
    ./src/backend/data/mot/mot-object.cpp
    ./src/support/band-handler.cpp
    #	 ./src/support/viterbi-handler.cpp
    ./src/support/protection.cpp
    ./src/support/protTables.cpp
    ./src/support/eep-protection.cpp
    ./src/support/uep-protection.cpp
    ./src/support/fft-handler.cpp
    ./src/support/dab-params.cpp
    ./src/support/tii_table.cpp
    ./src/support/viterbi-spiral/viterbi-spiral.cpp
)

if (X64_DEFINED)
    set (${objectName}_SRCS
        ${${objectName}_SRCS}
        ./src/support/viterbi-spiral/spiral-sse.c
    )
    set (${objectName}_HDRS
        ${${objectName}_HDRS}
        ./src/support/viterbi-spiral/spiral-sse.h
    )
    add_definitions (-DSSE_AVAILABLE)
elseif (RPI_DEFINED)
    set (${objectName}_SRCS
        ${${objectName}_SRCS}
        ./src/support/viterbi-spiral/spiral-neon.c
    )
    set (${objectName}_HDRS
        ${${objectName}_HDRS}
        ./src/support/viterbi-spiral/spiral-neon.h
    )
    add_definitions (-DNEON_AVAILABLE)
# compiler options moved below add_executable .. and target specific
#set (CMAKE_CXX_FLAGS "${CMAKE_XCC_FLAGS} -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")
#set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")
else (X64_DEFINED)
    set (${objectName}_SRCS
        ${${objectName}_SRCS}
        ./src/support/viterbi-spiral/spiral-no-sse.c
    )
    set (${objectName}_HDRS
        ${${objectName}_HDRS}
        ./src/support/viterbi-spiral/spiral-no-sse.h
    )
endif (X64_DEFINED)

include_directories (
    ${FFTW_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${FAAD_INCLUDE_DIRS}
    ${SNDFILES_INCLUDE_DIRS}
)

#####################################################################

add_library (${objectName} SHARED
    ${${objectName}_SRCS}
)

if (RPI_DEFINED)
    # compiler options seem changed with gcc versions. gcc 8.3.0 on Raspbian didn't accept -mcpu ..
    # test options with "make VERBOSE=1"
    target_compile_options(${objectName}  PRIVATE -march=armv7-a -mfloat-abi=hard -mfpu=neon-vfpv4 )
endif()

target_link_libraries (${objectName}
    ${FFTW3F_LIBRARIES}
    ${extraLibs}
    ${FAAD_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

install(TARGETS ${objectName} DESTINATION ${LIB_INSTALL_DIR})
install(FILES ${${objectName}_HDRS} DESTINATION include/${PROJECT_NAME})

########################################################################
# Create uninstall target
########################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
