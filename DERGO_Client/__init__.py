
import bpy
import bgl
import time

from .mesh_export import MeshExport
from .network import  *

bl_info = {
	 "name": "DERGO3D",
	 "author": "Matías N. Goldberg",
	 "version": (2, 1),
	 "blender": (2, 72, 0),
	 "category": "Render",
	 "location": "Info header, render engine menu",
	 "warning": "",
	 "wiki_url": "",
	 "tracker_url": "",
	 "description": "OGRE3D integration for Blender"
}

class DergoRenderEngine(bpy.types.RenderEngine):
	# These three members are used by blender to set up the
	# RenderEngine; define its internal name, visible name and capabilities.
	bl_idname = 'DERGO3D'
	bl_label = 'OGRE3D Renderer'
	bl_use_preview = True
	
	network = None
	
	objId	= 1
	meshId	= 1

	# This is the only method called by blender, in this example
	# we use it to detect preview rendering and call the implementation
	# in another method.
	def render(self, scene):
		scale = scene.render.resolution_percentage / 100.0
		self.size_x = int(scene.render.resolution_x * scale)
		self.size_y = int(scene.render.resolution_y * scale)

		print( self.is_preview )

		#if scene.name == 'preview':
		if self.is_preview:
			self.render_preview(scene)
		else:
			self.render_scene(scene)

	# In this example, we fill the preview renders with a flat green color.
	def render_preview(self, scene):
		pixel_count = self.size_x * self.size_y

		# The framebuffer is defined as a list of pixels, each pixel
		# itself being a list of R,G,B,A values
		green_rect = [[0.0, 1.0, 0.0, 1.0]] * pixel_count

		# Here we write the pixel values to the RenderResult
		result = self.begin_result(0, 0, self.size_x, self.size_y)
		layer = result.layers[0]
		layer.rect = green_rect
		self.end_result(result)

	# In this example, we fill the full renders with a flat blue color.
	def render_scene(self, scene):
		pixel_count = self.size_x * self.size_y

		# The framebuffer is defined as a list of pixels, each pixel
		# itself being a list of R,G,B,A values
		blue_rect = [[0.2, 0.4, 6.0, 1.0]] * pixel_count

		# Here we write the pixel values to the RenderResult
		result = self.begin_result(0, 0, self.size_x, self.size_y)
		layer = result.layers[0]
		layer.rect = blue_rect
		self.end_result(result)
		
	def view_update(self, context):
		#TODO: More robust initialization
		if not self.network:
			self.network = Network()
			self.network.connect()
	
		scene = context.scene
		object = scene.objects[0]
		self.syncObj( object, scene )
		return
	
	def syncObj( self, object, scene ):
		#if object.is_visible( scene ):
		if 'DERGO' not in object:
			object['DERGO'] = { 'in_sync' : False, 'id' : self.objId, 'id_mesh' : 0 }
			self.objId += 1

		objDergoProps = object['DERGO']

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
		
	def view_draw(self, context):
		scale = context.scene.render.resolution_percentage / 100.0
		#self.size_x = int(context.scene.render.resolution_x * scale)
		#self.size_y = int(context.scene.render.resolution_y * scale)
		self.size_x = 640
		self.size_y = 480
		#self.render_preview( context.scene )
		# Update the screen
		bufferSize = self.size_x * self.size_y * 3
		#time.sleep( 5 )
		self.viewImageBufferFloat = [0.5] * bufferSize
		glBuffer = bgl.Buffer(bgl.GL_FLOAT, [bufferSize], self.viewImageBufferFloat)
		#glBuffer = bgl.Buffer(bgl.GL_FLOAT, [bufferSize])
		bgl.glRasterPos2i(0, 0)
		bgl.glDrawPixels(self.size_x, self.size_y, bgl.GL_RGB, bgl.GL_FLOAT, glBuffer)
		
def register():
	bpy.utils.register_class(DergoRenderEngine)
	
	# RenderEngines also need to tell UI Panels that they are compatible
	# Otherwise most of the UI will be empty when the engine is selected.
	# In this example, we need to see the main render image button and
	# the material preview panel.
	from bl_ui import properties_render
	properties_render.RENDER_PT_render.COMPAT_ENGINES.add('DERGO3D')
	del properties_render

	from bl_ui import properties_material
	properties_material.MATERIAL_PT_preview.COMPAT_ENGINES.add('DERGO3D')
	del properties_material

def unregister():
	bpy.utils.unregister_class(DergoRenderEngine)
