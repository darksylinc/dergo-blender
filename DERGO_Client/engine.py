
import bpy
import bgl
import mathutils
import ctypes

from .mesh_export import MeshExport
from .network import  *

class PbsTexture:
	Diffuse, \
	Normal, \
	Specular, \
	Roughness, \
	DetailWeight, \
	Detail0, \
	Detail1, \
	Detail2, \
	Detail3, \
	DetailNm0, \
	DetailNm1, \
	DetailNm2, \
	DetailNm3, \
	Reflection, \
	NumPbsTextures = range( 15 )
	Names = ['DIFFUSE', 'NORMAL', 'SPECULAR', 'ROUGHNESS', 'DETAIL_WEIGHTS', \
			'DETAIL0', 'DETAIL1', 'DETAIL2', 'DETAIL3', \
			'DETAIL_NORMAL0', 'DETAIL_NORMAL1', 'DETAIL_NORMAL2', 'DETAIL_NORMAL3', \
			'REFLECTION', 'INVALID']
	
class TextureMapType:
	Diffuse, \
	Monochrome, \
	Normal, \
	Env_Map = range( 4 )

BlenderLightTypeToOgre = { 'POINT' : 1, 'SUN' : 0, 'SPOT' : 2 }
BlenderBrdfTypeToOgre = { 'DEFAULT' : 0, 'COOKTORR' : 1, 'DEFAULT_UNCORRELATED' : 0x80000000,\
'SEPARATE_DIFFUSE_FRESNEL' : 0x40000000, 'COOKTORR_SEPARATE_DIFFUSE_FRESNEL' : 0x40000001 }
BlenderTransparencyModeToOgre = { 'NONE' : 0, 'TRANSPARENT' : 1, 'FADE' : 2 }
BlenderFilterToOgre = { 'POINT' : 0, 'BILINEAR' : 1, 'TRILINEAR' : 2, 'ANISOTROPIC' : 3 }
BlenderTexAddressToOgre = { 'WRAP' : 0, 'MIRROR' : 1, 'CLAMP' : 2, 'BORDER' : 3 }
BlenderBlendModeToOgre = { 'NORMAL' : 0, 'NORMAL_PREMUL' : 1, 'ADD' : 2, 'SUBTRACT' : 3, \
'MULTIPLY' : 4, 'MULTIPLY2X' : 5, 'SCREEN' : 6, 'OVERLAY' : 7, 'LIGHTEN' : 8, 'DARKEN' : 9, \
'GRAIN_E' : 10, 'GRAIN_M' : 11, 'DIFFERENCE' : 12 }
BlenderCmpFuncToOgre = { 'ALWAYS_FAIL' : 0, 'ALWAYS_PASS' : 1, 'LESS' : 2, 'LESS_EQUAL' : 3, \
'EQUAL' : 4, 'NOT_EQUAL' : 5, 'GREATER_EQUAL' : 6, 'GREATER' : 7 }
BlenderMaterialWorkflowToOgre = { 'SPECULAR' : 0, 'FRESNEL' : 1, 'METALLIC' : 2 }
BlenderCullModeToOgre = { 'AUTO' : 0, 'NONE' : 1, 'CW' : 2, 'CCW' : 3 }

class Engine:
	numActiveRenderEngines = 0

	def __init__(self):
		self.objId	= 1
		self.meshId	= 1
		self.matId	= 1
		
		self.frame = 1
		
		self.textureSlotPanelOpen = False

		self.activeObjects	= set()
		self.activeLights	= set()
		
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
			object.dergo.in_sync	= False
			object.dergo.id			= 0
			object.dergo.id_mesh	= 0
			object.dergo.name		= ''
		for mesh in bpy.data.meshes:
			mesh.dergo.frame_sync	= 0
			mesh.dergo.id			= 0
		for mat in bpy.data.materials:
			mat.dergo.in_sync	= False
			mat.dergo.id		= 0
			mat.dergo.name		= ''
		for image in bpy.data.images:
			image.dergo.in_sync	= False

		self.objId	= 1
		self.meshId	= 1
		self.matId	= 1
		
		self.frame	= 1
		
		self.activeObjects	= set()
		self.activeLights	= set()
		
	def view_update(self, context):
		scene = context.scene
		
		newActiveObjects	= set()
		newActiveLights		= set()
		
		for tex in bpy.data.textures:
			self.syncTexture( tex )
		
		# First update materials. They're rarely destroyed
		# (they only do via script or after reloading file)
		# so we don't track which ones were destroyed.
		# We let them leak until next reset.
		needToReset = False
		for mat in bpy.data.materials:
			if self.syncMaterial( mat ) == False:
				needToReset = True
				break
		if needToReset:
			self.reset()
			for tex in bpy.data.textures:
				self.syncTexture( tex )
			for mat in bpy.data.materials:
				self.syncMaterial( mat )
		
		# We can't check whether the texture slots in
		# a material were changed or are dirty, but they
		# they can only change when they're active. Only
		# send all of them when we've had a reset.
		# Otherwise just send the active one.
		#
		# Additionally, we only do it if the 'Material' or
		# the "Texture" panels are open (only way for UI
		# to modify it). It's a race condition, but doesn't
		# matter since the user is normally not that fast.
		if self.frame == 1:
			# A reset. 
			for mat in bpy.data.materials:
				self.syncMaterialTextureSlots( mat )
		elif self.textureSlotPanelOpen:
			obj = context.active_object
			if obj and obj.active_material:
				self.syncMaterialTextureSlots( obj.active_material )
			self.textureSlotPanelOpen = False
		
		# Add and update all meshes & items
		for object in scene.objects:
			if not object.is_visible( scene ):
				object.dergo.in_sync = False
				if object.is_updated_data and object.type == 'MESH':
					object.data.dergo.frame_sync = 0
			elif object.type == 'MESH':
				self.syncItem( object, scene )
				newActiveObjects.add( (object.dergo.id, object.dergo.id_mesh) )
			elif object.type == 'LAMP':
				self.syncLight( object, scene )
				newActiveLights.add( object.dergo.id )
		
		# Remove items that are gone.
		removedObjects = self.activeObjects - newActiveObjects
		for idPair in removedObjects:
			self.network.sendData( FromClient.ItemRemove, struct.pack( '=ll', idPair[1], idPair[0] ) )
		
		self.activeObjects = newActiveObjects
		
		# Remove lights that are gone.
		if len( newActiveLights  ) < len( self.activeLights ):
			removedLights = self.activeLights - newActiveLights
			for lightId in removedLights:
				self.network.sendData( FromClient.LightRemove, struct.pack( '=l', lightId ) )
		
		self.activeLights = newActiveLights
		
		# Always keep in 32-bit signed range, non-zero
		self.frame = (self.frame % 2147483647) + 1
		return
		
	# Removes all objects with the same ID as selected (i.e. user duplicated an object
	# and now we're dealing with duplicated IDs). Removes from server and deletes its
	# associated DERGO data. Mesh is not removed from server.
	def removeObjectsWithId( self, id, scene ):
		for object in scene.objects:
			if object.dergo.id == id:
				if object.type == 'LAMP':
					self.network.sendData( FromClient.LightRemove, struct.pack( '=l', object.dergo.id ) )
				else:
					self.network.sendData( FromClient.ItemRemove, struct.pack( '=ll', object.dergo.id_mesh, object.dergo.id ) )

				object.dergo.in_sync	= False
				object.dergo.id			= 0
				object.dergo.id_mesh	= 0
				object.dergo.name		= ''
				if object.type == 'MESH':
					object.data.dergo.frame_sync= 0
					object.data.dergo.id		= 0
	
	def syncItem( self, object, scene ):
		if object.dergo.id == 0:
			object.dergo.id		= self.objId
			object.dergo.name	= object.name
			self.objId += 1
		
		if object.dergo.name != object.name:
			# Either user changed its name, or user hit "Duplicate" on the object; thus getting same ID.
			self.removeObjectsWithId( object.dergo.id, scene )
			object.dergo.in_sync	= False
			object.dergo.id			= self.objId
			object.dergo.id_mesh	= 0
			object.dergo.name		= object.name
			self.objId += 1

		# Server doesn't have object, or object was moved, or
		# mesh was modified, or modifier requires an update.
		#	print( object.is_updated_data )	# True when skeleton moved
		#	print( object.data.is_updated )	# False when skeleton moved
		if not object.dergo.in_sync or object.is_updated or object.is_updated_data:
			if object.data.dergo.id == 0:
				object.data.dergo.id = self.meshId
				self.meshId += 1
				
			data = object.data
			
			if len( object.modifiers ) > 0:
				meshName = '##internal##_' + object.name
				linkedMeshId = ctypes.c_int32( object.dergo.id | 0x80000000 ).value
			else:
				meshName = data.name
				linkedMeshId = data.dergo.id
			
			# Check if mesh changed, or if our modifiers made an update, and that
			# we haven't already sync'ed this object (only if shared)
			if \
			((not object.dergo.in_sync or object.is_updated_data) and len( object.modifiers ) > 0) or \
			((data.dergo.frame_sync == 0 or (data.dergo.frame_sync != self.frame and object.is_updated_data)) and len( object.modifiers ) == 0):
				exportMesh = object.to_mesh( scene, True, "PREVIEW", True, False)
				
				if not data.dergo.tangent_uv_source:
					tangentUvSource = 255
				else:
					tangentUvSource = data.uv_textures.find( data.dergo.tangent_uv_source )
					if tangentUvSource < 0: tangentUvSource = 255
					
				dataToSend = MeshExport.createSendBuffer( linkedMeshId, meshName,
															exportMesh, tangentUvSource )
				
				self.network.sendData( FromClient.Mesh, dataToSend )
				bpy.data.meshes.remove( exportMesh )
				if len( object.modifiers ) == 0:
					data.dergo.frame_sync = self.frame
			
			# Item is now linked to a different mesh! Remove ourselves			
			if object.dergo.id_mesh != 0 and object.dergo.id_mesh != linkedMeshId:
				self.network.sendData( FromClient.ItemRemove, struct.pack( '=ll', object.dergo.id_mesh, object.dergo.id ) )
				object.dergo.in_sync = False

			# Keep it up to date.
			object.dergo.id_mesh = linkedMeshId

			# Create or Update Item.
			if not object.dergo.in_sync or object.is_updated:
				# Mesh ID & Item ID
				dataToSend = bytearray( struct.pack( '=ll', linkedMeshId, object.dergo.id ) )
				
				# Item name
				asUtfBytes = object.data.name.encode('utf-8')
				dataToSend.extend( struct.pack( '=I', len( asUtfBytes ) ) )
				dataToSend.extend( asUtfBytes )
				
				loc, rot, scale = object.matrix_world.decompose()
				dataToSend.extend( struct.pack( '=10f', loc[0], loc[1], loc[2],\
														rot[0], rot[1], rot[2], rot[3],\
														scale[0], scale[1], scale[2] ) )
				
				self.network.sendData( FromClient.Item, dataToSend )

			object.dergo.in_sync = True
			
	def syncLight( self, object, scene ):
		if object.data.type not in {'POINT', 'SUN', 'SPOT'}:
			return

		if object.dergo.id == 0:
			object.dergo.id		= self.objId
			object.dergo.name	= object.name
			self.objId += 1
		
		if object.dergo.name != object.name:
			# Either user changed its name, or user hit "Duplicate" on the object; thus getting same ID.
			self.removeObjectsWithId( object.dergo.id, scene )
			object.dergo.in_sync	= False
			object.dergo.id			= self.objId
			object.dergo.id_mesh	= 0
			object.dergo.name		= object.name
			self.objId += 1
		
		# Server doesn't have object, or object was moved, or
		# mesh was modified, or modifier requires an update.
		if not object.dergo.in_sync or object.is_updated or object.is_updated_data:
			# Light ID
			dataToSend = bytearray( struct.pack( '=l', object.dergo.id ) )
			
			# Light name
			asUtfBytes = object.name.encode('utf-8')
			dataToSend.extend( struct.pack( '=I', len( asUtfBytes ) ) )
			dataToSend.extend( asUtfBytes )
			
			lamp = object.data
			dlamp = object.data.dergo
			
			# Light data
			lightType = BlenderLightTypeToOgre[lamp.type]
			castShadows = dlamp.cast_shadow
			color = lamp.color
			loc, rot, scale = object.matrix_world.decompose()
			dataToSend.extend( struct.pack( '=3B11f',
				lightType, castShadows, lamp.use_negative,\
				color[0], color[1], color[2], dlamp.energy,\
				loc[0], loc[1], loc[2], rot[0], rot[1], rot[2], rot[3] ) )

			#if dlamp.attenuation_mode == 'RANGE':
			dataToSend.extend( struct.pack( '=2f', dlamp.radius, dlamp.radius_threshold ) )
			#else:
			#	dataToSend.extend( struct.pack( '=2f', dlamp.radius, dlamp.range ) )
			
			if lamp.type == 'SPOT':
				dataToSend.extend( struct.pack( '=3f', lamp.spot_size, lamp.spot_blend, dlamp.spot_falloff ) )
			
			self.network.sendData( FromClient.Light, dataToSend )

			object.dergo.in_sync = True

	@staticmethod
	def iorToCoeff( value ):
		fresnel = (1.0 - value) / (1.0 + value)
		return fresnel * fresnel
	@staticmethod
	def iorToCoeff3( values ):
		return [Engine.iorToCoeff(values[0]), Engine.iorToCoeff(values[1]), Engine.iorToCoeff(values[2])]

	# Returns False if it we need to reset
	def syncMaterial( self, object ):
		if object.dergo.id == 0:
			object.dergo.id		= self.matId
			object.dergo.name	= object.name
			self.matId += 1
		
		if object.dergo.name != object.name:
			# Either user changed its name, or user hit "Duplicate" on the object; thus getting same ID.
			# Removing a material is too cumbersome (they're rarely destroyed, and when they do,
			# Ogre needs to destroy the associated Items or temporarily change their materials;
			# then we would need to resync those items).
			# So... just reset.
			return False
		
		# Server doesn't have object, or object was moved, or
		# mesh was modified, or modifier requires an update.
		if not object.dergo.in_sync or object.is_updated:
			# Material ID
			dataToSend = bytearray( struct.pack( '=l', object.dergo.id ) )
			
			# Material name
			asUtfBytes = object.name.encode('utf-8')
			dataToSend.extend( struct.pack( '=I', len( asUtfBytes ) ) )
			dataToSend.extend( asUtfBytes )

			mat = object
			dmat = object.dergo
			
			# Material data
			dataToSend.extend( struct.pack( '=LBBBBB', \
				BlenderBrdfTypeToOgre[dmat.brdf_type], \
				BlenderMaterialWorkflowToOgre[dmat.workflow],
				BlenderCullModeToOgre[dmat.cull_mode],
				BlenderCullModeToOgre[dmat.cull_mode_shadow],
				dmat.two_sided,
				BlenderTransparencyModeToOgre[dmat.transparency_mode] ) )

			if dmat.transparency_mode != 'NONE':
				dataToSend.extend( struct.pack( '=fB', dmat.transparency, dmat.use_alpha_from_texture ) )
				
			dataToSend.extend( struct.pack( '=B',
				BlenderCmpFuncToOgre[dmat.alpha_test_cmp_func] ) )
				
			if dmat.alpha_test_cmp_func != 'ALWAYS_PASS' \
			and dmat.alpha_test_cmp_func != 'ALWAYS_FAIL':
				dataToSend.extend( struct.pack( '=f', dmat.alpha_test_threshold ) )

			dataToSend.extend( struct.pack( '=8f', \
				mat.diffuse_color[0], mat.diffuse_color[1], mat.diffuse_color[2],\
				mat.specular_color[0], mat.specular_color[1], mat.specular_color[2],\
				dmat.roughness, dmat.normal_map_strength ) )

			if dmat.workflow != 'METALLIC':
				if dmat.fresnel_mode == 'COEFF':
					dataToSend.extend( struct.pack( '=3f', \
						dmat.fresnel_coeff, dmat.fresnel_coeff, dmat.fresnel_coeff ) )
				elif dmat.fresnel_mode == 'IOR':
					fresnelCoeff = Engine.iorToCoeff( dmat.fresnel_ior )
					dataToSend.extend( struct.pack( '=3f', \
						fresnelCoeff, fresnelCoeff, fresnelCoeff ) )
				elif dmat.fresnel_mode == 'COLOUR':
					dataToSend.extend( struct.pack( '=3f', \
						dmat.fresnel_colour[0], dmat.fresnel_colour[1], dmat.fresnel_colour[2] ) )
				elif dmat.fresnel_mode == 'COLOUR_IOR':
					dataToSend.extend( struct.pack( '=3f', *Engine.iorToCoeff3( dmat.fresnel_colour_ior ) ) )
			else:
				dataToSend.extend( struct.pack( '=3f', dmat.metallic, 0, 0 ) )

			for i in range( PbsTexture.NumPbsTextures ):
				strTexIdx = str(i)
				filter = getattr( dmat, "filter" + strTexIdx )
				addressU = getattr( dmat, "u" + strTexIdx )
				addressV = getattr( dmat, "v" + strTexIdx )
				uvSet = getattr( dmat, "uvSet" + strTexIdx )

				dataToSend.extend( struct.pack( '=BB',
					(BlenderFilterToOgre[filter] << 4) |
					(BlenderTexAddressToOgre[addressV] << 2) |
					BlenderTexAddressToOgre[addressU], uvSet ) )
				
				if addressU == 'BORDER' or addressV == 'BORDER':
					borderColour = getattr( dmat, "border_colour" + strTexIdx )
					borderAlpha = getattr( dmat, "border_alpha" + strTexIdx )
					dataToSend.extend( struct.pack( '=4f',
						borderColour[0], borderColour[1],
						borderColour[2], borderAlpha ) )

			# Send detail map settings
			for i in range(4):
				strTexIdx = str(i)
				blendMode = getattr( dmat, "detail_blend_mode" + strTexIdx )
				detailWeight = getattr( dmat, "detail_weight" + strTexIdx )
				detailOffset = getattr( dmat, "detail_offset" + strTexIdx )
				detailScale = getattr( dmat, "detail_scale" + strTexIdx )
				
				dataToSend.extend( struct.pack( '=B5f',
						BlenderBlendModeToOgre[blendMode], detailWeight,
						detailOffset[0], detailOffset[1],
						detailScale[0], detailScale[1], ) )
			# Send detail normal map settings
			for i in range(4):
				strTexIdx = str(i)
				isUnified = getattr( dmat, "detail_unified" + strTexIdx )
				if not isUnified: strTexIdx = "_nm" + strTexIdx
				detailWeight = getattr( dmat, "detail_weight" + strTexIdx )
				detailOffset = getattr( dmat, "detail_offset" + strTexIdx )
				detailScale = getattr( dmat, "detail_scale" + strTexIdx )
				
				dataToSend.extend( struct.pack( '=5f',
						detailWeight,
						detailOffset[0], detailOffset[1],
						detailScale[0], detailScale[1], ) )

			self.network.sendData( FromClient.Material, dataToSend )

			object.dergo.in_sync = True
		return True

	def syncMaterialTextureSlots( self, mat ):
		for i in range( PbsTexture.NumPbsTextures ):
			slot = mat.texture_slots[i]
			dataToSend = bytearray( struct.pack( '=lB', mat.dergo.id, i ) )

			if \
			slot != None and slot.texture != None and \
			slot.texture.type == 'IMAGE' and slot.texture.image != None and\
			slot.use:
				tex = slot.texture
				# Texture ID
				dataToSend.extend( struct.pack( '=QB', tex.image.as_pointer(),
										Engine.getTextureMapTypeFromTex( tex ) ) )
			else:
				# No texture
				dataToSend.extend( struct.pack( '=QB', 0, 0 ) )

			self.network.sendData( FromClient.MaterialTexture, dataToSend )

	@staticmethod
	def getTextureMapTypeFromTex( tex ):
		if tex.use_normal_map:
			return TextureMapType.Normal
		return TextureMapType.Diffuse
		
	def syncTexture( self, tex ):
		if tex.type != 'IMAGE' or tex.image == None:
			return

		if not tex.image.dergo.in_sync or tex.image.is_updated:
			dataToSend = bytearray( struct.pack( '=QB', tex.image.as_pointer(),
										Engine.getTextureMapTypeFromTex( tex ) ) )

			#if tex.image.is_dirty #or tex.image.packed_file #Need to send the pixels

			# Texture path
			asUtfBytes = tex.image.filepath_from_user().encode('utf-8')
			dataToSend.extend( struct.pack( '=I', len( asUtfBytes ) ) )
			dataToSend.extend( asUtfBytes )

			self.network.sendData( FromClient.Texture, dataToSend )
			tex.image.dergo.in_sync = True
		return

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
			struct.pack( '=BqHH16fB', bAskForResult, hash(str(area.spaces[0])), \
						size_x, size_y,\
						area.spaces[0].lens,\
						32.0,\
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
