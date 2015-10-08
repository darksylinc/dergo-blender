
#pragma once

#include "OgrePrerequisites.h"

namespace Network
{
	namespace FromClient
	{
	enum FromClient
	{
		ConnectionTest,
			//"Hello"
		Init,
        Mesh,
			//string meshName (UTF-8)
			//uint32 numVertices
			//uint8 hasColour
			//uint8 numUVs
			//[
			//	float3 position
			//	float3 normal
			//	uchar4 colour
			//	float2 uvN
			//]
			//[uint16 triangle's materialId] (size = numVertices / 3)
		Item,
			//string itemName (UTF-8)
			//string meshName (UTF-8)
			//float3 position
			//float4 quaternion/rotation
			//float3 scale
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
		ConnectionTest,
			//"Hello you too"
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
