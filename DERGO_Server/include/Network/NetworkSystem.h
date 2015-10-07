
#pragma once

#include "OgrePrerequisites.h"
#include "Network/NetworkListener.h"
#include <event2/util.h>

namespace Network
{
	class SmartData;
}

struct event_base;
struct bufferevent;
struct sockaddr;
struct evconnlistener;

namespace DERGO
{
	/// Single-connection network manager. Process reads, creates packets for sending.
	///	Encapusaltes libevent
	class NetworkSystem
	{
		event_base *m_eventBase;

		std::vector<Ogre::uint8>		m_currentStream;
		std::vector<Ogre::uint8>		m_stashData;
		std::vector<NetworkListener*>	m_listeners;

	public:
		NetworkSystem();
		~NetworkSystem();

		void addListener( NetworkListener *listener );

		int start();

		void send( bufferevent *bev, Network::FromServer::FromServer msg,
				   const void *data, Ogre::uint32 sizeBytes );

		void abortConnection( const char *msg, bufferevent *bev );

		/// Called when a new client connection arrives
		void _listener_cb( evconnlistener *listener, evutil_socket_t fd,
						   sockaddr *sa, int socklen );
		/// Called every time we receive more data
		void _buffered_on_read( bufferevent *bev );
		/// Called when a special event happened (i.e. connection terminated)
		void _conn_eventcb( bufferevent *bev, short events );
		/// Called on interrupts (e.g. Ctrl+C)
		void _signal_cb( evutil_socket_t sig, short events );
	};
}
