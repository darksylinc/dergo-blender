
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
		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem ) = 0;
	};
}
