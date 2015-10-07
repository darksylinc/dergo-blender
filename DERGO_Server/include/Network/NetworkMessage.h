
#pragma once

#include "OgrePrerequisites.h"

namespace Network
{
	namespace FromClient
	{
	enum FromClient
	{
		Init,
        Mesh,
			//string meshName
			//uint32 numVertices
			//uint8 hasColour
			//uint8 numUVs
			//[
			//	float3 position
			//	float3 normal
			//	float3 colour
			//	float2 uvN
			//]
			//uint32 numIndices
			//[uint16 indices] or [uint32 indices]
		Render,
			//uint16 width
			//uint16 height
		NumClientMessages
	};
	}

	namespace FromServer
	{
	enum FromServer
	{
		Result,
			//uint16 width
			//uint16 height
			//[width * height * 3] Image data
		NumServerMessages
	};
	}

#define HEADER_SIZE 5
#pragma pack( push, 1 )
	struct MessageHeader
	{
		Ogre::uint32	sizeBytes;		///Length of the message, without the header.
		Ogre::uint8		messageType;	///@see FromClient & FromServer
	};
#pragma pack( pop )
}
