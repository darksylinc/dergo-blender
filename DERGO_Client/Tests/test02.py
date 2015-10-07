#!/usr/bin/python

# Make imports work in Python IDLE
if __name__ == '__main__' and __package__ is None:
	from os import sys, path
	sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))

from network import  *
from mesh_export import  *

class CallbackObj:
	def processMessage( self, header_sizeBytes, header_messageType, data ):
		print( data.decode('latin-1') )

exportVertexArray = []

exportVertex = ExportVertex()
exportVertex.position = [0, 1, 2]
exportVertex.normal = [0.0, 0.707107, 0.707107]
exportVertexArray.append( exportVertex )
exportVertex = ExportVertex()
exportVertex.position = [4, 5, 6]
exportVertex.normal = [-1.0, 0.0, 0.0]
exportVertexArray.append( exportVertex )

n = Network()
n.connect()
n.sendData( FromClient.Mesh, MeshExport.vertexArrayToBytes(exportVertexArray) )
n.disconnect()