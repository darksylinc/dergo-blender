
import bpy

from . import engine
from .network import  *
from .engine import PbsTexture

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
	if scene.render.engine != "DERGO3D":
		return

	if not engine.dergo:
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
		sub.prop(dlamp, "energy")

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

class Dergo_PT_context_material(DergoButtonsPanel, bpy.types.Panel):
	bl_label = ""
	bl_context = "material"
	bl_options = {'HIDE_HEADER'}

	@classmethod
	def poll(cls, context):
		return (context.material or context.object) and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		mat = context.material
		ob = context.object
		slot = context.material_slot
		space = context.space_data
		is_sortable = len(ob.material_slots) > 1

		if ob:
			rows = 1
			if (is_sortable):
				rows = 4

			row = layout.row()

			row.template_list("MATERIAL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

			col = row.column(align=True)
			col.operator("object.material_slot_add", icon='ZOOMIN', text="")
			col.operator("object.material_slot_remove", icon='ZOOMOUT', text="")

			col.menu("MATERIAL_MT_specials", icon='DOWNARROW_HLT', text="")

			if is_sortable:
				col.separator()

				col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
				col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

			if ob.mode == 'EDIT':
				row = layout.row(align=True)
				row.operator("object.material_slot_assign", text="Assign")
				row.operator("object.material_slot_select", text="Select")
				row.operator("object.material_slot_deselect", text="Deselect")

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "active_material", new="material.new")
			row = split.row()

			if slot:
				row.prop(slot, "link", text="")
			else:
				row.label()
		elif mat:
			split.template_ID(space, "pin_id")
			split.separator()
			
		if mat:
			layout.prop( mat.dergo, "brdf_type" )
			row = layout.row()
			row.alignment = 'RIGHT'
			
			for i in range( PbsTexture.NumPbsTextures ):
				texSlot = mat.texture_slots[i]
				if texSlot == None or texSlot.texture == None \
				or texSlot.texture.type != 'IMAGE' \
				or len( texSlot.texture.users_material ) > 1:
					col = row.column(align=True)
					col.operator( "material.dergo_fix_material" )
					break

			row.prop( context.scene.dergo, "check_material_errors" )
			row.prop( context.scene.dergo, "show_textures" )
		#TODO: Add type (e.g. PBS, UNLIT, TOON)

class FixMaterialTexture(bpy.types.Operator):
	"""Tooltip"""
	bl_idname = "material.dergo_fix_material"
	bl_label = "FIX TEXTURES"
	bl_description = "Setup a Dergo material to use textures the way we need"

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		mat = context.material

		for i in range( PbsTexture.NumPbsTextures ):
			texSlot = mat.texture_slots[i]
			if texSlot == None:
				texSlot = mat.texture_slots.create( i )
				
			if i == PbsTexture.Normal or (i >= PbsTexture.DetailNm0 and i <= PbsTexture.DetailNm3):
				texSlot.use_map_normal = True
				texSlot.use_map_color_diffuse = False
			else:
				texSlot.use_map_color_diffuse = True
			texSlot.texture_coords = 'UV'
			texSlot.mapping = 'FLAT'

			if texSlot.texture == None or texSlot.texture.type != 'IMAGE':
				tex = bpy.data.textures.new( mat.name + '_' + str(PbsTexture.Names[i]), type = 'IMAGE' )
				texSlot.texture = tex
				if i == PbsTexture.Normal or (i >= PbsTexture.DetailNm0 and i <= PbsTexture.DetailNm3):
					tex.use_normal_map = True
			elif texSlot.texture.type == 'IMAGE' and len( texSlot.texture.users_material ) > 1:
				tex = bpy.data.textures.new( mat.name + '_' + str(PbsTexture.Names[i]), type = 'IMAGE' )
				tex = texSlot.texture.copy()
				texSlot.texture = tex
				if i == PbsTexture.Normal or (i >= PbsTexture.DetailNm0 and i <= PbsTexture.DetailNm3):
					tex.use_normal_map = True

		return {'FINISHED'}
		
class FixMeshTangents(bpy.types.Operator):
	"""Tooltip"""
	bl_idname = "material.dergo_fix_mesh_tangents"
	bl_label = "Fix Mesh Tangents"
	bl_description = "A mesh using this material has no tangents, which are needed by normal maps. Use this to fix this for you."

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		mat = context.material
		
		for obj in bpy.data.objects:
			if type(obj.data) is bpy.types.Mesh \
			and mat.name in obj.data.materials \
			and len( obj.data.uv_textures ) != 0 \
			and obj.data.dergo.tangent_uv_source not in obj.data.uv_textures:
				obj.data.dergo.tangent_uv_source = obj.data.uv_textures[0].name

		return {'FINISHED'}

def drawTextureLayout( layout, scene, mat, textureType ):
	if not scene.dergo.show_textures:
		return
		
	texSlot = mat.texture_slots[textureType]
		
	if texSlot == None or texSlot.texture == None or texSlot.texture.type != 'IMAGE':
		return
	
	tex = texSlot.texture
	layout.template_ID(tex, "image", open="image.open")
	layout.template_image(tex, "image", tex.image_user, compact=True)
	
	# Tell engine we may be modifying the texture slots of active material.
	# This is a race condition since the other thread will be checking
	# whether this is True (and then set it to False) but we don't care.
	engine.dergo.textureSlotPanelOpen = True

class Dergo_PT_material_diffuse(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Diffuse"
	bl_context = "material"

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		mat = context.material
		dmat = mat.dergo
		layout.prop(mat, "diffuse_color", text="")
		
		sub = layout.row()
		sub.prop(dmat, "transparency", slider=True)

		split = layout.split()
		col = split.column()
		sub1 = col.column()
		sub1.prop( mat.dergo, "use_alpha_from_texture" )
		split.column().prop(dmat, "transparency_mode", text="")

		sub.enabled = dmat.transparency_mode != 'NONE'
		sub1.enabled = dmat.transparency_mode != 'NONE'

		drawTextureLayout( layout, context.scene, mat, PbsTexture.Diffuse )
		
class Dergo_PT_material_specular(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Specular"
	bl_context = "material"

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		mat = context.material
		dmat = mat.dergo
		layout.prop(mat, "specular_color", text="")
		drawTextureLayout( layout, context.scene, mat, PbsTexture.Specular )
		
		layout.prop(dmat, "roughness", slider=True)
		drawTextureLayout( layout, context.scene, mat, PbsTexture.Roughness )

class Dergo_PT_material_normal(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Normal Map"
	bl_context = "material"

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		scene = context.scene
		mat = context.material
		dmat = mat.dergo

		layout.prop(dmat, "normal_map_strength")
		
		texSlot = mat.texture_slots[PbsTexture.Normal]
		if scene.dergo.check_material_errors and texSlot != None \
		and texSlot.texture != None and texSlot.texture.type == 'IMAGE' \
		and texSlot.texture.image != None:
			for obj in bpy.data.objects:
				if type(obj.data) is bpy.types.Mesh \
				and mat.name in obj.data.materials \
				and len( obj.data.uv_textures ) != 0 \
				and obj.data.dergo.tangent_uv_source not in obj.data.uv_textures:
					layout.operator( "material.dergo_fix_mesh_tangents" )
					break

		drawTextureLayout( layout, context.scene, mat, PbsTexture.Normal )
		
class Dergo_PT_material_fresnel(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Fresnel"
	bl_context = "material"

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		mat = context.material
		dmat = mat.dergo
		
		split = layout.split()

		col = split.column()
		sub = col.column()

		if dmat.fresnel_mode == 'COEFF':
			sub.prop(dmat, "fresnel_coeff", slider=True)
		elif dmat.fresnel_mode == 'IOR':
			sub.prop(dmat, "fresnel_ior")
		elif dmat.fresnel_mode == 'COLOUR':
			sub.prop(dmat, "fresnel_colour", text="")
		elif dmat.fresnel_mode == 'COLOUR_IOR':
			sub.prop(dmat, "fresnel_colour_ior")

		split.column().prop(dmat, "fresnel_mode", text="")

class DergoDetailPanelBase:
	def draw(self, context, detailIdx):
		layout = self.layout

		mat = context.material
		dmat = mat.dergo
		
		strIdx = str(detailIdx)
		layout.prop( dmat, "detail_unified" + strIdx )
		
		box = layout.box()
		box.prop( dmat, "detail_weight" + strIdx, slider=True )
		box.prop( dmat, "detail_blend_mode" + strIdx )
		box.prop( dmat, "detail_offset" + strIdx )
		box.prop( dmat, "detail_scale" + strIdx )

		box.label( "Diffuse" )
		drawTextureLayout( box, context.scene, mat, PbsTexture.Detail0 + detailIdx )

		box = layout.box()
		unifiedSettings = getattr( dmat, "detail_unified" + strIdx )
		if not unifiedSettings:
			box.prop( dmat, "detail_weight_nm" + strIdx, slider=True )
			box.prop( dmat, "detail_offset_nm" + strIdx )
			box.prop( dmat, "detail_scale_nm" + strIdx )
		box.label( "Normal map" )
		drawTextureLayout( box, context.scene, mat, PbsTexture.DetailNm0 + detailIdx )
		
		scene = context.scene
		texSlot = mat.texture_slots[PbsTexture.DetailNm0 + detailIdx]
		if scene.dergo.check_material_errors and texSlot != None \
		and texSlot.texture != None and texSlot.texture.type == 'IMAGE' \
		and texSlot.texture.image != None:
			for obj in bpy.data.objects:
				if type(obj.data) is bpy.types.Mesh \
				and mat.name in obj.data.materials \
				and len( obj.data.uv_textures ) != 0 \
				and obj.data.dergo.tangent_uv_source not in obj.data.uv_textures:
					layout.operator( "material.dergo_fix_mesh_tangents" )
					break

class Dergo_PT_material_detail0(DergoDetailPanelBase, DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Detail #1"
	bl_context = "material"
	bl_options = {'DEFAULT_CLOSED'}

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		DergoDetailPanelBase.draw( self, context, 0 )

class Dergo_PT_material_detail1(DergoDetailPanelBase, DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Detail #2"
	bl_context = "material"
	bl_options = {'DEFAULT_CLOSED'}

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		DergoDetailPanelBase.draw( self, context, 1 )

class Dergo_PT_material_detail2(DergoDetailPanelBase, DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Detail #3"
	bl_context = "material"
	bl_options = {'DEFAULT_CLOSED'}

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		DergoDetailPanelBase.draw( self, context, 2 )

class Dergo_PT_material_detail3(DergoDetailPanelBase, DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Detail #4"
	bl_context = "material"
	bl_options = {'DEFAULT_CLOSED'}

	@classmethod
	def poll(cls, context):
		return context.material and DergoButtonsPanel.poll(context)

	def draw(self, context):
		DergoDetailPanelBase.draw( self, context, 3 )
			
class Dergo_PT_mesh(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "DERGO"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		return context.mesh and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout

		mesh = context.mesh
		dmesh = context.mesh.dergo

		layout.prop_search( dmesh, "tangent_uv_source", mesh, "uv_textures", text="UV for normal maps" )

class DergoTexturePanel(DergoButtonsPanel):
	bl_context = "texture"

	@classmethod
	def poll(cls, context):
		#return context.material and DergoButtonsPanel.poll(context)
		return (context.active_object and context.active_object.active_material
				and DergoButtonsPanel.poll(context))

	@staticmethod
	def getActiveTexture( context ):
		if context.active_object and context.active_object.active_material:
			return context.active_object.active_material.active_texture
		return None
	
class DergoTexture_PT_context(DergoTexturePanel, bpy.types.Panel):
	bl_label = ""
	bl_options = {'HIDE_HEADER'}

	def draw(self, context):
		layout = self.layout

		slot = getattr(context, "texture_slot", None)
		node = getattr(context, "texture_node", None)
		space = context.space_data
		#tex = context.texture
		#idblock = context.material
		# context.material & context.texture are None due to bl_use_shading_nodes = True
		idblock = context.active_object.active_material
		tex = idblock.active_texture
		pin_id = space.pin_id

		space.use_limited_texture_context = True

		tex_collection = (pin_id is None) and (node is None) and (not isinstance(idblock, bpy.types.Brush))

		if tex_collection:
			layout.template_list("TEXTURE_UL_texslots", "", idblock, "texture_slots", idblock, "active_texture_index", rows=2)

class DergoTexture_PT_dergo(DergoTexturePanel, bpy.types.Panel):
	bl_label = "Dergo Texture sampling"

	@classmethod
	def poll(cls, context):
		activeTexture = DergoTexturePanel.getActiveTexture( context )
		return DergoTexturePanel.poll(context) and \
				type(DergoTexturePanel.getActiveTexture( context )) is bpy.types.ImageTexture

	def draw(self, context):
		layout = self.layout

		mat = context.active_object.active_material
		dmat = mat.dergo
		texIdx = context.active_object.active_material.active_texture_index
		strTexIdx = str(texIdx)

		layout.prop( dmat, "filter" + strTexIdx )
		layout.prop( dmat, "u" + strTexIdx )
		layout.prop( dmat, "v" + strTexIdx )
		layout.prop( dmat, "uvSet" + strTexIdx )
		
		if getattr( dmat, "u" + strTexIdx ) == 'BORDER' \
		or getattr( dmat, "v" + strTexIdx ) == 'BORDER':
			row = layout.row()
			row.prop( dmat, "border_colour" + strTexIdx )
			row.prop( dmat, "border_alpha" + strTexIdx, slider=True )

class DergoTexture_PT_preview(DergoTexturePanel, bpy.types.Panel):
	bl_label = "Preview"

	def draw(self, context):
		layout = self.layout

		activeMaterial = context.active_object.active_material
		
		tex = activeMaterial.active_texture
		slot = activeMaterial.texture_slots[activeMaterial.active_texture_index]
		idblock = context.active_object.active_material

		if idblock:
			layout.template_preview(tex, parent=idblock, slot=slot)
		else:
			layout.template_preview(tex, slot=slot)

class DergoTexture_PT_image(DergoTexturePanel, bpy.types.Panel):
	bl_label = "Image"

	@classmethod
	def poll(cls, context):
		return DergoTexturePanel.poll(context) and \
				type(DergoTexturePanel.getActiveTexture( context )) is bpy.types.ImageTexture

	def draw(self, context):
		layout = self.layout

		tex = DergoTexturePanel.getActiveTexture( context )
		layout.template_image(tex, "image", tex.image_user)

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
	bpy.utils.register_class(FixMaterialTexture)
	bpy.app.handlers.scene_update_post.append(everyFrame)
	bpy.types.VIEW3D_HT_header.append(draw_async_preview)

	for panel in get_panels():
		panel.COMPAT_ENGINES.add('DERGO3D')
		
def unregister():
	bpy.types.VIEW3D_HT_header.remove(draw_async_preview)
	bpy.app.handlers.scene_update_post.remove(everyFrame)
	bpy.utils.unregister_class(FixMaterialTexture)
	bpy.utils.unregister_class(DummyRendererOperatorToggle)
	bpy.utils.unregister_class(AsyncPreviewOperatorToggle)
	
	for panel in get_panels():
		panel.COMPAT_ENGINES.remove('DERGO3D')
