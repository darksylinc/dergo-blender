#This scripts will add all the cpp and h files under src and include folders, and
#assumes that your Ogre source code is in Dependencies/Ogre and that:
# In Windows you built Ogre into Dependencies/Ogre/build
# In Linux you built Release into Dependencies/Ogre/build/Release
# In Linux you built Debug into Dependencies/Ogre/build/Debug
#
# If your source code is not at "Dependencies/Ogre"; you can use "mklink /D" to create
# a symbolic link to where the source code is located on Windows.
# On Linux, you can use "ln -s"

project( DERGO_Server )
cmake_minimum_required( VERSION 2.8 )

set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG" )

if( CMAKE_BUILD_TYPE STREQUAL "" )
	set( CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "One of: Debug Release RelWithDebInfo MinSizeRel." FORCE )
endif()

# Determine if we are compiling for a 32bit or 64bit system
include(CheckTypeSize)
CHECK_TYPE_SIZE("void*" ARCH_PTR_SIZE BUILTIN_TYPES_ONLY)
if (ARCH_PTR_SIZE EQUAL 8)
  set(PLATFORM_X64 TRUE)
else ()
  set(PLATFORM_X64 FALSE)
endif ()

include( CMake/Dependencies/libevent.cmake )
include( CMake/Dependencies/OGRE.cmake )

setupLibevent( LIBEVENT_SOURCE, LIBEVENT_BINARIES, LIBEVENT_LIBRARIES )
setupOgre( OGRE_SOURCE, OGRE_BINARIES, OGRE_LIBRARIES TRUE )
if( OGRE_SOURCE )
	message( STATUS "Copying HDR data files from Ogre repository" )
	file( COPY "${OGRE_SOURCE}/Samples/Media/2.0/scripts/materials/HDR"	DESTINATION "${CMAKE_SOURCE_DIR}/bin/Data/Materials" )
	message( STATUS "Copying Parallax Corrected Cubemaps data files from Ogre repository" )
	file( COPY "${OGRE_SOURCE}/Samples/Media/2.0/scripts/materials/LocalCubemaps"	DESTINATION "${CMAKE_SOURCE_DIR}/bin/Data/Materials" )
endif()

# Setup our application
set( EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}" )
if( MSVC )
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	if( NOT PLATFORM_X64 )
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
	endif()
	add_definitions( -DUNICODE -D_UNICODE )
endif()

macro( add_recursive dir retVal )
	file( GLOB_RECURSE ${retVal} ${dir}/*.h ${dir}/*.cpp ${dir}/*.c )
endmacro()

include_directories( "./include" )

add_recursive( ./src SOURCES )
add_recursive( ./include HEADERS )

#add_executable( ${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} )
add_executable( ${PROJECT_NAME} ${SOURCES} ${HEADERS} )
target_link_libraries( ${PROJECT_NAME} ${OGRE_LIBRARIES} ${LIBEVENT_LIBRARIES} )

if( WIN32 )
	target_link_libraries( ${PROJECT_NAME} Ws2_32.lib OpenGL32.lib )
endif()

if( UNIX )
	target_link_libraries( ${PROJECT_NAME} )
endif()
