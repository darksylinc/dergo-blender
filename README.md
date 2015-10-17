# DERGO for Blender #

This is an experimental project to use [OGRE3D](www.ogre3d.org) as an external renderer for Blender 2.7x using the RenderEngine API in Python.

It consists of both a client written in Python (the Blender plugin) and a command line server that runs OGRE.

## Requirements##
* CMake
* Windows only: At least MSVC 2008 (Express Editions work)
* Linux only: GCC or Clang. QtCreator is highly recommended for nice GUI/IDE development.
* Ogre source code (not the SDK!). Grab it from [https://bitbucket.org/sinbad/ogre/](https://bitbucket.org/sinbad/ogre/). The Dependencies are conveniently in this repo: [https://bitbucket.org/cabalistic/ogredeps](https://bitbucket.org/cabalistic/ogredeps). [OGRE Build instructions](http://www.ogre3d.org/tikiwiki/CMake+Quick+Start+Guide). The CMake scripts expects the source code in Dependencies/Ogre.
    * Windows only: OGRE's CMake build must go in Dependencies/Ogre/build
    * Linux only: OGRE's CMake build must go in Dependencies/Ogre/build/Release (or Debug / RelWithDebInfo / MinSizeRel)
* libevent, using git revision 6e7a580c15a7722f94630184189ae3af6afabdd9.
    * Windows only: libevent CMake build must go in Dependencies/libevent/build

## Compiling ##
Build the CMake folder into the folder "build". Then compile it (make in Linux, open the generated sln file in Visual Studio, etc)
The CMake script will automatically copy the DLLs in Windows the the bin folder, including Plugins. It will also autogenerate the Plugins.cfg file.

## Useful CMake options ##
* OGRE_SOURCE. Path to Ogre; if it's somewhere else other than Dependencies/Ogre
* OGRE_BINARIES. Path to where CMake configured Ogre.
* LIBEVENT_SOURCE. Path to where libevent; if it's somewhere else other than Dependencies/libevent.
* LIBEVENT_BINARIES. Path to where CMake configured libevent.
* Linux only: CMAKE_BUILD_TYPE. Must be Release, Debug, RelWithDebInfo or MinSizeRel.

## WARNING ##
The server code was not coded with security in mind and assumes a strong degree of trust from the client.
It is likely possible to send crafted packets to the server that could compromise the security of the system.
It is your responsability to ensure the server is ran in a trusted environment and/or that you've firewalled it
to only accept connections from a very limited amount of sources (e.g. accept only from localhost).
I am not responsible for whatever happens to your system because of the use of this program.

## License ##
See LICENSE_Client.txt (GPL) and LICENSE_Server (MIT)