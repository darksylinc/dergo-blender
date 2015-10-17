
import bpy
import bgl
import mathutils
import time

from .mesh_export import MeshExport
from .network import  *
from .engine import *
from .ui import *

bl_info = {
	 "name": "DERGO3D",
	 "author": "Matías N. Goldberg",
	 "version": (2, 1),
	 "blender": (2, 76, 0),
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
	# We need this set to True so that DergoRenderEngine isn't restarted while we animate.
	bl_use_shading_nodes = True
	
	def __init__(self):
		print( "Reinit" )
		#self.initialized = False
		engine.dergo.numActiveRenderEngines += 1
		self.needsReset = False
		if engine.dergo.numActiveRenderEngines == 1:
			self.needsReset = True
		
	def __del__(self):
		print( "Deinit" )
		if engine.dergo is not None:
			engine.dergo.numActiveRenderEngines -= 1
		
	def bake(self, scene, obj, pass_type, object_id, pixel_array, num_pixels, depth, result):
		return
	def update_script_node(self, node):
		return

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
		result.layers[0].passes[0].rect = green_rect
		self.end_result(result)

	# In this example, we fill the full renders with a flat blue color.
	def render_scene(self, scene):
		pixel_count = self.size_x * self.size_y

		# The framebuffer is defined as a list of pixels, each pixel
		# itself being a list of R,G,B,A values
		blue_rect = [[0.2, 0.4, 6.0, 1.0]] * pixel_count

		# Here we write the pixel values to the RenderResult
		result = self.begin_result(0, 0, self.size_x, self.size_y)
		result.layers[0].passes[0].rect = blue_rect
		self.end_result(result)
		
	def view_update(self, context):
		if self.needsReset:
			engine.dergo.reset()
			self.needsReset = False

		engine.dergo.view_update( context )
		return
		
	def view_draw(self, context):
		if ui.isInDummyMode( context ):
			return

		self.renderedView = False
		
		size_x = int(context.region.width)
		size_y = int(context.region.height)
		engine.dergo.sendViewRenderRequest( context, context.area, context.region_data,\
											True, size_x, size_y )
		
		while not self.renderedView:
			engine.dergo.network.receiveData( self )
		#context.area.tag_redraw()

	def processMessage( self, header_sizeBytes, header_messageType, data ):
		if header_messageType == FromServer.Result:
			self.renderedView = True
			resolution = struct.unpack_from( '=HH', memoryview( data ) )
			imageSizeBytes = resolution[0] * resolution[1] * 4
			glBuffer = bgl.Buffer(bgl.GL_BYTE, [imageSizeBytes], list(data[4:4+imageSizeBytes]))
			bgl.glRasterPos2i(0, 0)
			bgl.glDrawPixels( resolution[0], resolution[1], bgl.GL_RGBA, bgl.GL_UNSIGNED_BYTE, glBuffer )

	#scene['dergo']. bpy.context.window.screen.name

#global drawHandle
def register():
	from . import properties

	engine.register()
	bpy.utils.register_class(DergoRenderEngine)
	
	# RenderEngines also need to tell UI Panels that they are compatible
	# Otherwise most of the UI will be empty when the engine is selected.
	# In this example, we need to see the main render image button and
	# the material preview panel.
	#from bl_ui import properties_render
	#properties_render.RENDER_PT_render.COMPAT_ENGINES.add('DERGO3D')
	#del properties_render

	#from bl_ui import properties_material
	#properties_material.MATERIAL_PT_preview.COMPAT_ENGINES.add('DERGO3D')
	#del properties_material

	properties.register()
	ui.register()
	bpy.utils.register_module(__name__)

def unregister():
	from . import properties

	ui.unregister()
	properties.unregister()

	bpy.utils.unregister_class(DergoRenderEngine)
		
	engine.unregister()
	bpy.utils.unregister_module(__name__)