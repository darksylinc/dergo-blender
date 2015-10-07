#!/usr/bin/python

from network import  *

class CallbackObj:
	def processMessage( self, header_sizeBytes, header_messageType, data ):
		print( data.decode('latin-1') )

n = Network()
n.connect()
n.sendData( FromClient.ConnectionTest, b"Hello" )
n.receiveData( CallbackObj() )
n.disconnect()