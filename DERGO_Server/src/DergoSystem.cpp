
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
#include "Vao/OgreStagingBuffer.h"

#include "OgreTextureManager.h"
#include "OgreHardwarePixelBuffer.h"
#include "OgreRenderTexture.h"

namespace DERGO
{
	Ogre::String toStr64( uint64_t val )
	{
		Ogre::StringStream stream;
		stream << val;

		return stream.str();
	}

	DergoSystem::BlenderItemVec::iterator DergoSystem::BlenderMesh::findItem( uint64_t itemId )
	{
		BlenderItemVec::iterator itor = items.begin();
		BlenderItemVec::iterator end  = items.end();

		while( itor != end && itor->id != itemId )
			++itor;

		return itor;
	}

	DergoSystem::DergoSystem( Ogre::ColourValue backgroundColour ) :
		GraphicsSystem( backgroundColour )
    {
    }
    //-----------------------------------------------------------------------------------
	DergoSystem::~DergoSystem()
	{
    }
	//-----------------------------------------------------------------------------------
	void DergoSystem::deinitialize()
	{
		reset();
		if( mWorkspace )
		{
			Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
			compositorManager->removeWorkspace( mWorkspace );
			mWorkspace = 0;
		}
		m_rtt.setNull();
		GraphicsSystem::deinitialize();
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncMesh( Network::SmartData &smartData )
	{
		uint64_t meshId = smartData.read<uint64_t>();
		Ogre::String meshName = smartData.getString();

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
		size_t optimizedNumVertices = 0;

		if( numVertices )
		{
			optimizedNumVertices = shrinkVertexBuffer( vertexData, vertexConversionLut,
													   bytesPerVertex, numVertices );
		}

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

		BlenderMeshMap::const_iterator meshEntryIt = m_meshes.find( meshId );
		if( meshEntryIt == m_meshes.end() )
		{
			//We don't have this mesh.
			createMesh( meshId, meshName, optimizedNumVertices, vertexElements, dataPtrContainer, indices );
		}
		else
		{
			//Check if we can update it (i.e. reuse existing buffers).
			Ogre::MeshPtr meshPtr = meshEntryIt->second.meshPtr;
			bool canReuse = true;

			if( indices.size() != meshPtr->getNumSubMeshes() )
				canReuse = false;

			for( uint16_t i=0; i<meshPtr->getNumSubMeshes() && canReuse; ++i )
			{
				Ogre::SubMesh *subMesh = meshPtr->getSubMesh( i );

				const Ogre::VertexBufferPackedVec &vertexBuffers =
						subMesh->mVao[0][0]->getVertexBuffers();

				//Vertex format changed! (e.g. added or removed UVs)
				if( vertexElements[0] != vertexBuffers[0]->getVertexElements() )
					canReuse = false;

				//Current buffer can't hold it, or it's too big
				if( optimizedNumVertices > vertexBuffers[0]->getNumElements() ||
					optimizedNumVertices < (vertexBuffers[0]->getNumElements() >> 2) )
				{
					canReuse = false;
				}

				Ogre::IndexBufferPacked *indexBuffer = subMesh->mVao[0][0]->getIndexBuffer();
				if( indexBuffer->getNumElements() > indices[i].size() ||
					indexBuffer->getNumElements() > (indices[i].size() >> 2) )
				{
					canReuse = false;
				}
			}

			if( canReuse )
			{
				updateMesh( meshEntryIt->second, optimizedNumVertices,
							dataPtrContainer, indices );
			}
			else
			{
				//Warning: meshEntryIt gets invalidated
				recreateMesh( meshEntryIt->first, meshEntryIt->second, optimizedNumVertices,
							  vertexElements, dataPtrContainer, indices );
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::createMesh( uint64_t meshId, const Ogre::String &meshName,
								  uint32_t optimizedNumVertices,
								  const Ogre::VertexElement2VecVec &vertexElements,
								  Ogre::FreeOnDestructor &vertexDataPtrContainer,
								  const std::vector< std::vector<uint32_t> > &indices )
	{
		Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().createManual(
					toStr64(meshId), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );

		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

		//Create actual GPU buffers.
		Ogre::VertexBufferPacked *vertexBuffer = vaoManager->createVertexBuffer(
					vertexElements[0], optimizedNumVertices, Ogre::BT_DEFAULT,
					vertexDataPtrContainer.ptr, true );
		vertexDataPtrContainer.ptr = 0;

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

		BlenderMesh meshEntry;
		meshEntry.meshPtr			= meshPtr;
		meshEntry.userFriendlyName	= meshName;
		m_meshes[meshId] = meshEntry;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::updateMesh( const BlenderMesh &meshEntry, uint32_t optimizedNumVertices,
								  Ogre::FreeOnDestructor &vertexDataPtrContainer,
								  const std::vector< std::vector<uint32_t> > &indices )
	{
		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

		Ogre::MeshPtr meshPtr = meshEntry.meshPtr;

		//Upload vertex data (all submeshes share it)
		Ogre::SubMesh *subMesh = meshPtr->getSubMesh( 0 );
		Ogre::VertexBufferPacked *vertexBuffer = subMesh->mVao[0][0]->getVertexBuffers()[0];
		vertexBuffer->upload( vertexDataPtrContainer.ptr, 0, optimizedNumVertices );

		for( uint16_t i=0; i<meshPtr->getNumSubMeshes(); ++i )
		{
			Ogre::SubMesh *subMesh = meshPtr->getSubMesh( i );

			Ogre::IndexBufferPacked *indexBuffer = subMesh->mVao[0][0]->getIndexBuffer();
			if( indexBuffer->getIndexType() == Ogre::IndexBufferPacked::IT_32BIT )
			{
				indexBuffer->upload( &indices[i][0], 0, indices[i].size() );
			}
			else
			{
				//const_cast so we can update the shadow copy without
				//malloc'ing extra memory and perform the copy in place.
				uint16_t * RESTRICT_ALIAS shadowCopy = reinterpret_cast<uint16_t * RESTRICT_ALIAS>(
													const_cast<void*>( indexBuffer->getShadowCopy() ));
				Ogre::StagingBuffer *stagingBuffer = vaoManager->getStagingBuffer(
							indices[i].size() * sizeof(uint16_t), true );

				uint16_t * RESTRICT_ALIAS index16 = reinterpret_cast<uint16_t * RESTRICT_ALIAS>(
							stagingBuffer->map( indices[i].size() * sizeof(uint16_t) ) );

				for( size_t j=0; j<indices[j].size(); ++j )
				{
					shadowCopy[j]	= static_cast<uint16_t>( indices[i][j] );
					index16[j]		= static_cast<uint16_t>( indices[i][j] );
				}

				stagingBuffer->unmap( Ogre::StagingBuffer::
									  Destination( indexBuffer, 0, 0,
												   indices[i].size() * sizeof(uint16_t) ) );
			}

			subMesh->mVao[0][0]->setPrimitiveRange( 0, indices[i].size() );
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::recreateMesh( uint64_t meshId, BlenderMesh meshEntry, uint32_t optimizedNumVertices,
									const Ogre::VertexElement2VecVec &vertexElements,
									Ogre::FreeOnDestructor &vertexDataPtrContainer,
									const std::vector< std::vector<uint32_t> > &indices )
	{
		//Destroy all items, but first saving their state.
		ItemDataVec itemsData;
		itemsData.reserve( meshEntry.items.size() );
		BlenderItemVec::const_iterator itor = meshEntry.items.begin();
		BlenderItemVec::const_iterator end  = meshEntry.items.end();

		while( itor != end )
		{
			Ogre::SceneNode *sceneNode = itor->item->getParentSceneNode();

			ItemData itemData;
			itemData.id			= itor->id;
			itemData.name		= itor->item->getName();
			itemData.position	= sceneNode->getPosition();
			itemData.rotation	= sceneNode->getOrientation();
			itemData.scale		= sceneNode->getScale();
			itemsData.push_back( itemData );

			sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
			mSceneManager->destroyItem( itor->item );

			++itor;
		}

		meshEntry.items.clear();

		//Destroy mesh
		Ogre::String userFriendlyName = meshEntry.userFriendlyName;

		Ogre::MeshManager::getSingleton().remove( meshEntry.meshPtr->getName() );
		meshEntry.meshPtr.setNull();
		m_meshes.erase( meshId );

		//Create mesh again.
		createMesh( meshId, userFriendlyName, optimizedNumVertices,
					vertexElements, vertexDataPtrContainer, indices );

		//Restore the items.
		BlenderMesh &newBlenderMesh = m_meshes[meshId];

		ItemDataVec::const_iterator itItem = itemsData.begin();
		ItemDataVec::const_iterator enItem = itemsData.end();

		while( itItem != enItem )
		{
			createItem( newBlenderMesh, *itItem );
			++itItem;
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
	bool DergoSystem::syncItem( Network::SmartData &smartData )
	{
		bool retVal = true;

		ItemData itemData;

		uint64_t meshId	= smartData.read<uint64_t>();
		itemData.id		= smartData.read<uint64_t>();

		itemData.name = smartData.getString();

		itemData.position.x = smartData.read<float>();
		itemData.position.y = smartData.read<float>();
		itemData.position.z = smartData.read<float>();
		itemData.rotation.w = smartData.read<float>();
		itemData.rotation.x = smartData.read<float>();
		itemData.rotation.y = smartData.read<float>();
		itemData.rotation.z = smartData.read<float>();
		itemData.scale.x = smartData.read<float>();
		itemData.scale.y = smartData.read<float>();
		itemData.scale.z = smartData.read<float>();

		BlenderMeshMap::iterator itMeshEntry = m_meshes.find( meshId );
		if( itMeshEntry != m_meshes.end() )
		{
			BlenderItemVec::iterator itItem = itMeshEntry->second.findItem( itemData.id );
			if( itItem != itMeshEntry->second.items.end() )
			{
				itItem->item->setName( itemData.name );
				Ogre::Node *node = itItem->item->getParentNode();
				node->setPosition( itemData.position );
				node->setOrientation( itemData.rotation );
				node->setScale( itemData.scale );
			}
			else
			{
				createItem( itMeshEntry->second, itemData );
			}
		}
		else
		{
			//Shouldn't happen! Tell client to resync
			assert( false );
			retVal = false;
		}

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::createItem( BlenderMesh &blenderMesh, const ItemData &itemData )
	{
		Ogre::Item *item = mSceneManager->createItem( blenderMesh.meshPtr );
		item->setName( itemData.name );

		Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode();
		sceneNode->setPosition( itemData.position );
		sceneNode->setOrientation( itemData.rotation );
		sceneNode->setScale( itemData.scale );
		sceneNode->attachObject( item );

		blenderMesh.items.push_back( BlenderItem( itemData.id, item ) );
	}
	//-----------------------------------------------------------------------------------
	bool DergoSystem::destroyItem( Network::SmartData &smartData )
	{
		bool retVal = true;

		uint64_t meshId = smartData.read<uint64_t>();
		uint64_t itemId = smartData.read<uint64_t>();

		BlenderMeshMap::iterator itMeshEntry = m_meshes.find( meshId );
		if( itMeshEntry != m_meshes.end() )
		{
			BlenderItemVec::iterator itemIt = itMeshEntry->second.findItem( itemId );

			Ogre::SceneNode *sceneNode = itemIt->item->getParentSceneNode();
			sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
			mSceneManager->destroyItem( itemIt->item );

			Ogre::efficientVectorRemove( itMeshEntry->second.items, itemIt );
		}
		else
		{
			//Shouldn't happen! Tell client to resync
			assert( false );
			retVal = false;
		}

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::reset()
	{
		BlenderMeshMap::iterator itor = m_meshes.begin();
		BlenderMeshMap::iterator end  = m_meshes.end();

		while( itor != end )
		{
			//Remove all items for this mesh
			BlenderItemVec::const_iterator itItem = itor->second.items.begin();
			BlenderItemVec::const_iterator enItem = itor->second.items.end();

			while( itItem != enItem )
			{
				Ogre::SceneNode *sceneNode = itItem->item->getParentSceneNode();
				sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
				mSceneManager->destroyItem( itItem->item );

				++itItem;
			}

			//Remove the mesh.
            Ogre::MeshManager::getSingleton().remove( itor->second.meshPtr->getName() );
            itor->second.meshPtr.setNull();

			++itor;
		}

		m_meshes.clear();
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::createRtt( Ogre::uint16 width, Ogre::uint16 height )
	{
		if( m_rtt.isNull() || m_rtt->getWidth() != width || m_rtt->getHeight() != height )
		{
			if( mWorkspace )
			{
				Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
				compositorManager->removeWorkspace( mWorkspace );
				mWorkspace = 0;
			}

			if( !m_rtt.isNull() )
			{
				Ogre::TextureManager::getSingleton().remove( m_rtt->getHandle() );
				m_rtt.setNull();
			}

			m_rtt = Ogre::TextureManager::getSingleton().createManual(
						"rtt", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
						Ogre::TEX_TYPE_2D, width, height, 0, Ogre::PF_A8B8G8R8,
						Ogre::TU_RENDERTARGET, 0, true );

			mWorkspace = setupCompositor();
		}
	}
	//-----------------------------------------------------------------------------------
	Ogre::CompositorWorkspace* DergoSystem::setupCompositor(void)
	{
		if( m_rtt.isNull() )
			return 0;

		Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();

		const Ogre::IdString workspaceName( "DERGO Workspace" );
		if( !compositorManager->hasWorkspaceDefinition( workspaceName ) )
		{
			compositorManager->createBasicWorkspaceDef( workspaceName, mBackgroundColour,
														Ogre::IdString() );
		}

		return compositorManager->addWorkspace( mSceneManager, m_rtt->getBuffer()->getRenderTarget(),
												mCamera, workspaceName, true );
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
			syncMesh( smartData );
			break;
		case Network::FromClient::Item:
			if( !syncItem( smartData ) )
				networkSystem.send( bev, Network::FromServer::Resync, 0, 0 );
			break;
		case Network::FromClient::ItemRemove:
			if( !destroyItem( smartData ) )
				networkSystem.send( bev, Network::FromServer::Resync, 0, 0 );
			break;
		case Network::FromClient::Reset:
			reset();
			break;
		case Network::FromClient::Render:
		{
			//Read camera paramters
			float fovDegrees	= smartData.read<float>();
			float nearClip		= smartData.read<float>();
			float farClip		= smartData.read<float>();
			Ogre::Vector3 camPos	= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camUp		= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camRight	= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camForward= smartData.read<Ogre::Vector3>();
			bool isPerspective		= smartData.read<uint8_t>() != 0;

			if( !isPerspective )
			{
				float orthoWidth = camRight.length();
				float orthoHeight = camUp.length();
				mCamera->setOrthoWindow( orthoWidth, orthoHeight );
			}

			mCamera->setProjectionType( isPerspective ? Ogre::PT_PERSPECTIVE : Ogre::PT_ORTHOGRAPHIC );
			mCamera->setFOVy( Ogre::Degree(fovDegrees) );
			mCamera->setNearClipDistance( nearClip );
			mCamera->setFarClipDistance( farClip );

			mCamera->setPosition( camPos );
			camUp.normalise();
			camRight.normalise();
			camForward.normalise();

			Ogre::Quaternion qRot( camRight, camUp, camForward );
			qRot.normalise();
			mCamera->setOrientation( qRot );

			Ogre::uint16 width	= smartData.read<Ogre::uint16>();
			Ogre::uint16 height	= smartData.read<Ogre::uint16>();
			createRtt( width, height );
			update();

			Network::SmartData toClient( 2 * sizeof(Ogre::uint16) +
										 Ogre::PixelUtil::getMemorySize( width, height, 1,
																		 m_rtt->getFormat() ) );
			toClient.write<uint16_t>( width );
			toClient.write<uint16_t>( height );
			Ogre::PixelBox dstData( width, height, 1, m_rtt->getFormat(), toClient.getCurrentPtr() );
			m_rtt->getBuffer()->blitToMemory( Ogre::Box( 0, 0, width, height ), dstData );
			networkSystem.send( bev, Network::FromServer::Result,
								toClient.getBasePtr(), toClient.getCapacity() );
			break;
		}
		default:
			break;
		}
	}
}
