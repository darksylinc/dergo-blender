
#pragma once

#include "GraphicsSystem.h"
#include "OgreMesh2.h"
#include "Vao/OgreVertexBufferPacked.h"

namespace DERGO
{
	class DergoSystem : public GraphicsSystem, public NetworkListener
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
			Ogre::MeshPtr	meshPtr;

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
		typedef std::vector<BlenderMaterial> BlenderMaterialVec;
		typedef std::map<uint32_t, BlenderMesh> BlenderMeshMap;
		typedef std::vector<ItemData> ItemDataVec;

		BlenderMeshMap		m_meshes;
		BlenderLightVec		m_lights;
		BlenderMaterialVec	m_materials;

		struct Window
		{
			Ogre::RenderWindow			*renderWindow;
			Ogre::Camera				*camera;
			Ogre::CompositorWorkspace	*workspace;
		};

		typedef std::map<uint64_t, Window> WindowMap;
		WindowMap	m_renderWindows;

		static uint32_t shrinkVertexBuffer( uint8_t *dstData,
											Ogre::FastArray<uint32_t> &vertexConversionLut,
											uint32_t bytesPerVertex,
											uint32_t numVertices );

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

		/** Reads material data from network, and updates the existing one.
			Creates a new one if doesn't exist.
		@param smartData
			Network data from client.
		*/
		void syncMaterial( Network::SmartData &smartData );

		/// Destroys everything. Useful for resync'ing
		void reset();

	public:
		DergoSystem( Ogre::ColourValue backgroundColour = Ogre::ColourValue( 0.2f, 0.4f, 0.6f ) );
		virtual ~DergoSystem();

		virtual void initialize();
		virtual void deinitialize();

		virtual Ogre::CompositorWorkspace* setupCompositor(void);

		/// @coppydoc NetworkListener::processMessage
		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem );
	};
}
