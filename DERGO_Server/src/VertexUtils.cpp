
#include "VertexUtils.h"

#include "OgreVector2.h"
#include "OgreVector3.h"

#include "OgreHardwareVertexBuffer.h"

namespace DERGO
{
#if defined( _MSC_VER ) && _MSC_VER < 1600
    //Do not clash with Ogre::uint32_t & co. defined in OgreMain/include/Hash/MurmurHash3.h
    using ::uint32_t;
    using ::uint8_t;
#endif

	void VertexUtils::deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							   const BlenderFace *faces, uint32_t numFaces,
							   const BlenderRawVertex *blenderRawVertices,
							   uint16_t * RESTRICT_ALIAS materialIds )
	{
		using namespace Ogre;

		for( ::uint32_t i=0; i<numFaces; ++i )
		{
			Vector3 * RESTRICT_ALIAS vPos[3];
			Vector3 * RESTRICT_ALIAS vNormal[3];

			for( int j=0; j<3; ++j )
			{
				vPos[j]		= reinterpret_cast<Vector3 * RESTRICT_ALIAS>(
								dstData + j * bytesPerVertex );
				vNormal[j]	= reinterpret_cast<Vector3 * RESTRICT_ALIAS>(
								dstData + sizeof(Ogre::Vector3) + j * bytesPerVertex );
			}

			const ::uint32_t k0 = faces[i].vertexIndex[0];
			const ::uint32_t k1 = faces[i].vertexIndex[1];
			const ::uint32_t k2 = faces[i].vertexIndex[2];

			const bool useSmooth = (faces[i].materialId & 0x8000) != 0;

			*vPos[0]	= blenderRawVertices[k0].vPos;
			*vNormal[0]	= useSmooth ? blenderRawVertices[k0].vNormal : faces[i].faceNormal;
			*vPos[1]	= blenderRawVertices[k1].vPos;
			*vNormal[1]	= useSmooth ? blenderRawVertices[k1].vNormal : faces[i].faceNormal;
			*vPos[2]	= blenderRawVertices[k2].vPos;
			*vNormal[2]	= useSmooth ? blenderRawVertices[k2].vNormal : faces[i].faceNormal;

			*materialIds++ = faces[i].materialId & 0x7FFF;

			dstData += bytesPerVertex * 3u;

			if( faces[i].numIndicesInFace == 4 )
			{
				const ::uint32_t k3 = faces[i].vertexIndex[3];

				for( int j=0; j<3; ++j )
				{
					vPos[j]		= reinterpret_cast<Vector3 * RESTRICT_ALIAS>(
									dstData + j * bytesPerVertex );
					vNormal[j]	= reinterpret_cast<Vector3 * RESTRICT_ALIAS>(
									dstData + sizeof(Ogre::Vector3) + j * bytesPerVertex );
				}

				*vPos[0]	= blenderRawVertices[k0].vPos;
				*vNormal[0]	= useSmooth ? blenderRawVertices[k0].vNormal : faces[i].faceNormal;
				*vPos[1]	= blenderRawVertices[k2].vPos;
				*vNormal[1]	= useSmooth ? blenderRawVertices[k2].vNormal : faces[i].faceNormal;
				*vPos[2]	= blenderRawVertices[k3].vPos;
				*vNormal[2]	= useSmooth ? blenderRawVertices[k3].vNormal : faces[i].faceNormal;

				*materialIds++ = faces[i].materialId & 0x7FFF;

				dstData += bytesPerVertex * 3u;
			}
		}
	}
	//-------------------------------------------------------------------------
	void VertexUtils::deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							   const BlenderFace *faces, uint32_t numFaces,
							   const BlenderFaceColour *facesColour )
	{
		using namespace Ogre;

		for( ::uint32_t i=0; i<numFaces; ++i )
		{
			uint8_t * RESTRICT_ALIAS diffuseColour[3];

			for( int j=0; j<3; ++j )
			{
				diffuseColour[j] = reinterpret_cast<uint8_t * RESTRICT_ALIAS>(
									dstData + sizeof(Ogre::Vector3) * 2u + j * bytesPerVertex );
			}

			diffuseColour[0][0] = static_cast<uint8_t>( facesColour[i].colour[0].x * 255.0f + 0.5f );
			diffuseColour[0][1] = static_cast<uint8_t>( facesColour[i].colour[0].y * 255.0f + 0.5f );
			diffuseColour[0][2] = static_cast<uint8_t>( facesColour[i].colour[0].z * 255.0f + 0.5f );
			diffuseColour[0][3] = 255;

			diffuseColour[1][0] = static_cast<uint8_t>( facesColour[i].colour[1].x * 255.0f + 0.5f );
			diffuseColour[1][1] = static_cast<uint8_t>( facesColour[i].colour[1].y * 255.0f + 0.5f );
			diffuseColour[1][2] = static_cast<uint8_t>( facesColour[i].colour[1].z * 255.0f + 0.5f );
			diffuseColour[1][3] = 255;

			diffuseColour[2][0] = static_cast<uint8_t>( facesColour[i].colour[2].x * 255.0f + 0.5f );
			diffuseColour[2][1] = static_cast<uint8_t>( facesColour[i].colour[2].y * 255.0f + 0.5f );
			diffuseColour[2][2] = static_cast<uint8_t>( facesColour[i].colour[2].z * 255.0f + 0.5f );
			diffuseColour[2][3] = 255;

			dstData += bytesPerVertex * 3u;

			if( faces[i].numIndicesInFace == 4 )
			{
				for( int j=0; j<3; ++j )
				{
					diffuseColour[j] = reinterpret_cast<uint8_t * RESTRICT_ALIAS>(
										dstData + sizeof(Ogre::Vector3) * 2u + j * bytesPerVertex );
				}

				diffuseColour[0][0] = static_cast<uint8_t>( facesColour[i].colour[0].x * 255.0f + 0.5f );
				diffuseColour[0][1] = static_cast<uint8_t>( facesColour[i].colour[0].y * 255.0f + 0.5f );
				diffuseColour[0][2] = static_cast<uint8_t>( facesColour[i].colour[0].z * 255.0f + 0.5f );
				diffuseColour[0][3] = 255;

				diffuseColour[1][0] = static_cast<uint8_t>( facesColour[i].colour[2].x * 255.0f + 0.5f );
				diffuseColour[1][1] = static_cast<uint8_t>( facesColour[i].colour[2].y * 255.0f + 0.5f );
				diffuseColour[1][2] = static_cast<uint8_t>( facesColour[i].colour[2].z * 255.0f + 0.5f );
				diffuseColour[1][3] = 255;

				diffuseColour[2][0] = static_cast<uint8_t>( facesColour[i].colour[3].x * 255.0f + 0.5f );
				diffuseColour[2][1] = static_cast<uint8_t>( facesColour[i].colour[3].y * 255.0f + 0.5f );
				diffuseColour[2][2] = static_cast<uint8_t>( facesColour[i].colour[3].z * 255.0f + 0.5f );
				diffuseColour[2][3] = 255;

				dstData += bytesPerVertex * 3u;
			}
		}
	}
	//-------------------------------------------------------------------------
	void VertexUtils::deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							   const BlenderFace *faces, uint32_t numFaces,
							   const BlenderFaceUv *faceUv, uint32_t uvStride )
	{
		using namespace Ogre;

		for( ::uint32_t i=0; i<numFaces; ++i )
		{
			Vector2 * RESTRICT_ALIAS uv[3];

			for( int j=0; j<3; ++j )
			{
				uv[j] = reinterpret_cast<Vector2 * RESTRICT_ALIAS>(
							dstData + uvStride + j * bytesPerVertex );
			}

			//Copy UVs, mirroring V.
			uv[0]->x = faceUv[i].uv[0].x;
			uv[0]->y = 1.0f - faceUv[i].uv[0].y;
			uv[1]->x = faceUv[i].uv[1].x;
			uv[1]->y = 1.0f - faceUv[i].uv[1].y;
			uv[2]->x = faceUv[i].uv[2].x;
			uv[2]->y = 1.0f - faceUv[i].uv[2].y;

			dstData += bytesPerVertex * 3u;

			if( faces[i].numIndicesInFace == 4 )
			{
				for( int j=0; j<3; ++j )
				{
					uv[j] = reinterpret_cast<Vector2 * RESTRICT_ALIAS>(
								dstData + uvStride + j * bytesPerVertex );
				}

				uv[0]->x = faceUv[i].uv[0].x;
				uv[0]->y = 1.0f - faceUv[i].uv[0].y;
				uv[1]->x = faceUv[i].uv[2].x;
				uv[1]->y = 1.0f - faceUv[i].uv[2].y;
				uv[2]->x = faceUv[i].uv[3].x;
				uv[2]->y = 1.0f - faceUv[i].uv[3].y;

				dstData += bytesPerVertex * 3u;
			}
		}
	}
	//-------------------------------------------------------------------------
	uint32_t VertexUtils::shrinkVertexBuffer( uint8_t *vertexData,
											  Ogre::FastArray<uint32_t> &vertexConversionLutArg,
											  uint32_t bytesPerVertex,
											  uint32_t numVertices )
	{
		//Mark duplicated vertices as such.
		//Swap the internal pointer to a local version of the
		//array to allow compiler optimizations (otherwise the
		//compiler can't know if vertexConversionLutArg.mSize
		//and co. may change after every non-trivial call)
		Ogre::FastArray<uint32_t> vertexConversionLut;
		vertexConversionLut.swap( vertexConversionLutArg );

		vertexConversionLut.resize( numVertices );
		for( uint32_t i=0; i<numVertices; ++i )
			vertexConversionLut[i] = i;

		for( uint32_t i=0; i<numVertices; ++i )
		{
			for( uint32_t j=i+1; j<numVertices; ++j )
			{
				if( vertexConversionLut[j] == j )
				{
					if( memcmp( vertexData + i * bytesPerVertex,
								vertexData + j * bytesPerVertex,
								bytesPerVertex ) == 0 )
					{
						vertexConversionLut[j] = i;
					}
				}
			}
		}

		uint32_t newNumVertices = numVertices;

		//Remove the duplicated vertices, iterating in reverse order for lower algorithmic complexity.
		for( uint32_t i=numVertices; --i; )
		{
			if( vertexConversionLut[i] != i )
			{
				--newNumVertices;
				memmove( vertexData + i * bytesPerVertex,
						 vertexData + (i+1) * bytesPerVertex,
						 (newNumVertices - i) * bytesPerVertex );
			}
		}

		//vertexConversionLut.resize( numVertices );
		//The table is outdated because some vertices have shifted. Example:
		//Before:
		//Vertices 0 & 5 were unique, 8 vertices:
		//  0000 5555
		//But now vertex 5 has become vertex 1. We need the table to say:
		//  0000 1111
		uint32_t numUniqueVerts = 0;
		for( uint32_t i=0; i<numVertices; ++i )
		{
			if( vertexConversionLut[i] == i )
				vertexConversionLut[i] = numUniqueVerts++;
			else
				vertexConversionLut[i] = vertexConversionLut[vertexConversionLut[i]];
		}

		vertexConversionLut.swap( vertexConversionLutArg );

		assert( newNumVertices == numUniqueVerts );

		return newNumVertices;
	}
	//-----------------------------------------------------------------------------------
	void VertexUtils::generateTangents( uint8_t *vertexData, uint32_t bytesPerVertex,
										uint32_t numVertices, uint32_t posStride, uint32_t normalStride,
										uint32_t tangentStride, uint32_t uvStride )
	{
		using namespace Ogre;
		for( ::uint32_t i=0; i<numVertices; i += 3 )
		{
			//Lengyel's Method, it is fast and simple. Also we don't need to care about duplicating
			//vertices due to parity discontinuities or averaging tangents that are too different
			//since the input assumes 3 vertices per triangle.
			Vector3 const * RESTRICT_ALIAS vPos[3];
			Vector3 const * RESTRICT_ALIAS vNormal[3];
			Vector2 const * RESTRICT_ALIAS uv[3];
			Vector3 * RESTRICT_ALIAS vTangent[3];
			float * RESTRICT_ALIAS fParity[3];

			for( int j=0; j<3; ++j )
			{
				vPos[j] = reinterpret_cast<Vector3 const * RESTRICT_ALIAS>(
							vertexData + posStride + j * bytesPerVertex );
				vNormal[j] = reinterpret_cast<Vector3 const * RESTRICT_ALIAS>(
							vertexData + normalStride + j * bytesPerVertex );
				uv[j] = reinterpret_cast<Vector2 const * RESTRICT_ALIAS>(
							vertexData + uvStride + j * bytesPerVertex );
				vTangent[j] = reinterpret_cast<Vector3 * RESTRICT_ALIAS>(
							vertexData + tangentStride + j * bytesPerVertex );
				fParity[j] = reinterpret_cast<float * RESTRICT_ALIAS>(
							vertexData + tangentStride + sizeof(Vector3) + j * bytesPerVertex );
			}

			const Vector2 deltaUV1 = *uv[1] - *uv[0];
			const Vector2 deltaUV2 = *uv[2] - *uv[0];
			const Vector3 deltaPos1 = *vPos[1] - *vPos[0];
			const Vector3 deltaPos2 = *vPos[2] - *vPos[0];

			// face normal
			Vector3 tsN = deltaPos1.crossProduct(deltaPos2);
			tsN.normalise();

			const Real uvarea = deltaUV1.crossProduct(deltaUV2);
			if( Math::RealEqual(uvarea, 0.0f) )
			{
				// no tangent, null uv area
				*vTangent[0] = *vTangent[1] = *vTangent[2] = Vector3::ZERO;
			}
			else
			{
				// Normalise by uvarea
				const Real a =  deltaUV2.y / uvarea;
				const Real b = -deltaUV1.y / uvarea;
				const Real c = -deltaUV2.x / uvarea;
				const Real d =  deltaUV1.x / uvarea;

				const Vector3 tsU = (deltaPos1 * a) + (deltaPos2 * b);
				const Vector3 tsV = (deltaPos1 * c) + (deltaPos2 * d);

				// Gram-Schmidt orthogonalize
				*vTangent[0] = (tsU - (*vNormal[0]) * vNormal[0]->dotProduct( tsU )).normalisedCopy();
				*vTangent[1] = (tsU - (*vNormal[1]) * vNormal[1]->dotProduct( tsU )).normalisedCopy();
				*vTangent[2] = (tsU - (*vNormal[2]) * vNormal[2]->dotProduct( tsU )).normalisedCopy();

				//Calculate handedness
				const float parity = tsN.crossProduct(tsU).dotProduct(tsV) < 0.0f ? -1.0f : 1.0f;
				*fParity[0] = parity;
				*fParity[1] = parity;
				*fParity[2] = parity;
			}

			vertexData += bytesPerVertex * 3;
		}
	}
	//-----------------------------------------------------------------------------------
	void VertexUtils::generateTanUV( const uint8_t *vertexData, const uint32_t *indexData,
									 uint32_t bytesPerVertex, uint32_t numVertices, uint32_t numIndices,
									 uint32_t posStride, uint32_t normalStride, uint32_t tangentStride,
									 uint32_t uvStride, Ogre::Vector3 * RESTRICT_ALIAS outData )
	{
		using namespace Ogre;

		Vector3 * RESTRICT_ALIAS tsUs = outData;
		Vector3 * RESTRICT_ALIAS tsVs = outData + numVertices;

		for( ::uint32_t i=0; i<numIndices; i += 3 )
		{
			//Lengyel's Method, it is fast and simple. Also we don't need to care about duplicating
			//vertices due to parity discontinuities or averaging tangents that are too different
			//since the input assumes 3 vertices per triangle.
			Vector3 const * RESTRICT_ALIAS vPos[3];
			Vector3 const * RESTRICT_ALIAS vNormal[3];
			Vector2 const * RESTRICT_ALIAS uv[3];

			for( int j=0; j<3; ++j )
			{
				vPos[j] = reinterpret_cast<Vector3 const * RESTRICT_ALIAS>(
							vertexData + posStride + indexData[i+j] * bytesPerVertex );
				vNormal[j] = reinterpret_cast<Vector3 const * RESTRICT_ALIAS>(
							vertexData + normalStride + indexData[i+j] * bytesPerVertex );
				uv[j] = reinterpret_cast<Vector2 const * RESTRICT_ALIAS>(
							vertexData + uvStride + indexData[i+j] * bytesPerVertex );
			}

			const Vector2 deltaUV1 = *uv[1] - *uv[0];
			const Vector2 deltaUV2 = *uv[2] - *uv[0];
			const Vector3 deltaPos1 = *vPos[1] - *vPos[0];
			const Vector3 deltaPos2 = *vPos[2] - *vPos[0];

			// face normal
			Vector3 tsN = deltaPos1.crossProduct(deltaPos2);
			tsN.normalise();

			const Real uvarea = deltaUV1.crossProduct(deltaUV2);
			if( !Math::RealEqual(uvarea, 0.0f) )
			{
				// Normalise by uvarea
				const Real a =  deltaUV2.y / uvarea;
				const Real b = -deltaUV1.y / uvarea;
				const Real c = -deltaUV2.x / uvarea;
				const Real d =  deltaUV1.x / uvarea;

				const Vector3 tsU = (deltaPos1 * a) + (deltaPos2 * b);
				const Vector3 tsV = (deltaPos1 * c) + (deltaPos2 * d);

				uint32_t idx = indexData[i+0];
				tsUs[idx] += tsU;
				tsVs[idx] += tsV;

				idx = indexData[i+1];
				tsUs[idx] += tsU;
				tsVs[idx] += tsV;

				idx = indexData[i+2];
				tsUs[idx] += tsU;
				tsVs[idx] += tsV;
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void VertexUtils::generateTangetsMergeTUV( uint8_t *vertexData, uint32_t bytesPerVertex,
											   uint32_t numVertices, uint32_t normalStride,
											   uint32_t tangentStride,
											   Ogre::Vector3 * RESTRICT_ALIAS inOutUvBuffer,
											   size_t numThreads )
	{
		using namespace Ogre;

		Vector3 * RESTRICT_ALIAS tsUs = inOutUvBuffer;
		Vector3 * RESTRICT_ALIAS tsVs = inOutUvBuffer + numVertices;

		for( ::uint32_t i=0; i<numVertices; ++i )
		{
			for( size_t j=1; j<numThreads; ++j )
			{
				//Merge the tsU & tsV vectors calculated by the other threads for these same vertices
				tsUs[i] += inOutUvBuffer[i + numVertices * 2u * j];
				tsVs[i] += inOutUvBuffer[i + numVertices * 2u * j + numVertices];
			}

			Vector3 const * RESTRICT_ALIAS vNormal;
			Vector3 * RESTRICT_ALIAS vTangent;
			float * RESTRICT_ALIAS fParity;

			vNormal		= reinterpret_cast<Vector3 const * RESTRICT_ALIAS>( vertexData + normalStride );
			vTangent	= reinterpret_cast<Vector3 * RESTRICT_ALIAS>( vertexData + tangentStride );
			fParity		= reinterpret_cast<float * RESTRICT_ALIAS>( vertexData +
																	tangentStride + sizeof(Vector3) );

			// Gram-Schmidt orthogonalize
			*vTangent = (tsUs[i] - (*vNormal) * vNormal->dotProduct( tsUs[i] )).normalisedCopy();

			//Calculate handedness
			*fParity = vNormal->crossProduct(tsUs[i]).dotProduct(tsVs[i]) < 0.0f ? -1.0f : 1.0f;

			vertexData += bytesPerVertex;
		}
	}
	//-----------------------------------------------------------------------------------
	void GenerateTangentsTask::execute( size_t threadId, size_t numThreads )
	{
		if( !indexData )
		{
			//Make sure numVertices is always multiple of 3 when assigned to each thread
			const uint32_t totalTris = numVertices / 3u;
			const uint32_t numTrisPerThread = Ogre::alignToNextMultiple( totalTris,
																		 numThreads ) / numThreads;

			//If we've got 4 threads and 2 tris, threads 2 & 3 need
			//to process 0 triangles: Make sure we don't overflow.
			uint32_t trisToProcess = totalTris - std::min( totalTris, threadId * numTrisPerThread );
			trisToProcess = std::min( numTrisPerThread, trisToProcess );

			VertexUtils::generateTangents( vertexData + threadId * bytesPerVertex *
														numTrisPerThread * 3u,
										   bytesPerVertex, trisToProcess * 3u,
										   posStride, normalStride,
										   tangentStride, uvStride );
		}
		else
		{
			//Make sure numIndices is always multiple of 3 when assigned to each thread
			const uint32_t totalTris = numIndices / 3u;
			const uint32_t numTrisPerThread = Ogre::alignToNextMultiple( totalTris,
																		 numThreads ) / numThreads;

			//If we've got 4 threads and 2 tris, threads 2 & 3 need
			//to process 0 triangles: Make sure we don't overflow.
			uint32_t trisToProcess = totalTris - std::min( totalTris, threadId * numTrisPerThread );
			trisToProcess = std::min( numTrisPerThread, trisToProcess );

			VertexUtils::generateTanUV( vertexData,
										indexData + threadId * numTrisPerThread * 3u,
										bytesPerVertex, numVertices, trisToProcess * 3u,
										posStride, normalStride,
										tangentStride, uvStride,
										&tuvBuffer[numVertices * 2u * threadId] );

			barrier->sync();

			if( threadId == 0 )
			{
				VertexUtils::generateTangetsMergeTUV( vertexData, bytesPerVertex, numVertices,
													  normalStride, tangentStride, &tuvBuffer[0],
													  numThreads );
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DeindexTask::execute( size_t threadId, size_t numThreads )
	{
		const uint32_t totalFaces = static_cast<uint32_t>( faces->size() );
		const uint32_t numFacesPerThread = Ogre::alignToNextMultiple( totalFaces,
																	  numThreads ) / numThreads;

		//If we've got 4 threads and 2 tris, threads 2 & 3 need
		//to process 0 triangles: Make sure we don't overflow.
		uint32_t numFacesToProcess = totalFaces - std::min( totalFaces, threadId * numFacesPerThread );
		numFacesToProcess = std::min( numFacesPerThread, numFacesToProcess );

		BlenderFace const * ptrFaces = 0;
		BlenderFaceColour const *ptrFacesColour = 0;
		BlenderFaceUv const *ptrFacesUv = 0;
		BlenderRawVertex const *ptrRawVertices = 0;
		uint16_t *ptrMaterialIds = 0;

		if( !faces->empty() )
			ptrFaces = &(*faces)[0];
		if( !facesColour->empty() )
			ptrFacesColour = &(*facesColour)[0];
		if( !faceUv->empty() )
			ptrFacesUv = &(*faceUv)[0];
		if( !blenderRawVertices->empty() )
			ptrRawVertices = &(*blenderRawVertices)[0];
		if( !materialIds->empty() )
			ptrMaterialIds = &(*materialIds)[0];

		VertexUtils::deindex( vertexData + vertexStartThreadIdx[threadId] * bytesPerVertex,
							  bytesPerVertex,
							  ptrFaces + threadId * numFacesPerThread,
							  numFacesToProcess, ptrRawVertices,
							  ptrMaterialIds + vertexStartThreadIdx[threadId] / 3u );

		uint32_t uvStride = sizeof(Ogre::Vector3) * 2u;
		if( !facesColour->empty() )
		{
			VertexUtils::deindex( vertexData + vertexStartThreadIdx[threadId] * bytesPerVertex,
								  bytesPerVertex,
								  ptrFaces + threadId * numFacesPerThread,
								  numFacesToProcess,
								  ptrFacesColour + threadId * numFacesPerThread );

			uvStride += sizeof(uint8_t) * 4u;
		}

		for( uint32_t i=0; i<numUVs; ++i )
		{
			VertexUtils::deindex( vertexData + vertexStartThreadIdx[threadId] * bytesPerVertex,
								  bytesPerVertex,
								  ptrFaces + threadId * numFacesPerThread,
								  numFacesToProcess,
								  ptrFacesUv + totalFaces * i + threadId * numFacesPerThread,
								  uvStride );

			uvStride += sizeof(Ogre::Vector2);
		}
	}
}
