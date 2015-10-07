#!/usr/bin/python
# Code based on Eric Langyel's OpenGEX exporter. All credits to him. His source code was released under public domain

import struct

class ExportVertex:
	__slots__ = ("hash", "vertexIndex", "faceIndex", "position", "normal", "color", "texcoord0", "texcoord1")

	def __init__(self):
		self.color = [255, 255, 255, 255]
		self.texcoord0 = [0.0, 0.0]
		self.texcoord1 = [0.0, 0.0]

	def __eq__(self, v):
		if (self.hash != v.hash):
			return (False)
		if (self.position != v.position):
			return (False)
		if (self.normal != v.normal):
			return (False)
		if (self.color != v.color):
			return (False)
		if (self.texcoord0 != v.texcoord0):
			return (False)
		if (self.texcoord1 != v.texcoord1):
			return (False)
		return (True)

	def Hash(self):
		h = hash(self.position[0])
		h = h * 21737 + hash(self.position[1])
		h = h * 21737 + hash(self.position[2])
		h = h * 21737 + hash(self.normal[0])
		h = h * 21737 + hash(self.normal[1])
		h = h * 21737 + hash(self.normal[2])
		h = h * 21737 + hash(self.color[0])
		h = h * 21737 + hash(self.color[1])
		h = h * 21737 + hash(self.color[2])
		h = h * 21737 + hash(self.texcoord0[0])
		h = h * 21737 + hash(self.texcoord0[1])
		h = h * 21737 + hash(self.texcoord1[0])
		h = h * 21737 + hash(self.texcoord1[1])
		self.hash = h


class MeshExport:
	@staticmethod
	def vertexArrayToBytes( exportVertexArray ):
		bytesObj = bytearray( len(exportVertexArray) * (3 + 3 + 1 + 2 + 2) * 4 )
		i = 0
		
		vector3Struct = struct.Struct( "=3f" )
		vector2Struct = struct.Struct( "=2f" )
		vectorUchar4Struct = struct.Struct( "=4B" )
		
		for vertex in exportVertexArray:
			vector3Struct.pack_into( bytesObj, i, *vertex.position );	i += 3 * 4
			vector3Struct.pack_into( bytesObj, i, *vertex.normal );		i += 3 * 4
			
			vectorUchar4Struct.pack_into( bytesObj, i, *vertex.color );	i += 1 * 4
			vector2Struct.pack_into( bytesObj, i, *vertex.texcoord0 );	i += 2 * 4
			vector2Struct.pack_into( bytesObj, i, *vertex.texcoord1 );	i += 2 * 4
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
				exportVertexArray[vertexIndex].color = int( cf.color1 * 255.0 + 0.5 )
				vertexIndex += 1
				exportVertexArray[vertexIndex].color = int( cf.color2 * 255.0 + 0.5 )
				vertexIndex += 1
				exportVertexArray[vertexIndex].color = int( cf.color3 * 255.0 + 0.5 )
				vertexIndex += 1

				if (len(face.vertices) == 4):
					exportVertexArray[vertexIndex].color = int( cf.color1 * 255.0 + 0.5 )
					vertexIndex += 1
					exportVertexArray[vertexIndex].color = int( cf.color3 * 255.0 + 0.5 )
					vertexIndex += 1
					exportVertexArray[vertexIndex].color = int( cf.color4 * 255.0 + 0.5 )
					vertexIndex += 1

				faceIndex += 1

		texcoordCount = len(mesh.tessface_uv_textures)
		if (texcoordCount > 0):
			texcoordFace = mesh.tessface_uv_textures[0].data
			vertexIndex = 0
			faceIndex = 0

			for face in mesh.tessfaces:
				tf = texcoordFace[faceIndex]
				exportVertexArray[vertexIndex].texcoord0 = tf.uv1
				vertexIndex += 1
				exportVertexArray[vertexIndex].texcoord0 = tf.uv2
				vertexIndex += 1
				exportVertexArray[vertexIndex].texcoord0 = tf.uv3
				vertexIndex += 1

				if (len(face.vertices) == 4):
					exportVertexArray[vertexIndex].texcoord0 = tf.uv1
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord0 = tf.uv3
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord0 = tf.uv4
					vertexIndex += 1

				faceIndex += 1

			if (texcoordCount > 1):
				texcoordFace = mesh.tessface_uv_textures[1].data
				vertexIndex = 0
				faceIndex = 0

				for face in mesh.tessfaces:
					tf = texcoordFace[faceIndex]
					exportVertexArray[vertexIndex].texcoord1 = tf.uv1
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord1 = tf.uv2
					vertexIndex += 1
					exportVertexArray[vertexIndex].texcoord1 = tf.uv3
					vertexIndex += 1

					if (len(face.vertices) == 4):
						exportVertexArray[vertexIndex].texcoord1 = tf.uv1
						vertexIndex += 1
						exportVertexArray[vertexIndex].texcoord1 = tf.uv3
						vertexIndex += 1
						exportVertexArray[vertexIndex].texcoord1 = tf.uv4
						vertexIndex += 1

					faceIndex += 1

		for ev in exportVertexArray:
			ev.Hash()

		return (exportVertexArray)