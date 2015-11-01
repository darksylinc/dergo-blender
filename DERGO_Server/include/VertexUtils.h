
#pragma once

#include "DergoCommon.h"
#include "OgreVector2.h"
#include "OgreVector3.h"
#include "Vao/OgreVertexBufferPacked.h"
#include "Threading/OgreBarrier.h"
#include "Threading/OgreUniformScalableTask.h"

namespace DERGO
{
	struct BlenderFace
	{
		uint32_t		vertexIndex[4];
		Ogre::Vector3	faceNormal;
		uint16_t		materialId; // -> Last bit is use_smooth
		uint8_t			numIndicesInFace;
	};
	static const uint32_t c_sizeOfBlenderFace = sizeof(uint32_t) * 4 + sizeof(Ogre::Vector3) +
												sizeof(uint16_t) + sizeof(uint8_t);
	struct BlenderFaceUv
	{
		Ogre::Vector2	uv[4];
	};
	struct BlenderFaceColour
	{
		Ogre::Vector3	colour[4];
	};
	struct BlenderRawVertex
	{
		Ogre::Vector3	vPos;
		Ogre::Vector3	vNormal;
	};

	class VertexUtils
	{
	public:
		/** Deindexes all vertex positions & normals from Blender's representation
			into 3 vertices per triangle.
		@param dstData [out]
			Destination pointer to store the deindexed data.
			Size must be calculated as:

			uint32_t numVertices = 0;
			std::vector<BlenderFace>::const_iterator itor = blenderFaces.begin();
			std::vector<BlenderFace>::const_iterator end  = blenderFaces.end();
			while( itor != end )
			{
				if( itor->numIndicesInFace == 4 )
					numVertices += 6;
				else
					numVertices += 3;
				++itor;
			}

			uint8_t *dstData = new uint8_t[numVertices * bytesPerVertex];
		@param bytesPerVertex
		@param faces
			Blender faces.
		@param numFaces
			Number faces.
		@param blenderRawVertices
			Blender's unique vertices. Each faces[i].vertexIndex[j] must be low enough
			to dereference blenderRawVertices correctly
		@param materialIds [out]
			Pointer to the material ID of each triangle. Must be:
				uint16_t *materialIds = new uint16_t[numVertices / 3u];
			See dstData on how to calculate numVertices.
		*/
		static void deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							 const BlenderFace *faces, uint32_t numFaces,
							 const BlenderRawVertex *blenderRawVertices,
							 uint16_t * RESTRICT_ALIAS materialIds );

		static void deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							 const BlenderFace *faces, uint32_t numFaces,
							 const BlenderFaceColour *facesColour );

		static void deindex( uint8_t * RESTRICT_ALIAS dstData, uint32_t bytesPerVertex,
							 const BlenderFace *faces, uint32_t numFaces,
							 const BlenderFaceUv *faceUv, uint32_t uvStride );

		static uint32_t shrinkVertexBuffer( uint8_t *dstData,
											Ogre::FastArray<uint32_t> &vertexConversionLut,
											uint32_t bytesPerVertex,
											uint32_t numVertices );

		static void mirrorVs( uint8_t *dstData, uint32_t numVertices,
							  const Ogre::VertexElement2Vec &vertexElements );

		/// Non-indexed lists
		static void generateTangents( uint8_t *vertexData, uint32_t bytesPerVertex,
									  uint32_t numVertices, uint32_t posStride, uint32_t normalStride,
									  uint32_t tangentStride, uint32_t uvStride );

		/** Generates tangents for normal mapping for indexed lists version. Step 01.
			The data will be stored into outData.
		@remarks
			Tangent generation for indexed lists is split in two steps to allow some degree
			of threading, since step 01 can be done concurrently, while step 02 cannot.
			To generate tangents on a single threading app, do the following:
				Ogre::Vector3 *tuvData = new Ogre::Vector3[numVertices * 2u]
				generateTanUV( vertexData, indexData, bytesPerVertex,
								numVertices, numIndices, posStride,
								normalStride, tangentStride, uvStride,
								outData );
				generateTangetsMergeTUV( vertexData, bytesPerVertex, numVertices,
										 normalStride, tangentStride, outData, 1 );
				delete [] outData;
		@param vertexData
		@param indexData
		@param bytesPerVertex
		@param numVertices
		@param numIndices
		@param posStride
		@param normalStride
		@param tangentStride
		@param uvStride
		@param outData
			Buffer to store the result of this step. It generates vectors 'tsU' and 'tsV', one
			pair for each vertex. outData must be at least outData = new Vector3[numVertices*2u]
		*/
		static void generateTanUV( const uint8_t *vertexData, const uint32_t *indexData,
								   uint32_t bytesPerVertex,
								   uint32_t numVertices, uint32_t numIndices, uint32_t posStride,
								   uint32_t normalStride, uint32_t tangentStride, uint32_t uvStride,
								   Ogre::Vector3 * RESTRICT_ALIAS outData );

		/** Generates tangents for normal mapping for indexed lists version. Step 02 and final step.
			Takes the data in inOutUvBuffer (which was the output of generateTanTUV) and fills
			the vertexData.
		@param vertexData
		@param bytesPerVertex
		@param numVertices
		@param normalStride
		@param tangentStride
		@param inOutUvBuffer
			The buffer filled with tsU & tsV by generateTanUV. Assumes all threads are consecutive.
			This means the buffer must be at least new Vector3[numVertices*2u*numThreads]
		@param numThreads
			The number of threads that processed generateTanUV.
		*/
		static void generateTangetsMergeTUV( uint8_t *vertexData, uint32_t bytesPerVertex,
											 uint32_t numVertices, uint32_t normalStride,
											 uint32_t tangentStride,
											 Ogre::Vector3 * RESTRICT_ALIAS inOutUvBuffer,
											 size_t numThreads );
	};

	class GenerateTangentsTask : public Ogre::UniformScalableTask
	{
		uint8_t *vertexData;
		uint32_t bytesPerVertex;
		uint32_t numVertices;
		uint32_t posStride;
		uint32_t normalStride;
		uint32_t tangentStride;
		uint32_t uvStride;
		uint32_t *indexData; /// Can be null.
		uint32_t numIndices; /// Can be 0.

		std::vector<Ogre::Vector3>	tuvBuffer;
		Ogre::Barrier				*barrier;

	public:
		GenerateTangentsTask( uint8_t *_vertexData, uint32_t _bytesPerVertex,
							  uint32_t _numVertices, uint32_t _posStride, uint32_t _normalStride,
							  uint32_t _tangentStride, uint32_t _uvStride,
							  uint32_t *_indexData, uint32_t _numIndices,
							  size_t numThreads ) :
			vertexData( _vertexData ), bytesPerVertex( _bytesPerVertex ),
			numVertices( _numVertices ), posStride( _posStride ),
			normalStride( _normalStride ), tangentStride( _tangentStride ),
			uvStride( _uvStride ), indexData( _indexData ), numIndices( _numIndices )
		{
			if( indexData )
			{
				tuvBuffer.resize( numVertices * 2u * numThreads, Ogre::Vector3::ZERO );
				barrier = new Ogre::Barrier( numThreads );
			}
		}

		virtual void execute( size_t threadId, size_t numThreads );
	};

	class DeindexTask : public Ogre::UniformScalableTask
	{
		uint8_t *vertexData;
		uint32_t bytesPerVertex;
		uint32_t numVertices;
		uint8_t numUVs;
		/// Must be size() == numThreads + 1;
		std::vector<uint32_t> vertexStartThreadIdx;

		const std::vector<BlenderFace>			*faces;
		const std::vector<BlenderFaceColour>	*facesColour;
		/// Assertion: faceUv->size() == faces->size() * numUVs;
		const std::vector<BlenderFaceUv>		*faceUv;
		const std::vector<BlenderRawVertex>		*blenderRawVertices;
		Ogre::FastArray<uint16_t>				*materialIds;

	public:
		DeindexTask( uint8_t *_vertexData, uint32_t _bytesPerVertex, uint32_t _numVertices,
					 uint8_t _numUVs, const std::vector<uint32_t> &_vertexStartThreadIdx,
					 const std::vector<BlenderFace>			*_faces,
					 const std::vector<BlenderFaceColour>	*_facesColour,
					 const std::vector<BlenderFaceUv>		*_faceUv,
					 const std::vector<BlenderRawVertex>	*_blenderRawVertices,
					 Ogre::FastArray<uint16_t>				*_materialIds ) :
			vertexData( _vertexData ), bytesPerVertex( _bytesPerVertex ),
			numVertices( _numVertices ), numUVs( _numUVs ),
			vertexStartThreadIdx( _vertexStartThreadIdx ),
			faces( _faces ), facesColour( _facesColour ),
			faceUv( _faceUv ), blenderRawVertices( _blenderRawVertices ),
			materialIds( _materialIds )
		{
			assert( materialIds->size() == numVertices / 3u );
			assert( faceUv->size() == faces->size() * numUVs );
			assert( facesColour->empty() || facesColour->size() == faces->size() );
		}

		virtual void execute( size_t threadId, size_t numThreads );
	};

	class MirrorVsTask : public Ogre::UniformScalableTask
	{
		uint8_t *vertexData;
		uint32_t bytesPerVertex;
		uint32_t numVertices;
		Ogre::VertexElement2Vec vertexElements;

	public:
		MirrorVsTask( uint8_t *_vertexData, uint32_t _bytesPerVertex, uint32_t _numVertices,
					  const Ogre::VertexElement2Vec &_vertexElements ) :
			vertexData( _vertexData ), bytesPerVertex( _bytesPerVertex ),
			numVertices( _numVertices ), vertexElements( _vertexElements )
		{
		}

		virtual void execute( size_t threadId, size_t numThreads );
	};
}
