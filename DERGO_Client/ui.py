
import bpy

from . import engine

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

	# Iterate through all screens in the currently active window
	# and asynchronously render those that the user requested.
	for area in bpy.context.window.screen.areas:
		if area.type == 'VIEW_3D':
			spaceId = str(area.spaces[0])
			if spaceId in asyncPreviews[screenName]:
				region_data = area.spaces[0].region_3d
				engine.dergo.sendViewRenderRequest( bpy.context, area, region_data, False, 256, 256 )
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

		bpy.context.space_data.viewport_shade = 'RENDERED'

		dummyWindows = context.scene['DERGO']['dummy_window']
		screenName = context.window.screen.name
		if screenName not in dummyWindows:
			dummyWindows[screenName] = {}
		
		spaceId = str(context.area.spaces[0])
		if spaceId in dummyWindows[screenName]:
			del dummyWindows[screenName][spaceId]
		else:
			dummyWindows[screenName][spaceId] = 1
		return {'FINISHED'}

def register():
	bpy.utils.register_class(AsyncPreviewOperatorToggle)
	bpy.utils.register_class(DummyRendererOperatorToggle)
	bpy.app.handlers.scene_update_post.append(everyFrame)
	bpy.types.VIEW3D_HT_header.append(draw_async_preview)

def unregister():
	bpy.types.VIEW3D_HT_header.remove(draw_async_preview)
	bpy.app.handlers.scene_update_post.remove(everyFrame)
	bpy.utils.unregister_class(DummyRendererOperatorToggle)
	bpy.utils.unregister_class(AsyncPreviewOperatorToggle)
