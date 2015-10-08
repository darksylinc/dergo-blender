
#include "DergoSystem.h"

#include "OgreRoot.h"
#include "OgreException.h"

#include "OgreRenderWindow.h"
#include "OgreCamera.h"
#include "OgreItem.h"

#include "OgreHlmsUnlit.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsManager.h"

#include "Compositor/OgreCompositorManager2.h"


#include "Network/NetworkSystem.h"
#include "Network/NetworkMessage.h"
#include "Network/SmartData.h"
#include "OgreMeshManager2.h"
#include "OgreMesh2.h"
#include "OgreSubMesh2.h"

namespace DERGO
{
	DergoSystem::DergoSystem( Ogre::ColourValue backgroundColour ) :
		GraphicsSystem( backgroundColour )
    {
    }
    //-----------------------------------------------------------------------------------
	DergoSystem::~DergoSystem()
	{
    }
	//-----------------------------------------------------------------------------------
	void DergoSystem::createMesh( Network::SmartData &smartData )
	{
		Ogre::String meshName = smartData.getString();

		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

		const Ogre::uint32 numVertices	= smartData.read<Ogre::uint32>();
		const bool hasColour			= smartData.read<Ogre::uint8>() != 0;
		const Ogre::uint8 numUVs		= smartData.read<Ogre::uint8>();

		Ogre::VertexElement2VecVec vertexElements( 1 );
		vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );
		vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_NORMAL ) );
		if( hasColour )
		{
			vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_UBYTE4_NORM,
															   Ogre::VES_DIFFUSE ) );
		}
		for( Ogre::uint8 i=0; i<numUVs; ++i )
		{
			vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT2,
															   Ogre::VES_TEXTURE_COORDINATES ) );
		}

		//Read vertex data
		Ogre::uint32 bytesPerVertex = Ogre::VaoManager::calculateVertexSize( vertexElements[0] );

		unsigned char *vertexData = reinterpret_cast<unsigned char*>( OGRE_MALLOC_SIMD(
																		  numVertices * bytesPerVertex,
																		  Ogre::MEMCATEGORY_GEOMETRY ) );

		//We need this in case we raise an exception too early.
		Ogre::FreeOnDestructor dataPtrContainer( vertexData );

		smartData.read( vertexData, numVertices * bytesPerVertex );

		//Remove duplicates (Blender plugin sends us 3 vertices per triangle!)
		Ogre::FastArray<uint32_t> vertexConversionLut;
		const size_t optimizedNumVertices = shrinkVertexBuffer( vertexData, vertexConversionLut,
																bytesPerVertex, numVertices );

		//Split into submeshes based on material assignment.
		std::vector<std::vector<uint32_t>> indices;
		{
			std::vector<uint16_t> uniqueMaterials;

			Ogre::FastArray<uint16_t> materialIds;
			materialIds.resize( numVertices / 3 );
			smartData.read( reinterpret_cast<unsigned char*>( &materialIds[0] ),
							sizeof(uint16_t) * materialIds.size() );

			uint32_t currentVertex = 0;
			Ogre::FastArray<uint16_t>::const_iterator itor = materialIds.begin();
			Ogre::FastArray<uint16_t>::const_iterator end  = materialIds.end();

			while( itor != end )
			{
				std::vector<uint16_t>::const_iterator itSubMesh = std::find( uniqueMaterials.begin(),
																			 uniqueMaterials.end(),
																			 *itor );
				if( itSubMesh == uniqueMaterials.end() )
				{
					//New material found. Need to split another submesh.
					uniqueMaterials.push_back( *itor );
					itSubMesh = uniqueMaterials.end() - 1;
					indices.push_back( std::vector<uint32_t>() );
				}

				size_t subMeshIdx = itSubMesh - uniqueMaterials.begin();

				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );
				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );
				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );

				++itor;
			}
		}

		//TODO: If mesh already exists and vertex & index count are <= than current buffers
		//just update them, don't recreate them.
		Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().createManual(
					meshName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );

		//Create actual GPU buffers.
		Ogre::VertexBufferPacked *vertexBuffer = vaoManager->createVertexBuffer(
					vertexElements[0], optimizedNumVertices, Ogre::BT_DEFAULT, vertexData, true );
		dataPtrContainer.ptr = 0;

		Ogre::VertexBufferPackedVec vertexBuffers( 1, vertexBuffer );

		const Ogre::IndexBufferPacked::IndexType indexType = optimizedNumVertices > 0xffff ?
					Ogre::IndexBufferPacked::IT_32BIT : Ogre::IndexBufferPacked::IT_16BIT;

		std::vector<std::vector<uint32_t>>::const_iterator itor = indices.begin();
		std::vector<std::vector<uint32_t>>::const_iterator end  = indices.end();
		while( itor != end )
		{
			const size_t indexBytes =
					itor->size() * (indexType == Ogre::IndexBufferPacked::IT_16BIT ? 2 : 4);
			uint16_t *indexData = reinterpret_cast<uint16_t*>(
						OGRE_MALLOC_SIMD( indexBytes, Ogre::MEMCATEGORY_GEOMETRY ) );
			Ogre::FreeOnDestructor safeIndexPtr( indexData );

			if( indexType == Ogre::IndexBufferPacked::IT_32BIT )
				memcpy( indexData, &(*itor)[0], itor->size() * sizeof(uint32_t) );
			else
			{
				for( size_t i=0; i<itor->size(); ++i )
					indexData[i] = static_cast<uint16_t>( (*itor)[i] );
			}

			Ogre::IndexBufferPacked *indexBuffer = vaoManager->createIndexBuffer( indexType,
																				  itor->size(),
																				  Ogre::BT_DEFAULT,
																				  indexData, true );
			safeIndexPtr.ptr = 0;

			Ogre::VertexArrayObject *vao = vaoManager->createVertexArrayObject( vertexBuffers,
																				indexBuffer,
																				Ogre::OT_TRIANGLE_LIST );
			Ogre::SubMesh *subMesh = meshPtr->createSubMesh();
			subMesh->mVao[0].push_back( vao );
			subMesh->mVao[1].push_back( vao );

			++itor;
		}
	}
	//-----------------------------------------------------------------------------------
	uint32_t DergoSystem::shrinkVertexBuffer( uint8_t *vertexData,
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
	void DergoSystem::createItem( Network::SmartData &smartData )
	{
		Ogre::String itemName = smartData.getString();
		Ogre::String meshName = smartData.getString();

		//TODO: If object already exists and references the same mesh, just update transform.
		Ogre::Item *item = mSceneManager->createItem( meshName );
		item->setName( itemName );

		Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode();

		Ogre::Vector3 vPos, vScale;
		Ogre::Quaternion qRot;
		vPos.x = smartData.read<float>();
		vPos.y = smartData.read<float>();
		vPos.z = smartData.read<float>();
		qRot.w = smartData.read<float>();
		qRot.x = smartData.read<float>();
		qRot.y = smartData.read<float>();
		qRot.z = smartData.read<float>();
		vScale.x = smartData.read<float>();
		vScale.y = smartData.read<float>();
		vScale.z = smartData.read<float>();
		sceneNode->setPosition( vPos );
		sceneNode->setOrientation( qRot );
		sceneNode->setScale( vScale );
		sceneNode->attachObject( item );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::processMessage( const Network::MessageHeader &header,
									  Network::SmartData &smartData,
									  bufferevent *bev, NetworkSystem &networkSystem )
	{
		switch( header.messageType )
		{
		case Network::FromClient::ConnectionTest:
		{
			char str[128];
			size_t maxVal = std::min( 127u, header.sizeBytes );
			memcpy( str, smartData.getCurrentPtr(), maxVal );
		    str[maxVal] = '\0';
			printf( str );
			networkSystem.send( bev, Network::FromServer::ConnectionTest,
								"Hello you too", sizeof("Hello you too") );
		}
			break;
		case Network::FromClient::Mesh:
			createMesh( smartData );
			break;
		case Network::FromClient::Item:
			createItem( smartData );
			break;
		case Network::FromClient::Render:
		{
			Ogre::uint16 width	= smartData.read<Ogre::uint16>();
			Ogre::uint16 height	= smartData.read<Ogre::uint16>();
			break;
		}
		default:
			break;
		}
	}
}
