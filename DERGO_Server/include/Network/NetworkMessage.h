
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
			//uint32 meshId
			//string meshName (UTF-8)
			//uint32 numVertices
			//uint8 hasColour
			//uint8 numUVs
			//uint8 tangentUVSource (255 = disable tangents)
			//[
			//	float3 position
			//	float3 normal
			//	uchar4 colour
			//	float2 uvN
			//]
			//uint16 numMaterials
			//[uint32 materialIds]	(Table with size = numMaterials)
			//[uint16 triangle's materialId] (size = numVertices / 3; ID is relative to the table)
		Item,
			//uint32 meshId
			//uint32 itemId
			//string itemName (UTF-8)
			//float3 position
			//float4 quaternion/rotation
			//float3 scale
		ItemRemove,
			//uint32 meshId
			//uint32 itemId
		Light,
			//uint32 lampId
			//string lampName (UTF-8)
			//uint8 lightType
			//uint8 castShadow
			//uint8 useNegative
			//float3 colour
			//float power
			//float3 position
			//float4 quaternion/rotation
			//float radius
			//float rangeOrThreshold
			//	float spotOuterAngle	[Only sent if lightType = spot]
			//	float spotInnerAngle	[Only sent if lightType = spot]
			//	float spotFalloff		[Only sent if lightType = spot]
		LightRemove,
			//uint32 lampId
		Material,
			//uint32 materialId
			//string materialName (UTF-8)
			//uint32 brdfType
			//uint8 transparencyMode
			//	float transparencyValue		[Only sent if transparencyMode != None]
			//	uint8 useAlphaFromTexture	[Only sent if transparencyMode != None]
			//float3 kD
			//float3 kS
			//float roughness
			//float normalMapWeight
			//float3 fresnelCoeff
		MaterialTexture,
			//uint32 materialId
			//uint8	slot
			//uint64 textureId
		Texture,
			//uint64 textureId
			//string texturePath
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
