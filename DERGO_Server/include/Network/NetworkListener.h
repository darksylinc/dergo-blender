
#pragma once

#include "Network/NetworkMessage.h"

namespace Network
{
	class SmartData;
}

struct bufferevent;

namespace DERGO
{
	class NetworkSystem;

	class NetworkListener
	{
	public:

		/**
		@param header
			The header of the message
		@param smartData
			It is guaranteed to hold header.sizeBytes data starting from smartData.getCurrentPtr()
		@param bev
		@param networkSystem
		*/
		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem ) = 0;
	};
}
