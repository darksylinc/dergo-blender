#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

#include "Network/NetworkSystem.h"
#include "Network/NetworkMessage.h"
#include "Network/SmartData.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

static const int PORT = 9995;

namespace DERGO
{
	static void buffered_on_read( bufferevent *bev, void *_userData )
	{
		NetworkSystem *networkSystem = reinterpret_cast<NetworkSystem*>( _userData );
		networkSystem->_buffered_on_read( bev );
	}

	static void listener_cb( evconnlistener *listener, evutil_socket_t fd,
							 sockaddr *sa, int socklen, void *_userData )
	{
		NetworkSystem *networkSystem = reinterpret_cast<NetworkSystem*>( _userData );
		networkSystem->_listener_cb( listener, fd, sa, socklen );
	}

	static void conn_eventcb( bufferevent *bev, short events, void *_userData )
	{
		NetworkSystem *networkSystem = reinterpret_cast<NetworkSystem*>( _userData );
		networkSystem->_conn_eventcb( bev, events );
	}

	static void signal_cb( evutil_socket_t sig, short events, void *_userData )
	{
		NetworkSystem *networkSystem = reinterpret_cast<NetworkSystem*>( _userData );
		networkSystem->_signal_cb( sig, events );
	}

	NetworkSystem::NetworkSystem() :
		m_eventBase( 0 ),
		m_numActiveConnections( 0 )
	{
		assert( sizeof(Network::MessageHeader) == HEADER_SIZE );
		m_rcvBuffer.resize( 8 * 1024 * 1024 );
	}
	//-------------------------------------------------------------------------
	NetworkSystem::~NetworkSystem()
	{
		if( m_eventBase )
			event_base_free( m_eventBase );
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::addListener( NetworkListener *listener )
	{
		m_listeners.push_back( listener );
	}
	//-------------------------------------------------------------------------
	int NetworkSystem::start()
	{
		m_currentStream.clear();
		m_eventBase = event_base_new();
		if( !m_eventBase )
		{
			fprintf( stderr, "Could not initialize libevent!\n" );
			return 1;
		}

		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(PORT);

		evconnlistener *listener = evconnlistener_new_bind( m_eventBase, listener_cb,
															reinterpret_cast<void*>(this),
															LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
															(struct sockaddr*)&sin, sizeof(sin) );

		if( !listener )
		{
			fprintf(stderr, "Could not create a listener!\n");
			return 1;
		}

		event *signal_event = evsignal_new( m_eventBase, SIGINT, signal_cb,
											reinterpret_cast<void*>(this) );

		if( !signal_event || event_add(signal_event, NULL)<0 )
		{
			fprintf(stderr, "Could not create/add a signal event!\n");
			return 1;
		}

		event_base_dispatch( m_eventBase );

		evconnlistener_free( listener );
		event_free( signal_event );
		event_base_free( m_eventBase );
		m_eventBase = 0;

		return 0;
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::send( bufferevent *bev, Network::FromServer::FromServer msg,
							  const void *data, Ogre::uint32 sizeBytes )
	{
		m_stashData.resize( sizeof(Network::MessageHeader) + sizeBytes );

		Network::SmartData smartData( &m_stashData[0], m_stashData.size(), false );

		smartData.write<Ogre::uint32>( sizeBytes );
		smartData.write<Ogre::uint8>( msg );
		smartData.write( reinterpret_cast<const unsigned char*>(data), sizeBytes );

		bufferevent_write( bev, smartData.getBasePtr(), smartData.getOffset() );
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::abortConnection( const char *msg, bufferevent *bev )
	{
		printf( "%s", msg );

		assert( false );

		bufferevent_free( bev );
		m_currentStream.clear();
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::_buffered_on_read( bufferevent *bev )
	{
		//Ogre::uint8 data[8192];

		/* Read 8k at a time and send it to all connected clients. */
		while( true )
		{
//			const size_t bytesRead = bufferevent_read(bev, data, sizeof(data));
//			if( bytesRead == 0 || bytesRead > sizeof(data) )
//			{
//				// Done.
//				break;
//			}

//			const size_t oldSize = m_currentStream.size();
//			m_currentStream.resize( oldSize + bytesRead );
//			memcpy( &m_currentStream[oldSize], data, bytesRead );
			const size_t bytesRead = bufferevent_read(bev, &m_rcvBuffer[0], m_rcvBuffer.size());
			if( bytesRead == 0 || bytesRead > m_rcvBuffer.size() )
			{
				// Done.
				break;
			}

			const size_t oldSize = m_currentStream.size();
			m_currentStream.resize( oldSize + bytesRead );
			memcpy( &m_currentStream[oldSize], &m_rcvBuffer[0], bytesRead );
		}

		if( m_currentStream.empty() )
			return;

		Network::SmartData smartData( &m_currentStream[0], m_currentStream.size(), false );

		size_t remainingBytes = smartData.getCapacity() - smartData.getOffset();

		while( remainingBytes >= HEADER_SIZE )
		{
			Network::MessageHeader header = smartData.read<Network::MessageHeader>();
			if( header.sizeBytes > smartData.getCapacity() - smartData.getOffset() )
			{
				//Packet is incomplete. Process it the next time.
				smartData.seekCur( -HEADER_SIZE );
				break;
			}

			if( header.messageType >= Network::FromClient::NumClientMessages )
			{
				abortConnection( "Message type is higher than NumClientMessages. "
								 "Message is corrupt!!!\n", bev );
				return;
			}

			const size_t nextPos = smartData.getOffset() + header.sizeBytes;

			std::vector<NetworkListener*>::const_iterator itor = m_listeners.begin();
			std::vector<NetworkListener*>::const_iterator end  = m_listeners.end();

			while( itor != end )
			{
				(*itor)->processMessage( header, smartData, bev, *this );
				++itor;
			}

			assert( smartData.getOffset() <= nextPos &&
					"processMessage read beyond of what it was allowed" );

			smartData.seekSet( nextPos );
			remainingBytes = smartData.getCapacity() - smartData.getOffset();
		}

		const size_t bytesLeftUnread = smartData.getCapacity() - smartData.getOffset();
		if( bytesLeftUnread )
		{
			//If there was any leftover, we'll read it the next time.
			memmove( &m_currentStream[0],
					&m_currentStream[smartData.getOffset()],
					bytesLeftUnread );
		}

		m_currentStream.resize( bytesLeftUnread );
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::_listener_cb( evconnlistener *listener, evutil_socket_t fd,
									  sockaddr *sa, int socklen )
	{
		bufferevent *bev = bufferevent_socket_new( m_eventBase, fd, BEV_OPT_CLOSE_ON_FREE );
		if (!bev)
		{
			fprintf(stderr, "Error constructing bufferevent!");
			event_base_loopbreak( m_eventBase );
			return;
		}
		bufferevent_setcb( bev, buffered_on_read, /*conn_writecb*/NULL, conn_eventcb, this );
		bufferevent_enable( bev, EV_WRITE );
		bufferevent_enable( bev, EV_READ );

		char ipAddress[INET6_ADDRSTRLEN];
		switch( sa->sa_family )
		{
		case AF_INET:
		{
			sockaddr_in *addr_in = reinterpret_cast<sockaddr_in*>(sa);
			inet_ntop( AF_INET, &addr_in->sin_addr, ipAddress, INET_ADDRSTRLEN );
			break;
		}
		case AF_INET6:
		{
			sockaddr_in6 *addr_in6 = reinterpret_cast<sockaddr_in6*>(sa);
			inet_ntop( AF_INET6, &addr_in6->sin6_addr, ipAddress, INET6_ADDRSTRLEN );
			break;
		}
		default:
			break;
		}

		++m_numActiveConnections;

		printf( "Incoming new client connection from %s\n", ipAddress );
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::_conn_eventcb( bufferevent *bev, short events )
	{
		if (events & BEV_EVENT_EOF) {
			printf("Connection closed.\n");
		} else if (events & BEV_EVENT_ERROR) {
			printf("Got an error on the connection: %s\n",
				strerror(errno));/*XXX win32*/
		}

		--m_numActiveConnections;

		if( m_numActiveConnections == 0 )
		{
			std::vector<NetworkListener*>::const_iterator itor = m_listeners.begin();
			std::vector<NetworkListener*>::const_iterator end  = m_listeners.end();

			while( itor != end )
			{
				(*itor)->allConnectionsTerminated();
				++itor;
			}
		}

		/* None of the other events can happen here, since we haven't enabled
		 * timeouts */
		bufferevent_free( bev );
	}
	//-------------------------------------------------------------------------
	void NetworkSystem::_signal_cb( evutil_socket_t sig, short events )
	{
		timeval delay = { 1, 0 };

		printf( "Caught an interrupt signal; exiting cleanly in one second.\n" );

		event_base_loopexit( m_eventBase, &delay );
	}
}
