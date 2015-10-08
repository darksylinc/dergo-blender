#!/usr/bin/python
# Code based on Eric Langyel's OpenGEX exporter. All credits to him. His source code was released under public domain

import struct

class ExportVertex:
	__slots__ = ("hash", "vertexIndex", "faceIndex", "position", "normal", "color", "texcoord")

	def __init__(self):
		self.color = []
		self.texcoord = []

	def __eq__(self, v):
		if (self.hash != v.hash):
			return (False)
		if (self.position != v.position):
			return (False)
		if (self.normal != v.normal):
			return (False)
		if (self.color != v.color):
			return (False)
		for i in range( len( self.texcoord ) ):
			if (self.texcoord[i] != v.texcoord[i]):
				return (False)
		return (True)

	def Hash(self):
		h = hash(self.position[0])
		h = h * 21737 + hash(self.position[1])
		h = h * 21737 + hash(self.position[2])
		h = h * 21737 + hash(self.normal[0])
		h = h * 21737 + hash(self.normal[1])
		h = h * 21737 + hash(self.normal[2])
		if self.color != []:
			h = h * 21737 + hash(self.color[0])
			h = h * 21737 + hash(self.color[1])
			h = h * 21737 + hash(self.color[2])
		for texCoord in self.texcoord:
			h = h * 21737 + hash(texCoord[0])
			h = h * 21737 + hash(texCoord[1])
		self.hash = h


class MeshExport:
	@staticmethod
	def vertexArrayToBytes( exportVertexArray ):
		bytesPerVertex = 3 + 3
		if len(exportVertexArray) > 0:
			vertex = exportVertexArray[0]
			if vertex.color != []:
				bytesPerVertex += 1
			bytesPerVertex += len( vertex.texcoord )
		bytesPerVertex *= 4
		bytesObj = bytearray( len(exportVertexArray) * bytesPerVertex )
		i = 0
		
		vector3Struct = struct.Struct( "=3f" )
		vector2Struct = struct.Struct( "=2f" )
		vectorUchar4Struct = struct.Struct( "=4B" )
		
		for vertex in exportVertexArray:
			vector3Struct.pack_into( bytesObj, i, *vertex.position );	i += 3 * 4
			vector3Struct.pack_into( bytesObj, i, *vertex.normal );		i += 3 * 4
			
			if vertex.color != []:
				vectorUchar4Struct.pack_into( bytesObj, i, \
												int(vertex.color[0] * 255.0 + 0.5),\
												int(vertex.color[1] * 255.0 + 0.5),\
												int(vertex.color[2] * 255.0 + 0.5),\
												255	);					i += 1 * 4
												
			for texCoord in vertex.texcoord:
				vector2Struct.pack_into( bytesObj, i, *texCoord );		i += 2 * 4
		return bytesObj
		
	@staticmethod
	def DeindexMesh(mesh, materialTable):

		# This function deindexes all vertex positions, colors, and texcoords.
		# Three separate ExportVertex structures are created for each triangle.

		vertexArray = mesh.vertices
		exportVertexArray = []
		faceIndex = 0

		for face in mesh.tessfaces:
			k1 = face.vertices[0]
			k2 = face.vertices[1]
			k3 = face.vertices[2]

			v1 = vertexArray[k1]
			v2 = vertexArray[k2]
			v3 = vertexArray[k3]

			exportVertex = ExportVertex()
			exportVertex.vertexIndex = k1
			exportVertex.faceIndex = faceIndex
			exportVertex.position = v1.co
			exportVertex.normal = v1.normal if (face.use_smooth) else face.normal
			exportVertexArray.append(exportVertex)

			exportVertex = ExportVertex()
			exportVertex.vertexIndex = k2
			exportVertex.faceIndex = faceIndex
			exportVertex.position = v2.co
			exportVertex.normal = v2.normal if (face.use_smooth) else face.normal
			exportVertexArray.append(exportVertex)

			exportVertex = ExportVertex()
			exportVertex.vertexIndex = k3
			exportVertex.faceIndex = faceIndex
			exportVertex.position = v3.co
			exportVertex.normal = v3.normal if (face.use_smooth) else face.normal
			exportVertexArray.append(exportVertex)

			materialTable.append(face.material_index)

			if (len(face.vertices) == 4):
				k1 = face.vertices[0]
				k2 = face.vertices[2]
				k3 = face.vertices[3]

				v1 = vertexArray[k1]
				v2 = vertexArray[k2]
				v3 = vertexArray[k3]

				exportVertex = ExportVertex()
				exportVertex.vertexIndex = k1
				exportVertex.faceIndex = faceIndex
				exportVertex.position = v1.co
				exportVertex.normal = v1.normal if (face.use_smooth) else face.normal
				exportVertexArray.append(exportVertex)

				exportVertex = ExportVertex()
				exportVertex.vertexIndex = k2
				exportVertex.faceIndex = faceIndex
				exportVertex.position = v2.co
				exportVertex.normal = v2.normal if (face.use_smooth) else face.normal
				exportVertexArray.append(exportVertex)

				exportVertex = ExportVertex()
				exportVertex.vertexIndex = k3
				exportVertex.faceIndex = faceIndex
				exportVertex.position = v3.co
				exportVertex.normal = v3.normal if (face.use_smooth) else face.normal
				exportVertexArray.append(exportVertex)

				materialTable.append(face.material_index)

			faceIndex += 1

		colorCount = len(mesh.tessface_vertex_colors)
		if (colorCount > 0):
			colorFace = mesh.tessface_vertex_colors[0].data
			vertexIndex = 0
			faceIndex = 0

			for face in mesh.tessfaces:
				cf = colorFace[faceIndex]
				exportVertexArray[vertexIndex].color = cf.color1
				vertexIndex += 1
				exportVertexArray[vertexIndex].color = cf.color2
				vertexIndex += 1
				exportVertexArray[vertexIndex].color = cf.color3
				vertexIndex += 1

				if (len(face.vertices) == 4):
					exportVertexArray[vertexIndex].color = cf.color1
					vertexIndex += 1
					exportVertexArray[vertexIndex].color = cf.color3
					vertexIndex += 1
					exportVertexArray[vertexIndex].color = cf.color4
					vertexIndex += 1

				faceIndex += 1

		for tessface_uv_texture in mesh.tessface_uv_textures:
			texcoordFace = tessface_uv_texture.data
			vertexIndex = 0
			faceIndex = 0

			for face in mesh.tessfaces:
				tf = texcoordFace[faceIndex]
				exportVertexArray[vertexIndex].texcoord.append( tf.uv1 )
				vertexIndex += 1
				exportVertexArray[vertexIndex].texcoord.append( tf.uv2 )
				vertexIndex += 1
				exportVertexArray[vertexIndex].texcoord.append( tf.uv3 )
				vertexIndex += 1

				if (len(face.vertices) == 4):
					exportVertexArray[vertexIndex].texcoord.append( tf.uv1 )
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord.append( tf.uv3 )
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord.append( tf.uv4 )
					vertexIndex += 1

				faceIndex += 1

		for ev in exportVertexArray:
			ev.Hash()

		return (exportVertexArray)