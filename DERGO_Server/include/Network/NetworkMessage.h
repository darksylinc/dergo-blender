
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
			//uint64 meshId
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
			//uint64 meshId
			//uint64 itemId
			//string itemName (UTF-8)
			//float3 position
			//float4 quaternion/rotation
			//float3 scale
		ItemRemove,
			//uint64 meshId
			//uint64 itemId
		Light,
			//uint64 lampId
			//string lampName (UTF-8)
			//uint8 lightType
			//uint8 castShadow
			//float3 colour
			//float power
			//uint8 useRangeInsteadOfRadius
			//float radius
			//float threshold (not used if useRangeInsteadOfRadius == 1)
			//float spotInnerAngle
			//float spotOuterAngle
			//float spotFalloff
			//float3 position
			//float4 quaternion/rotation
		Reset,
		Render,
			//uint8 returnResult
			//uint64 windowId	//Not used if returnResult != 0
			//uint16 width
			//uint16 height
			//float fov (degrees)
			//float nearClip
			//float farClip
			//float3 camPos
			//float3 camUp (not normalized!)
			//float3 camRight (not normalized!)
			//float3 -camForward (not normalized!)
			//uint8 isPerspectiveMode //0 ortho, 1 perspective.
		InitAsync,
		FinishAsync,
		NumClientMessages
	};
	}

	namespace FromServer
	{
	enum FromServer
	{
		ConnectionTest,
			//"Hello you too"
		Resync, /// Tels the client we want a Reset.
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
