
import bpy

from . import engine
from .network import  *

def checkDergoInScene( scene ):
	if 'DERGO' not in scene:
		scene['DERGO'] = { 'async_preview' : {}, 'dummy_window' : {} }
		
def isInDummyMode( context ):
	checkDergoInScene( context.scene )
	dummyWindows = context.scene['DERGO']['dummy_window']
	
	screenName = context.window.screen.name
	if screenName not in dummyWindows:
		return False

	spaceId = str(context.area.spaces[0])
	return spaceId in dummyWindows[screenName]

from bpy.app.handlers import persistent
@persistent
def everyFrame( scene ):
	if scene.render.engine == "DERGO3D" and not engine.dergo:
		engine.dergo = engine.Engine()

	checkDergoInScene( scene )
	
	screenName = bpy.context.window.screen.name
	asyncPreviews = bpy.context.scene['DERGO']['async_preview']
	if screenName not in asyncPreviews:
		return

	engine.dergo.network.sendData( FromClient.InitAsync, None )
		
	# Iterate through all screens in the currently active window
	# and asynchronously render those that the user requested.
	for area in bpy.context.window.screen.areas:
		if area.type == 'VIEW_3D':
			spaceId = str(area.spaces[0])
			if spaceId in asyncPreviews[screenName]:
				region_data = area.spaces[0].region_3d
				engine.dergo.sendViewRenderRequest( bpy.context, area, region_data, False, 256, 256 )
				
	engine.dergo.network.sendData( FromClient.FinishAsync, None )
	return

def draw_async_preview(self, context):
	layout = self.layout
	scene = context.scene

	if scene.render.engine == "DERGO3D":
		checkDergoInScene( scene )
		
		screenName = bpy.context.window.screen.name
		asyncPreviews = scene['DERGO']['async_preview']
	
		view = context.space_data
		
		#hasDummyWindow = False
		#for area in bpy.context.window.screen.areas:
		#	if area.type == 'VIEW_3D' and area.spaces[0].viewport_shade == 'RENDERED':
		#		hasDummyWindow = True
		#		break
		hasDummyWindow = engine.dergo.numActiveRenderEngines != 0

		if view.viewport_shade != 'RENDERED':
			row = layout.row()
			row.operator("scene.dergo_toggle_dummy")
			row.operator("scene.async_preview")
			
			asyncEnabled = False
			if screenName not in asyncPreviews:
				statusText = 'OFF'
			else:
				spaceId = str(view)
				if spaceId in asyncPreviews[screenName]:
					statusText = ' ON'
					asyncEnabled = True
				else:
					statusText = ' OFF'

			if hasDummyWindow:
				if asyncEnabled:
					statusText += ', Rendering Async'
				else:
					statusText += ', Ready'
			if not hasDummyWindow:
				statusText += ', No Dummy window'

			#TODO
			#statusText += ', cannot connect to server'
				
			layout.label(text='STATUS: ' + statusText)
		else:
			row = layout.row()
			row.operator("scene.dergo_toggle_dummy")
			
class AsyncPreviewOperatorToggle(bpy.types.Operator):
	"""Tooltip"""
	bl_idname = "scene.async_preview"
	bl_label = "Async Preview in DERGO"
	bl_description = "Toggles DERGO Async Preview on current view. In order to work, there must be a dummy 3D View window set to 'Rendered' (Shift+Z)"

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		checkDergoInScene( context.scene )
		asyncPreviews = context.scene['DERGO']['async_preview']
		
		screenName = context.window.screen.name
		if screenName not in asyncPreviews:
			asyncPreviews[screenName] = {}
		
		spaceId = str(context.area.spaces[0])
		if spaceId in asyncPreviews[screenName]:
			del asyncPreviews[screenName][spaceId]
		else:
			asyncPreviews[screenName][spaceId] = 1
		return {'FINISHED'}
		
class DummyRendererOperatorToggle(bpy.types.Operator):
	"""Tooltip"""
	bl_idname = "scene.dergo_toggle_dummy"
	bl_label = "Set/Toggle as Dummy"
	bl_description = "Async Preview requires a window being set to 'Rendered'. The problem is that even a single 'Rendered' mode is slooow. To get the best performance, use dummy mode where nothing will be rendered in Blender, and the preview will be seen in the server's window (MUCH faster)"

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		checkDergoInScene( context.scene )

		wasInRenderedMode = bpy.context.space_data.viewport_shade == 'RENDERED'
		bpy.context.space_data.viewport_shade = 'RENDERED'

		dummyWindows = context.scene['DERGO']['dummy_window']
		screenName = context.window.screen.name
		if screenName not in dummyWindows:
			dummyWindows[screenName] = {}
		
		spaceId = str(context.area.spaces[0])
		if spaceId in dummyWindows[screenName]:
			if wasInRenderedMode:
				del dummyWindows[screenName][spaceId]
		else:
			dummyWindows[screenName][spaceId] = 1
		return {'FINISHED'}
		
class DergoButtonsPanel():
	bl_space_type = "PROPERTIES"
	bl_region_type = "WINDOW"
	bl_context = "render"

	@classmethod
	def poll(cls, context):
		rd = context.scene.render
		return rd.engine == 'DERGO3D'
		
class DergoLamp_PT_lamp(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Lamp"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		return context.lamp and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		lamp = context.lamp
		dlamp = lamp.dergo

		layout.prop(lamp, "type", expand=True)
		
		if lamp.type in {'AREA', 'HEMI'}:
			layout.label(text="Not supported. Skipped")
			return

		split = layout.split()

		col = split.column()
		sub = col.column()
		sub.prop(lamp, "color", text="")
		sub.prop(lamp, "energy")

		if lamp.type in {'POINT', 'SPOT'}:
			layout.label(text="Attenuation:")
			layout.prop(dlamp, "attenuation_mode")
			if dlamp.attenuation_mode == 'RADIUS':
				layout.prop(dlamp, "radius")
				layout.prop(dlamp, "radius_threshold", slider=True)
			else:
				layout.prop(dlamp, "range")

		col = split.column()
		col.prop(lamp, "use_negative")
		col.prop(dlamp, "cast_shadow")
		
class DergoLamp_PT_spot(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Spot Shape"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SPOT') and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		lamp = context.lamp
		dlamp = lamp.dergo

		split = layout.split()

		col = split.column()
		sub = col.column()
		sub.prop(lamp, "spot_size", text="Size")
		sub.prop(lamp, "spot_blend", text="Blend", slider=True)
		sub.prop(dlamp, "spot_falloff")

		col = split.column()
		col.prop(lamp, "show_cone")

def get_panels():
	return (
		bpy.types.RENDER_PT_render,
		bpy.types.RENDER_PT_output,
		bpy.types.RENDER_PT_encoding,
		bpy.types.RENDER_PT_dimensions,
		bpy.types.RENDER_PT_stamp,
		bpy.types.SCENE_PT_scene,
		bpy.types.SCENE_PT_audio,
		bpy.types.SCENE_PT_unit,
		bpy.types.SCENE_PT_keying_sets,
		bpy.types.SCENE_PT_keying_set_paths,
		bpy.types.SCENE_PT_physics,
		bpy.types.WORLD_PT_context_world,
		bpy.types.DATA_PT_context_mesh,
		bpy.types.DATA_PT_context_camera,
		bpy.types.DATA_PT_context_lamp,
		bpy.types.DATA_PT_texture_space,
		bpy.types.DATA_PT_curve_texture_space,
		bpy.types.DATA_PT_mball_texture_space,
		bpy.types.DATA_PT_vertex_groups,
		bpy.types.DATA_PT_shape_keys,
		bpy.types.DATA_PT_uv_texture,
		bpy.types.DATA_PT_vertex_colors,
		bpy.types.DATA_PT_camera,
		bpy.types.DATA_PT_camera_display,
		bpy.types.DATA_PT_lens,
		bpy.types.DATA_PT_custom_props_mesh,
		bpy.types.DATA_PT_custom_props_camera,
		bpy.types.DATA_PT_custom_props_lamp,
		bpy.types.TEXTURE_PT_clouds,
		bpy.types.TEXTURE_PT_wood,
		bpy.types.TEXTURE_PT_marble,
		bpy.types.TEXTURE_PT_magic,
		bpy.types.TEXTURE_PT_blend,
		bpy.types.TEXTURE_PT_stucci,
		bpy.types.TEXTURE_PT_image,
		bpy.types.TEXTURE_PT_image_sampling,
		bpy.types.TEXTURE_PT_image_mapping,
		bpy.types.TEXTURE_PT_musgrave,
		bpy.types.TEXTURE_PT_voronoi,
		bpy.types.TEXTURE_PT_distortednoise,
		bpy.types.TEXTURE_PT_voxeldata,
		bpy.types.TEXTURE_PT_pointdensity,
		bpy.types.TEXTURE_PT_pointdensity_turbulence,
		bpy.types.PARTICLE_PT_context_particles,
		bpy.types.PARTICLE_PT_emission,
		bpy.types.PARTICLE_PT_hair_dynamics,
		bpy.types.PARTICLE_PT_cache,
		bpy.types.PARTICLE_PT_velocity,
		bpy.types.PARTICLE_PT_rotation,
		bpy.types.PARTICLE_PT_physics,
		bpy.types.PARTICLE_PT_boidbrain,
		bpy.types.PARTICLE_PT_render,
		bpy.types.PARTICLE_PT_draw,
		bpy.types.PARTICLE_PT_children,
		bpy.types.PARTICLE_PT_field_weights,
		bpy.types.PARTICLE_PT_force_fields,
		bpy.types.PARTICLE_PT_vertexgroups,
		bpy.types.PARTICLE_PT_custom_props,
		)
		
def register():
	bpy.utils.register_class(AsyncPreviewOperatorToggle)
	bpy.utils.register_class(DummyRendererOperatorToggle)
	bpy.app.handlers.scene_update_post.append(everyFrame)
	bpy.types.VIEW3D_HT_header.append(draw_async_preview)

	for panel in get_panels():
		panel.COMPAT_ENGINES.add('DERGO3D')
		
def unregister():
	bpy.types.VIEW3D_HT_header.remove(draw_async_preview)
	bpy.app.handlers.scene_update_post.remove(everyFrame)
	bpy.utils.unregister_class(DummyRendererOperatorToggle)
	bpy.utils.unregister_class(AsyncPreviewOperatorToggle)
	
	for panel in get_panels():
		panel.COMPAT_ENGINES.remove('DERGO3D')
