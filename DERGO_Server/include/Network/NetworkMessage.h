
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
		WorldParams,
			//float3 skyColour
			//float skyPower
			//float3 upperHemiColour
			//float upperHemiPower
			//float3 lowerHemiColour
			//float lowerHemiPower
			//float3 hemisphereDir
			//float exposure
			//float minAutoExposure
			//float maxAutoExposure
			//float bloomThreshold
			//float envmapScale
		InstantRadiosity,
			//float numRays
			//float numRayBounces
			//float survivingRayFraction
			//float cellSize
			//uint8 numSpreadIterations
			//float spreadThreshold
			//float bias
			//float vplMaxRange
			//float vplConstAtten
			//float vplLinearAtten
			//float vplQuadAtten
			//float vplThreshold
			//float vplPowerBoost
			//uint8 vplUseIntensityForMaxRange
			//float vplIntensityRangeMultiplier
			//uint8 useIrradianceVolumes
        Mesh,
			//uint32 meshId
			//string meshName (UTF-8)
			//uint32 numFaces
			//uint32 numRawVertices
			//uint8 hasColour
			//uint8 numUVs
			//uint8 tangentUVSource (255 = disable tangents)
			//[
			//	uint4	vertexIndices
			//	float3	faceNormal
			//	ushort	materialId -> Last bit is use_smooth
			//	uint8_t	numIndicesInFace;
			//]
			//[
			//	float3 vertexColour[numFaces][4]
			//][hasColour]
			//[
			//	float2 uv[numFaces][4]
			//][numUVs]
			//[
			//	float3 position
			//	float3 normal
			//][numRawVertices]
			//uint16 numMaterials
			//[uint32 materialIds]	(Table with size = numMaterials)
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
			//uint8 materialWorkflow
			//uint8 cullMode
			//uint8 cullModeShadow
			//uint8 twoSided
			//uint8 transparencyMode
			//	float transparencyValue		[Only sent if transparencyMode != None]
			//	uint8 useAlphaFromTexture	[Only sent if transparencyMode != None]
			//uint8 alphaTestCmpFunc
			//	float alphaTestThreshold	[Only if alphaTestCmpFunc != CMPF_ALWAYS_PASS && != *_FAIL]
			//float3 kD
			//float3 kS
			//float roughness
			//float normalMapWeight
			//float3 fresnelCoeff/metalness
			//[
			//  uint8 (filter << 6u) | (addressU << 2u) | (addressV & 0x03)
			//  uint8 uvSet
			//  float4 borderColour [Only if addressU or addressV == TAM_BORDER]
			//] (repeated NumPbsTextures times)
			//[
			//	uint8 blendMode (only for details maps)
			//	float weight
			//	float2 offset
			//	float2 scale
			//] (repeated 8 times; first detail maps, then detail normal maps)
		MaterialTexture,
			//uint32 materialId
			//uint8	slot
			//uint64 textureId
			//uint8 textureMapType
		Texture,
			//uint64 textureId
			//uint8 textureMapType
			//string texturePath
		Reset,
		Render,
			//uint8 returnResult
			//uint64 windowId	//Not used if returnResult != 0
			//uint16 width
			//uint16 height
			//float focalLength (degrees)
			//float sensorSize
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
