#!/usr/bin/python

import socket
import struct

class FromClient:
	ConnectionTest, \
	Init, \
	WorldParams, \
	InstantRadiosity, \
	Mesh, \
	Item, \
	ItemRemove, \
	Light, \
	LightRemove, \
	Empties, \
	Material, \
	MaterialTexture, \
	Texture, \
	Reset, \
	Render, \
	InitAsync, \
	FinishAsync, \
	NumClientMessages = range( 18 )
	
class FromServer:
	ConnectionTest, \
	Resync, \
	Result, \
	NumServerMessages = range( 4 )

class Network:
	def __init__( self ):
		self.headerStruct = struct.Struct( "=IB" )
		self.stream = bytearray()
		
		self.HEADER_SIZE = 5

	def connect( self ):
		self.socket = socket.socket()	# Create a socket object
		host = socket.gethostname() 	# Get local machine name
		port = 9995						# Reserve a port for your service.
		self.socket.connect( (host, port) )

	def disconnect( self ):
		self.socket.close()
		
	def sendData( self, messageType, data ):
		assert( messageType < FromClient.NumClientMessages )
		
		if data == None:
			sizeBytes = 0
			data = bytes(0)
		else:
			sizeBytes = len( data )
		
		packet = self.headerStruct.pack( sizeBytes, messageType )
		
		self.socket.send( b''.join( (packet, bytes(data)) ) )
		
	def receiveData( self, callbackObj ):
		chunk = self.socket.recv( 8192 * 1024 )
		if chunk == '':
			raise RuntimeError("socket connection broken")

		self.stream.extend( chunk )
		
		remainingBytes = len( self.stream )
		
		while remainingBytes >= self.HEADER_SIZE:
			header = self.headerStruct.unpack_from( memoryview( self.stream ) )
			header_sizeBytes	= header[0]
			header_messageType	= header[1]
			
			if header_sizeBytes > remainingBytes - self.HEADER_SIZE:
				# Packet is incomplete. Process it the next time.
				break;
			
			if header_messageType >= FromServer.NumServerMessages:
				raise RuntimeError( "Message type is higher than NumServerMessages. Message is corrupt!!!" )
			
			callbackObj.processMessage( header_sizeBytes, header_messageType,
										self.stream[self.HEADER_SIZE:(self.HEADER_SIZE + header_sizeBytes)] )
			
			remainingBytes -= self.HEADER_SIZE
			remainingBytes -= header_sizeBytes
			
			self.stream = self.stream[(self.HEADER_SIZE + header_sizeBytes):]
