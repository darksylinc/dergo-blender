
#pragma once

#include "GraphicsSystem.h"
#include "OgreMesh2.h"
#include "Vao/OgreVertexBufferPacked.h"
#include "OgreIdString.h"

#include "OgreSceneFormatBase.h"
#include "OgreResourceGroupManager.h"

#include "Utils/ShadowsUtils.h"

namespace Ogre
{
	class InstantRadiosity;
	class IrradianceVolume;
	class ParallaxCorrectedCubemap;
	class CubemapProbe;
}

namespace DERGO
{
	class WindowEventListener;

	class DergoSystem : public GraphicsSystem, public NetworkListener,
			public Ogre::DefaultSceneFormatListener, public Ogre::ResourceLoadingListener
	{
	protected:
		struct BlenderItem
		{
			uint32_t	id;
			Ogre::Item *item;

			BlenderItem( uint32_t _id, Ogre::Item *_item ) : id( _id ), item( _item ) {}
		};

		typedef std::vector<BlenderItem> BlenderItemVec;

		struct BlenderMesh
		{
			BlenderItemVec	items;
			Ogre::Mesh		*meshPtr;

			Ogre::String	userFriendlyName;

			BlenderItemVec::iterator findItem( uint32_t itemId );
		};

		struct BlenderLight
		{
			uint32_t	id;
			Ogre::Light *light;

			BlenderLight( uint32_t _id, Ogre::Light *_light ) : id( _id ), light( _light ) {}
		};
		struct BlenderLightCmp
		{
			bool operator () ( const BlenderLight &a, const BlenderLight &b ) const	{ return a.id < b.id; }
			bool operator () ( const BlenderLight &light, uint32_t _id ) const		{ return light.id < _id; }
			bool operator () ( uint32_t _id, const BlenderLight &light ) const		{ return _id < light.id; }
		};

		struct BlenderEmpty
		{
			uint32_t	id;
			Ogre::CubemapProbe *probe;

			bool isAoI;
			bool pccIsStatic;
			Ogre::uint8	pccNumIterations;
			Ogre::Vector3		position;
			Ogre::Quaternion	qRot;
			Ogre::Vector3		halfSize;
			Ogre::Vector3		pccCamPos;
			Ogre::Vector3		pccInnerRegion;
			Ogre::Aabb			linkedArea;

			BlenderEmpty( uint32_t _id ) :
				id( _id ), probe( 0 ), isAoI( false ), pccIsStatic( false ), pccNumIterations( 1u ),
				position( Ogre::Vector3::ZERO ), qRot( Ogre::Quaternion::IDENTITY ),
				halfSize( Ogre::Vector3::ZERO ),
				pccCamPos( Ogre::Vector3::ZERO ), pccInnerRegion( Ogre::Vector3::UNIT_SCALE ),
				linkedArea( Ogre::Aabb::BOX_ZERO ) {}
		};
		struct BlenderEmptyCmp
		{
			bool operator () ( const BlenderEmpty &a, const BlenderEmpty &b ) const	{ return a.id < b.id; }
			bool operator () ( const BlenderEmpty &empty, uint32_t _id ) const		{ return empty.id < _id; }
			bool operator () ( uint32_t _id, const BlenderEmpty &empty ) const		{ return _id < empty.id; }
		};

		struct BlenderMaterial
		{
			uint32_t			id;
			Ogre::HlmsDatablock	*datablock;

			BlenderMaterial( uint32_t _id, Ogre::HlmsDatablock *_db ) : id( _id ), datablock( _db ) {}
		};
		struct BlenderMaterialCmp
		{
			bool operator () ( const BlenderMaterial &a, const BlenderMaterial &b ) const
			{ return a.id < b.id; }
			bool operator () ( const BlenderMaterial &_l, uint32_t _id ) const
			{ return _l.id < _id; }
			bool operator () ( uint32_t _id, const BlenderMaterial &_r ) const
			{ return _id < _r.id; }
		};

		struct ItemData
		{
			uint32_t			id;
			Ogre::String		name;
			Ogre::Vector3		position;
			Ogre::Quaternion	rotation;
			Ogre::Vector3		scale;
		};

		typedef std::vector<BlenderLight> BlenderLightVec;
		typedef std::vector<BlenderEmpty> BlenderEmptyVec;
		typedef std::vector<BlenderMaterial> BlenderMaterialVec;
		typedef std::map<uint32_t, BlenderMesh> BlenderMeshMap;
		typedef std::vector<ItemData> ItemDataVec;
		typedef std::map<Ogre::IdString, Ogre::String> TexAliasToFullPathMap;

		BlenderMeshMap		m_meshes;
		BlenderLightVec		m_lights;
		BlenderEmptyVec		m_empties;
		BlenderMaterialVec	m_materials;
		TexAliasToFullPathMap m_textures;

		bool					m_enableInstantRadiosity;
		Ogre::InstantRadiosity	*m_instantRadiosity;
		Ogre::IrradianceVolume	*m_irradianceVolume;
		Ogre::Vector3			m_irradianceCellSize;
		bool					m_irDirty;

		Ogre::ParallaxCorrectedCubemapAuto	*m_parallaxCorrectedCubemap;

		ShadowsUtils::Settings	m_shadowsSettings;

		struct Window
		{
			Ogre::Window				*renderWindow;
			Ogre::Camera				*camera;
			Ogre::CompositorWorkspace	*workspace;
		};

		typedef std::map<uint64_t, Window> WindowMap;
		WindowMap	m_renderWindows;
		WindowEventListener	*m_windowEventListener;

		virtual void loadResources(void);

		/** Reads the world data, and updates overall scene settings.
		@param smartData
			Network data from client.
		*/
		void syncWorld( Network::SmartData &smartData );

		void rebuildInstantRadiosity();
		void updateIrradianceVolume();

		/** Reads global IR data, and updates overall scene settings.
		@param smartData
			Network data from client.
		*/
		void syncInstantRadiosity( Network::SmartData &smartData );

		/** Reads global PCC data, and updates overall scene settings.
		@param smartData
			Network data from client.
		*/
		void syncParallaxCorrectedCubemaps( Network::SmartData &smartData );

	protected:
		void setShadowsSettings( const ShadowsUtils::Settings &shadowSettings );
	public:
		/**
		@param smartData
			Network data from client.
		*/
		void syncShadowsSettings( Network::SmartData &smartData );

		/// Ogre does not really support sharing the vertex buffer across multiple submeshes,
		/// for simplicity (simpler file format, easier loading, less corner cases, etc).
		/// However in our particular case sharing the vertex buffer is very convenient and
		/// efficient, thus we need to manually destroy the Vaos to prevent Ogre from trying
		/// to delete the same vertex buffer multiple times.
		void destroyMeshVaos( Ogre::Mesh *mesh );

		/** Reads the mesh data, prepares/compacts it, then checks if we need
			to create a new Mesh or update an existing one.
		@param smartData
			Network data from client.
		*/
		void syncMesh( Network::SmartData &smartData );

		/** Creates a mesh.
		@param meshName
			Name of the mesh
		@param optimizedNumVertices
			Number of vertices, already shrunk down.
		@param vertexElements
			Vertex format for the mesh.
		@param vertexDataPtrContainer [in/out]
			Container holding the vertex data.
		@param indices
			Index data, one entry per submesh.
		*/
		void createMesh( uint32_t meshId, const Ogre::String &meshName, uint32_t optimizedNumVertices,
						 const Ogre::VertexElement2VecVec &vertexElements,
						 Ogre::FreeOnDestructor &vertexDataPtrContainer,
						 const std::vector< std::vector<uint32_t> > &indices,
						 const Ogre::Aabb &aabb );

		/** Updates an existing mesh with new content.
			Assumes caller already knows we can do that.
		@param meshEntry
			Existing mesh entry to update.
		*/
		void updateMesh( const BlenderMesh &meshEntry, uint32_t optimizedNumVertices,
						 Ogre::FreeOnDestructor &vertexDataPtrContainer,
						 const std::vector< std::vector<uint32_t> > &indices,
						 const Ogre::Aabb &aabb );

		/** Destroys existing mesh, creates it again, then restores all asociated items.
		@param meshEntry
			Existing mesh entry to update. Must be a hard copy.
		*/
		void recreateMesh( uint32_t meshId, BlenderMesh meshEntry, uint32_t optimizedNumVertices,
						   const Ogre::VertexElement2VecVec &vertexElements,
						   Ogre::FreeOnDestructor &vertexDataPtrContainer,
						   const std::vector< std::vector<uint32_t> > &indices,
						   const Ogre::Aabb &aabb );

		/** Reads item data from network, and updates the existing one.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		@return
			False if failed to sync due to an error. e.g. the mesh ID does not exist.
		*/
		bool syncItem( Network::SmartData &smartData );

		/** Creates an Item at the given parameters
		@param blenderMesh
		@param itemData
		*/
		void createItem( BlenderMesh &blenderMesh, const ItemData &itemData );

		/**
		@param meshId
			ID of the mesh. Must exist.
		@param itemId
			ID of the item. Ignored if does not exist.
		@return
			False if failed to sync due to an error. e.g. the mesh ID does not exist.
		*/
		bool destroyItem( Network::SmartData &smartData );

		/** Reads light data from network, and updates the existing one.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		*/
		void syncLight( Network::SmartData &smartData );

		/**
		@param lightId
			ID of the light. Ignored if does not exist.
		*/
		void destroyLight( Network::SmartData &smartData );

		/** Reads 'empty' data from network, and updates the existing one.
			It contains info such as Instant Radiosity's Area of Interests or PCC probes.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		*/
		void syncEmpty( Network::SmartData &smartData );

		/**
		@param emptyId
			ID of the light. Ignored if does not exist.
		*/
		void destroyEmpty( Network::SmartData &smartData );

		/** Reads material data from network, and updates the existing one.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		*/
		void syncMaterial( Network::SmartData &smartData );

		/** Assigns textures to material slots. Ignores missing textures.
		@param smartData
			Network data from client.
		@return
			False if failed to sync due to an error. e.g. the material ID does not exist.
		*/
		bool syncMaterialTexture( Network::SmartData &smartData );

		virtual Ogre::DataStreamPtr resourceLoading( const Ogre::String &name,
													 const Ogre::String &group,
													 Ogre::Resource *resource );
		virtual void resourceStreamOpened( const Ogre::String &name, const Ogre::String &group,
										   Ogre::Resource *resource, Ogre::DataStreamPtr& dataStream );
		virtual bool resourceCollision( Ogre::Resource *resource,
										Ogre::ResourceManager *resourceManager );

		virtual bool grouplessResourceExists( const Ogre::String &name );
		virtual Ogre::DataStreamPtr grouplessResourceLoading( const Ogre::String &name );
		virtual Ogre::DataStreamPtr grouplessResourceOpened( const Ogre::String &name,
															 Ogre::Archive *archive,
															 Ogre::DataStreamPtr &dataStream );

		//void openImageFromFile( const Ogre::String &filename, Ogre::Image2 &outImage );

		/** Reads texture data from network, and updates the existing one.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		*/
		void syncTexture( Network::SmartData &smartData );

		/// Destroys everything. Useful for resync'ing
		void reset();

		void exportToFile( Network::SmartData &smartData );

	public:
		DergoSystem( Ogre::ColourValue backgroundColour = Ogre::ColourValue( 0.2f, 0.4f, 0.6f ) );
		virtual ~DergoSystem();

		virtual void initialize();
		virtual void deinitialize();

		virtual void chooseSceneManager();
		virtual Ogre::CompositorWorkspace* setupCompositor(void);

		/// @coppydoc NetworkListener::processMessage
		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem );
		/// @coppydoc NetworkListener::allConnectionsTerminated
		virtual void allConnectionsTerminated();

		// HlmsJsonListener overload
		virtual void savingChangeTextureName( Ogre::String &inOutTexName );
		// HlmsTextureExportListener overload
		virtual void savingChangeTextureNameOriginal( const Ogre::String &aliasName,
													  Ogre::String &inOutResourceName,
													  Ogre::String &inOutFilename );
	};
}
