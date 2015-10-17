
import bpy
import bgl
import mathutils

from .mesh_export import MeshExport
from .network import  *

class Engine:
	def __init__(self):
		self.objId	= 1
		self.meshId	= 1
		
		self.numActiveRenderEngines = 0
		
		try:
			self.network = Network()
			self.network.connect()
			self.reset()
		except ConnectionError as e:
			print( e )
			pass
		
	def __del__(self):
		return
		
	def reset( self ):
		# Tell server to reset
		self.network.sendData( FromClient.Reset, None )
		# Remove our data
		for object in bpy.data.objects:
			try:
				del object['DERGO']
			except KeyError: pass
		for mesh in bpy.data.meshes:
			try:
				del mesh['DERGO']
			except KeyError: pass

		self.objId	= 1
		self.meshId	= 1
		
	def view_update(self, context):
		scene = context.scene
		
		for object in scene.objects:
			if object.type == 'MESH':
				self.syncItem( object, scene )
		return
		
	# Removes all objects with the same ID as selected (i.e. user duplicated an object
	# and now we're dealing with duplicated IDs). Removes from server and deletes its
	# associated DERGO data. Mesh is not removed from server.
	def removeObjectsWithId( self, id, scene ):
		for object in scene.objects:
			if 'DERGO' in object and object['DERGO']['id'] == id:
				objDergoProps = object['DERGO']
				self.network.sendData( FromClient.ItemRemove, struct.pack( '=QQ', objDergoProps['id_mesh'], objDergoProps['id'] ) )
				try:
					del object.data['DERGO']
				except KeyError: pass
				del object['DERGO']
	
	def syncItem( self, object, scene ):
		#if object.is_visible( scene ):
		if 'DERGO' not in object:
			object['DERGO'] = { 'in_sync' : False, 'id' : self.objId, 'id_mesh' : 0, 'name' : object.name }
			self.objId += 1

		objDergoProps = object['DERGO']
		
		if objDergoProps['name'] != object.name:
			# Either user changed its name, or user hit "Duplicate" on the object; thus getting same ID.
			self.removeObjectsWithId( objDergoProps['id'], scene )
			object['DERGO'] = { 'in_sync' : False, 'id' : self.objId, 'id_mesh' : 0, 'name' : object.name }
			objDergoProps = object['DERGO']
			self.objId += 1

		# Server doesn't have object, or object was moved, or
		# mesh was modified, or modifier requires an update.
		#	print( object.is_updated_data )	# True when skeleton moved
		#	print( object.data.is_updated )	# False when skeleton moved
		if not objDergoProps['in_sync'] or object.is_updated or object.is_updated_data:
			if 'DERGO' not in object.data:
				object.data['DERGO'] = { 'in_sync' : False, 'id' : self.meshId }
				self.meshId += 1
				
			dataDergoProps = object.data['DERGO']
			
			if len( object.modifiers ) > 0:
				meshName = '##internal##_' + object.name
				linkedMeshId = objDergoProps['id'] | 0x8000000000000000
			else:
				meshName = object.data.name
				linkedMeshId = dataDergoProps['id']
			
			# Check if mesh changed, or if our modifiers made an update, and that
			# we haven't already sync'ed this object (only if shared)
			if \
			((not objDergoProps['in_sync'] or object.is_updated_data) and len( object.modifiers ) > 0) or \
			((not dataDergoProps['in_sync'] or object.data.is_updated) and len( object.modifiers ) == 0):
				exportMesh = object.to_mesh( scene, True, "PREVIEW", True, False)
					
				# Triangulate mesh and remap vertices to eliminate duplicates.
				materialTable = []
				exportVertexArray = MeshExport.DeindexMesh(exportMesh, materialTable)
				#triangleCount = len(materialTable)
				
				dataToSend = bytearray( struct.pack( '=Q', linkedMeshId ) )
				nameAsUtfBytes = meshName.encode('utf-8')
				dataToSend.extend( struct.pack( '=I', len( nameAsUtfBytes ) ) )
				dataToSend.extend( nameAsUtfBytes )
				dataToSend.extend( struct.pack( "=IBB", len( exportVertexArray ),
												len(exportMesh.tessface_vertex_colors) > 0,
												len(exportMesh.tessface_uv_textures) ) )
				dataToSend.extend( MeshExport.vertexArrayToBytes( exportVertexArray ) )
				dataToSend.extend( struct.pack( '=%sH' % len( materialTable ), *materialTable ) )
				
				self.network.sendData( FromClient.Mesh, dataToSend )
				bpy.data.meshes.remove( exportMesh )
				if len( object.modifiers ) == 0:
					dataDergoProps['in_sync'] = True
			
			# Item is now linked to a different mesh! Remove ourselves			
			if objDergoProps['id_mesh'] != 0 and objDergoProps['id_mesh'] != linkedMeshId:
				self.network.sendData( FromClient.ItemRemove, struct.pack( '=QQ', objDergoProps['id_mesh'], objDergoProps['id'] ) )
				objDergoProps['in_sync'] = False

			# Keep it up to date.
			objDergoProps['id_mesh'] = linkedMeshId

			# Create or Update Item.
			if not objDergoProps['in_sync'] or object.is_updated:
				# Mesh ID & Item ID
				dataToSend = bytearray( struct.pack( '=QQ', linkedMeshId, objDergoProps['id'] ) )
				
				# Item name
				asUtfBytes = object.data.name.encode('utf-8')
				dataToSend.extend( struct.pack( '=I', len( asUtfBytes ) ) )
				dataToSend.extend( asUtfBytes )
				
				loc, rot, scale = object.matrix_world.decompose()
				dataToSend.extend( struct.pack( '=10f', loc[0], loc[1], loc[2],\
														rot[0], rot[1], rot[2], rot[3],\
														scale[0], scale[1], scale[2] ) )
				
				self.network.sendData( FromClient.Item, dataToSend )

			objDergoProps['in_sync'] = True

	# Requests server to render the current frame.
	# size_x & size_y are ignored if bAskForResult is false
	def sendViewRenderRequest( self, context, area, region_data,\
								bAskForResult, size_x, size_y ):
		invViewProj = region_data.perspective_matrix.inverted()
		camPos = invViewProj * mathutils.Vector( (0, 0, 0, 1 ) )
		camPos /= camPos[3]
		
		camUp = invViewProj * mathutils.Vector( (0, 1, 0, 1 ) )
		camUp /= camUp[3]
		camUp -= camPos
		
		camRight = invViewProj * mathutils.Vector( (1, 0, 0, 1 ) )
		camRight /= camRight[3]
		camRight -= camPos
		
		camForwd = invViewProj * mathutils.Vector( (0, 0, -1, 1 ) )
		camForwd /= camForwd[3]
		camForwd -= camPos
		
		# print( 'Pos ' + str(camPos) )
		# print( 'Up ' + str(camUp) )
		# print( 'Right ' + str(camRight) )
		# print( 'Forwd ' + str(camForwd) )
		# return

		self.network.sendData( FromClient.Render,\
			struct.pack( '=BqHH15fB', bAskForResult, hash(str(area.spaces[0])), \
						size_x, size_y,\
						area.spaces[0].lens,\
						area.spaces[0].clip_start,\
						area.spaces[0].clip_end,\
						camPos[0], camPos[1], camPos[2],\
						camUp[0], camUp[1], camUp[2],\
						camRight[0], camRight[1], camRight[2],\
						camForwd[0], camForwd[1], camForwd[2],\
						region_data.is_perspective ) )
		return

	# Callback to process Network messages from server.
	def processMessage( self, header_sizeBytes, header_messageType, data ):
		return
	
dergo = None

def register():
	#global dergo
	#dergo = Engine()
	return

def unregister():
	global dergo
	dergo = None