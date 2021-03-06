/*
  This exmple program provides a trivial server program that listens for TCP
  connections on port 9995.  When they arrive, it writes a short message to
  each client connection, and closes each connection once it is flushed.

  Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/

#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

#include "Network/SmartData.h"
#include "Network/NetworkMessage.h"

static const int PORT = 9995;

#include "DergoSystem.h"
#include "Network/NetworkSystem.h"

int main( int argc, char **argv )
{
	DERGO::DergoSystem dergoSystem;
	DERGO::NetworkSystem networkSystem;

	dergoSystem.initialize();

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	networkSystem.addListener( &dergoSystem );

	int retVal = networkSystem.start();

	dergoSystem.deinitialize();

#ifdef _WIN32
	WSACleanup();
#endif

	printf( "Done.\n" );
	return retVal;
}
