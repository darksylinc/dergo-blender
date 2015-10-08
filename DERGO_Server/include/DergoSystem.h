
#pragma once

#include "GraphicsSystem.h"

namespace DERGO
{
	class DergoSystem : public GraphicsSystem, public NetworkListener
    {
	protected:
		static uint32_t shrinkVertexBuffer( uint8_t *dstData,
											Ogre::FastArray<uint32_t> &vertexConversionLut,
											uint32_t bytesPerVertex,
											uint32_t numVertices );

		void createMesh( Network::SmartData &smartData );
		void createItem( Network::SmartData &smartData );

    public:
		DergoSystem( Ogre::ColourValue backgroundColour = Ogre::ColourValue( 0.2f, 0.4f, 0.6f ) );
		virtual ~DergoSystem();

		/// @coppydoc NetworkListener::processMessage
		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem );
    };
}
