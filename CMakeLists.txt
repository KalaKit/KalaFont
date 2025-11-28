cmake_minimum_required(VERSION 3.29.2)

set(PROGRAM_VERSION_NUMBER 1.0.0.0)

project("KalaFont" VERSION ${PROGRAM_VERSION_NUMBER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform Detection
if (WIN32)
    message(STATUS "[KalaFont] Platform = Windows")
else()
    message(FATAL_ERROR "[KalaFont] Unsupported platform. Only Windows is supported.")
endif()

# Build Type Detection
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(IS_RELEASE TRUE)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'! Must be Debug, Release, or RelWithDebInfo.")
endif()

# Runtime lib handling for MSVC
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Directory Setup
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(EXT_SHARED_DIR "${CMAKE_SOURCE_DIR}/_external_shared")

set(EXT_FREETYPE_DIR "${EXT_SHARED_DIR}/FreeType")
set(EXT_CLI_DIR "${EXT_SHARED_DIR}/KalaCLI")

# Library Paths
if(IS_RELEASE)
	set(FREETYPE_LIBRARY_PATH "${EXT_FREETYPE_DIR}/release/freetype.lib")
	set(CLI_LIBRARY_PATH "${EXT_CLI_DIR}/release/KalaCLI1.lib")
else()
	set(FREETYPE_LIBRARY_PATH "${EXT_FREETYPE_DIR}/debug/freetyped.lib")
	set(CLI_LIBRARY_PATH "${EXT_CLI_DIR}/debug/KalaCLI1d.lib")
endif()

# Source Files
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
)

# Executable
add_executable(KalaFont ${SOURCE_FILES})

# Enable exception handling for MSVC
if (MSVC)
    target_compile_options(KalaFont PRIVATE /EHsc)
endif()

set_target_properties(KalaFont PROPERTIES OUTPUT_NAME "KalaFont")
target_compile_features(KalaFont PRIVATE cxx_std_20)

# Includes
file(GLOB_RECURSE HEADERS
	configure_depends
	"${CMAKE_SOURCE_DIR}/include/*.hpp"
)
target_sources(KalaFont PRIVATE ${HEADERS})
target_include_directories(KalaFont PRIVATE
	"${INCLUDE_DIR}"
	"${EXT_SHARED_DIR}"
)

# Preprocessor Defines
target_compile_definitions(KalaFont PRIVATE 
	WIN32_LEAN_AND_MEAN
	NOMINMAX)
	
# Link libraries
target_link_libraries(KalaFont PRIVATE
	opengl32
	${FREETYPE_LIBRARY_PATH}
	${CLI_LIBRARY_PATH})

# Hide console in release mode
#if(IS_RELEASE)
#    set_target_properties(KalaFont PROPERTIES WIN32_EXECUTABLE TRUE)
#    if (MSVC)
#        set_target_properties(KalaFont PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
#    endif()
#endif()

# Installation
set(CMAKE_INSTALL_BINDIR bin)
install(TARGETS KalaFont DESTINATION ${CMAKE_INSTALL_BINDIR})

# Copy external binaries
if(IS_RELEASE)
	set(BIN_FREETYPE "${EXT_FREETYPE_DIR}/release")
	set(BIN_KALACLI "${EXT_CLI_DIR}/release")
else()
	set(BIN_FREETYPE "${EXT_FREETYPE_DIR}/debug")
	set(BIN_KALACLI "${EXT_CLI_DIR}/debug")
endif()

if (IS_RELEASE)
	file(GLOB BIN_FILES
		"${BIN_FREETYPE}/*.dll"
		"${BIN_KALACLI}/*.dll"
	)
else()
	file(GLOB BIN_FILES
		"${BIN_FREETYPE}/*.dll"
		"${BIN_FREETYPE}/*.pdb"
		"${BIN_KALACLI}/*.dll"
		"${BIN_KALACLI}/*.dll"
	)
endif()

set(BIN_TARGET_DIR "$<TARGET_FILE_DIR:KalaFont>")

foreach(BIN_FILE ${BIN_FILES})
    get_filename_component(BIN_NAME ${BIN_FILE} NAME)
    add_custom_command(TARGET KalaFont POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
			"${BIN_FILE}"
			"${BIN_TARGET_DIR}/${BIN_NAME}"
    )
endforeach()

# Copy important files
set(COPY_TARGETS README.md LICENSE.md)
foreach(FILE ${COPY_TARGETS})
add_custom_command(TARGET KalaFont POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f 
        "$<TARGET_FILE_DIR:KalaFont>/${FILE}"
    COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_SOURCE_DIR}/${FILE}"
		"$<TARGET_FILE_DIR:KalaFont>/${FILE}"
)
endforeach()

# Copy entire docs folder
add_custom_command(TARGET KalaFont POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_FILE_DIR:KalaFont>/docs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/docs"
        "$<TARGET_FILE_DIR:KalaFont>/docs"
)

# Package
include(CPack)
