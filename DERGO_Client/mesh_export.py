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
			bytesPerVertex += len( vertex.texcoord ) * 2
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

		#for ev in exportVertexArray:
		#	ev.Hash()

		return (exportVertexArray)
	
	@staticmethod
	def createSendBuffer(meshId, meshName, mesh, tangentUvSource):
		nameAsUtfBytes = meshName.encode('utf-8')
		hasColour = False
		
		bytesNeeded = 4 + 4 + len( nameAsUtfBytes )
		bytesNeeded += 4 + 4 + 1 + 1 + 1
		bytesNeeded += len( mesh.tessfaces ) * 31 + \
						len( mesh.vertices ) * 24
		if len(mesh.tessface_vertex_colors) > 0:
			bytesNeeded += len(mesh.tessface_vertex_colors[0].data) * 48
			hasColour = True
			
		for tessface_uv_texture in mesh.tessface_uv_textures:
			bytesNeeded += len( tessface_uv_texture.data ) * 32
		
		bytesNeeded += 2 + len( mesh.materials ) * 4
		
		bytesObj = bytearray( bytesNeeded )
		currentOffset = 0
		
		# Mesh ID and Name string
		struct.pack_into( "=lI", bytesObj, currentOffset, meshId, len( nameAsUtfBytes ) )
		currentOffset += 8
		bytesObj[currentOffset:currentOffset+len( nameAsUtfBytes )] = nameAsUtfBytes
		currentOffset += len( nameAsUtfBytes )

		# Most of data's header
		struct.pack_into( "=II3B", bytesObj, currentOffset,
			len( mesh.tessfaces ), len( mesh.vertices ), hasColour,
			len( mesh.tessface_uv_textures ), tangentUvSource )
		currentOffset += 4 + 4 + 3

		faceStruct = struct.Struct( "=4I3fHB" )
		faceColourStruct = struct.Struct( "=12f" )
		faceUvStruct = struct.Struct( "=8f" )
		rawVertexStruct = struct.Struct( "=6f" )

		# Send the faces
		for face in mesh.tessfaces:
			vertsRaw = face.vertices_raw

			faceStruct.pack_into( bytesObj, currentOffset,
					vertsRaw[0], vertsRaw[1], vertsRaw[2], vertsRaw[3],
					face.normal[0], face.normal[1], face.normal[2],
					(face.use_smooth << 15) | face.material_index,
					len(face.vertices) )
			
			currentOffset += 31

		# Send the vertex colour
		colorCount = len(mesh.tessface_vertex_colors)
		if (colorCount > 0):
			colorFace = mesh.tessface_vertex_colors[0].data
			for cf in colorFace:
				faceColourStruct.pack_into( bytesObj, currentOffset,
						cf.color1[0], cf.color1[1], cf.color1[2],
						cf.color2[0], cf.color2[1], cf.color2[2],
						cf.color3[0], cf.color3[1], cf.color3[2],
						cf.color4[0], cf.color4[1], cf.color4[2] )

				currentOffset += 48

		# Send the UVs
		for tessface_uv_texture in mesh.tessface_uv_textures:
			texcoordFace = tessface_uv_texture.data

			for tf in texcoordFace:
				faceUvStruct.pack_into( bytesObj, currentOffset, *tf.uv_raw )
				currentOffset += 32
		
		# Send the Raw Vertices
		vertices = mesh.vertices
		for vertex in vertices:
			#TODO: Should/could we send weights too? (vertex.groups)
			position = vertex.co
			normal = vertex.normal
			rawVertexStruct.pack_into( bytesObj, currentOffset,
				position[0], position[1], position[2],
				normal[0], normal[1], normal[2] )
			currentOffset += 24

		# Send the materials
		materialIdTable = []
		for mat in mesh.materials:
			materialIdTable.append( mat.dergo.id )
			
		struct.pack_into( '=H%sl' % len( materialIdTable ), bytesObj, currentOffset,
							len( materialIdTable ), *materialIdTable )
		currentOffset += 2 + len( materialIdTable )

		return bytesObj
