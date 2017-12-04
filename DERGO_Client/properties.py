
import bpy
from bpy.props import (BoolProperty,
					   EnumProperty,
					   FloatProperty,
					   FloatVectorProperty,
					   IntProperty,
					   PointerProperty,
					   StringProperty)

import math

from .instant_radiosity import *
from .parallax_corrected_cubemaps import *

enum_attenuation_mode = (
	('RANGE', "Range", "Light affects everything that is within the range. Very intuitive but not physically based."),
	('RADIUS', "Radius", "Specify the radius of the light (e.g. light bulb is a couple centimeters, sun is ~696km). "
				"Technically light never fades 100% over distance. Use the threshold to determine the % of luminance "
				"at which the light is considered too dim and is cut off (for performance reasons)."),
	)

enum_fresnel_mode = (
	('COEFF', "Coefficient", "Set the fresnel coefficient directly"),
	('IOR', "Index of Refraction", "Same as coefficient, but based on an IOR value"),
	('COLOUR', "Coloured", "Specify a coefficient for each individual RGB channel"),
	('COLOUR_IOR', "Coloured IOR", "Specify IOR for each individual RGB channel."),
	)
	
enum_transparency_mode = (
	('NONE', "No Transparency", "Disable transparency"),
	('TRANSPARENT', "Transparent", "Realistic transparency that preserves lighting reflections. Great for glass. Note that at t = 0 the object may not be fully invisible."),
	('FADE', "Fade", "Good 'ol regular alpha blending. Ideal for just fading out an object until it completely disappears"),
	)
	
enum_brdf_types = (
	('DEFAULT', "Default", "Most physically accurate BRDF we have. Good for representing majority of materials"),
	('COOKTORR', "CookTorrance", "Cook Torrance. Ideal for silk (use high roughness values), synthetic fabric"),
	('DEFAULT_UNCORRELATED', "Default Uncorrelated", "Similar to Default. Notably edges are dimmer and is less correct, but looks more like Unity (Marmoset too?)."),
	('SEPARATE_DIFFUSE_FRESNEL', "Separate Diffuse Fresnel", "For surfaces w/ complex refractions and reflections like glass, transparent plastics, fur, and surfaces w/ refractions and multiple rescattering that cannot be represented well w/ the default BRDF"),
	('COOKTORR_SEPARATE_DIFFUSE_FRESNEL', "Cook Torrance - Separate Diffuse Fresnel", "Ideal for shiny objects like glass toy marbles, some types of rubber"),
	)
	
enum_workflows = (
	('SPECULAR', "Specular (Ogre)", "Specular workflow. Specular texture is used as 'kS'"),
	('FRESNEL', "Specular (Fresnel, common)", "Specular workflow. Many PBRs use this mode. Specular texture affects fresnel. Use coloured fresnel to let the texture have colour"),
	('METALLIC', "Metallic", "Metallic workflow"),
	)
	
enum_cull_modes = (
	('AUTO', "Auto", "Defaults to cull back faces. Handles two sided lighting smartly."),
	('NONE', "None", "Shows both faces. Lighting could be incorrect if two-sided is disabled"),
	('CW', "CW", "Cull clockwise triangles (culls back faces)"),
	('CCW', "CCW", "Cull counter-clockwise triangles (culls front faces)"),
	)

enum_cmp_func = (
	#('ALWAYS_FAIL', "Invisible (Always fail)", ""),
	('ALWAYS_PASS', "Disabled (Always pass)", ""),
	('LESS', "Less", ""),
	('LESS_EQUAL', "Less or Equal", ""),
	('EQUAL', "Equal", ""),
	('NOT_EQUAL', "Not Equal", ""),
	('GREATER_EQUAL', "Greater or Equal", ""),
	('GREATER', "Greater", ""),
	)
	
enum_filtering_modes = (
	('POINT', "Point", "Nearest filter. Fast, but looks horrible"),
	('BILINEAR', "Bilinear", "Bilinear filtering"),
	('TRILINEAR', "Trilinear", "Like bilinear, but uses mipmaps for enhanced visual quality and higher performance"),
	('ANISOTROPIC', "Anisotropic", "Highest quality, specially on oblique viewing angles (like typically roads). But is more expensive"),
	)
	
enum_texture_addressing_modes = (
	('WRAP', "Wrap / Repeat", "Repeat the texture (wrap around)"),
	('MIRROR', "Mirror", "Repeat alternating the direction each time the end of the texture is reached"),
	('CLAMP', "Clamp", "Don't repeat the texture. Stretch the edge of the texture when the end is reached"),
	('BORDER', "Custom Border", "Like clamp, but a custom border is used (can be slow on mobile!)"),
	)
	
enum_detail_blending_modes = (
	('NORMAL', "Normal", "Texture layed on top of the other, using alpha to blend"),
	('NORMAL_PREMUL', "Normal Premultiplied", "Like normal, but assumes the alpha of the source is alpha-premultiplied"),
	('ADD', "Add", ""),
	('SUBTRACT', "Subtract", ""),
	('MULTIPLY', "Multiply", ""),
	('MULTIPLY2X', "Multiply 2x", ""),
	('SCREEN', "Screen", ""),
	('OVERLAY', "Overlay", ""),
	('LIGHTEN', "Lighten", ""),
	('DARKEN', "Darken", ""),
	('GRAIN_E', "Grain Extract", ""),
	('GRAIN_M', "Grain Merge", ""),
	('DIFFERENCE', "Difference", ""),
	)

class DergoSpaceViewSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(drg):
		# bpy.types.SpaceView3D.dergo = PointerProperty(
				# name="Dergo SpaceView3D Settings",
				# description="Dergo SpaceView3D settings",
				# type=drg,
				# )

		# drg.async_preview = BoolProperty(
				# name="Async Preview in DERGO",
				# description="Simultaneously shows the output of this view in DERGO. (needs a dummy view set to 'Rendered'!)",
				# default=False,
				# )
		return

	@classmethod
	def unregister(drg):
		#del bpy.types.SpaceView3D.dergo
		return

class DergoSceneSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Scene.dergo = PointerProperty(
				name="Dergo Scene Settings",
				description="Dergo scene settings",
				type=cls,
				)
		cls.show_textures = BoolProperty(
				name="Show Textures",
				description="Shows Textures embedded in the UI. Disable if they're cluttering too much",
				default=True,
				)
		cls.check_material_errors = BoolProperty(
				name="Check errors",
				description="Checks for errors in objects that make them incompatible with the material. Disable if UI responsiveness is degraded (e.g. many thousands of objects on scene)",
				default=True,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Scene.dergo

class DergoWorldSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.World.dergo = PointerProperty(
				name="Dergo World Settings",
				description="Dergo world settings",
				type=cls,
				)
		cls.in_sync = BoolProperty(
				name="in_sync",
				default=False,
				)
		cls.sky = FloatVectorProperty(
				name="Sky",
				description="Sky",
				min=0, max=1,
				default=(0.2, 0.4, 0.6),
				subtype='COLOR'
				)
		cls.sky_power = FloatProperty(
				name="Power",
				description="Sky Power, in 10k lumens (i.e. 97 = 97.000 lumens)",
				min=0.0, max=100,
				default=60.0,
				)
		cls.ambient_hemi_dir = FloatVectorProperty(
				name="Ambient Hemisphere Up Direction",
				description="Ambient Hemisphere Up Direction",
				min=-1, max=1,
				default=(0, 0, 1),
				subtype='XYZ'
				)
		cls.ambient_upper_hemi = FloatVectorProperty(
				name="Ambient Upper Hemisphere Colour",
				description="Ambient Upper Hemisphere Colour. Set both (upper & lower) to black to disable",
				min=0, max=1,
				default=(0.3, 0.50, 0.7),
				subtype='COLOR'
				)
		cls.ambient_upper_hemi_power = FloatProperty(
				name="Power",
				description="Upper Hemis. Power, in 10k lumens (i.e. 97 = 97.000 lumens)",
				min=0.0, max=100,
				default=4.5,
				)
		cls.ambient_lower_hemi = FloatVectorProperty(
				name="Ambient Lower Hemisphere Colour",
				description="Ambient Lower Hemisphere Colour. Set both (upper & lower) to black to disable",
				min=0, max=1,
				default=(0.6, 0.45, 0.3),
				subtype='COLOR'
				)
		cls.ambient_lower_hemi_power = FloatProperty(
				name="Power",
				description="Lower Hemis. Power, in 10k lumens (i.e. 97 = 97.000 lumens)",
				min=0.0, max=100,
				default=2.925,
				)
		cls.exposure = FloatProperty(
				name="Exposure",
				description="Exposure in EV steps",
				min=-5, max=5,
				default=0.0,
				)
		cls.min_auto_exposure = FloatProperty(
				name="Min Auto Exposure",
				description="Min Auto Exposure in EV steps",
				min=-5, max=5,
				default=-1.0,
				)
		cls.max_auto_exposure = FloatProperty(
				name="Max Auto Exposure",
				description="Max Auto Exposure in EV steps",
				min=-5, max=5,
				default=2.5,
				)
		cls.bloom_threshold = FloatProperty(
				name="Bloom Threshold",
				description="Bloom Threshold",
				min=-10, max=10,
				default=5.0,
				)
		cls.envmap_scale = FloatProperty(
				name="Environment Map Scale",
				description="Environment Map Scale",
				min=0, max=100,
				default=1.0,
				)

		bpy.utils.register_class(DergoWorldInstantRadiositySettings)
		cls.instant_radiosity = PointerProperty(
						name="Dergo Instant Radiosity (GI) Settings",
						description="Instant Radiosity (GI) settings",
						type=DergoWorldInstantRadiositySettings,
						)

		bpy.utils.register_class(DergoWorldPccSettings)
		cls.pcc = PointerProperty(
						name="Dergo PCC Settings",
						description="PCC Settings",
						type=DergoWorldPccSettings,
						)

	@classmethod
	def unregister(cls):
		del cls.pcc
		del cls.instant_radiosity
		bpy.utils.unregister_class(DergoWorldPccSettings)
		bpy.utils.unregister_class(DergoWorldInstantRadiositySettings)
		del bpy.types.World.dergo

class DergoObjectSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Object.dergo = PointerProperty(
				name="Dergo Object Settings",
				description="Dergo object settings",
				type=cls,
				)
		cls.in_sync = BoolProperty(
				name="in_sync",
				default=False,
				)
		cls.id = IntProperty(
				name="id",
				default=0,
				)
		cls.id_mesh = IntProperty(
				name="id_mesh",
				default=0,
				)
		cls.name = StringProperty(
				name="name",
				)
		cls.cast_shadow = BoolProperty(
				name="Cast Shadow",
				description="Object casts shadows",
				default=True,
				)
		DergoObjectInstantRadiosity.registerExtraProperties(cls)
		DergoObjectParallaxCorrectedCubemaps.registerExtraProperties(cls)
		cls.linked_area = StringProperty(
				name="Linked Area",
				description="IR: The radius of the chosen object will be used as sphere radius for the AoI. PCC: The area in which the probe becomes active"
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Object.dergo
		
class DergoMeshSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Mesh.dergo = PointerProperty(
				name="Dergo Mesh Settings",
				description="Dergo mesh settings",
				type=cls,
				)
		cls.frame_sync = IntProperty(
				name="frame_sync",
				description="__Internal__ Last frame mesh was sync'ed. When zero, a forced sync was requested",
				default=0,
				)
		cls.id = IntProperty(
				name="id",
				default=0,
				)
		cls.tangent_uv_source = StringProperty(
				description="Select UV source for generating tangents for normal maps. Blank for none (faster if you don't use normal maps!)",
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Mesh.dergo
		
class DergoLampSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Lamp.dergo = PointerProperty(
				name="Dergo Lamp Settings",
				description="Dergo lamp settings",
				type=cls,
				)
		cls.cast_shadow = BoolProperty(
				name="Cast Shadow",
				description="Lamp casts shadows",
				default=False,
				)
		cls.energy = FloatProperty(
				name="Energy",
				description="Amount of energy that the lamp emits",
				min=0.0, max=100000,
				default=3.14192,
				)
		cls.attenuation_mode = EnumProperty(
				name="Attenuation mode",
				items=enum_attenuation_mode,
				default='RADIUS',
				)
		cls.radius = FloatProperty(
				name="Radius",
				description="Light radius. (e.g. light bulb is a couple centimeters, sun is ~696km)",
				min=0, default=1.0,
				)
		cls.radius_threshold = FloatProperty(
				name="Threshold",
				description="Sets range at which the luminance (in percentage) of a point would go below "
				"the threshold. e.g. lumThreshold = 0 means the attenuation range is infinity; "
				"lumThreshold = 1 means nothing is affected by the light",
				min=0, max=0.9999,
				default=0.00392,
				)
		cls.range = FloatProperty(
				name="Range",
				description="Everything inside the range is affected by the light",
				min=0, default=5,
				)
		cls.spot_falloff = FloatProperty(
				name="Falloff",
				min=0.001, default=1.0,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Lamp.dergo
		
class DergoMaterialSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Material.dergo = PointerProperty(
				name="Dergo Material Settings",
				description="Dergo material settings",
				type=cls,
				)
		cls.in_sync = BoolProperty(
				name="in_sync",
				default=False,
				)
		cls.id = IntProperty(
				name="id",
				default=0,
				)
		cls.name = StringProperty(
				name="name",
				)
		cls.brdf_type = EnumProperty(
				name="BRDF Type",
				items=enum_brdf_types,
				default='DEFAULT',
				)
		cls.workflow = EnumProperty(
				name="Workflow",
				items=enum_workflows,
				default='METALLIC',
				)
		cls.two_sided = BoolProperty(
				name="Two Sided",
				description="Two Sided Lighting",
				default=False,
				)
		cls.cull_mode = EnumProperty(
				name="Cull Mode",
				items=enum_cull_modes,
				default='AUTO',
				)
		cls.cull_mode_shadow = EnumProperty(
				name="Cull Mode (Shadows)",
				items=enum_cull_modes,
				default='AUTO',
				)
		cls.transparency_mode = EnumProperty(
				name="Transparency Mode",
				items=enum_transparency_mode,
				default='NONE',
				)
		cls.transparency = FloatProperty(
				name="Transparency",
				min=0.0, max=1.0,
				default=1.0,
				)
		cls.metallic = FloatProperty(
				name="Metallic",
				min=0.0, max=1.0,
				default=1.0,
				)
		cls.alpha_test_cmp_func = EnumProperty(
				name="Alpha Test",
				description="'On or off' transparency. It's greatest strength is being fast & not having Z fighting or sorting issues. Useful for grass, leaves, trellis, grating, etc",
				items=enum_cmp_func,
				default='ALWAYS_PASS',
				)
		cls.alpha_test_threshold = FloatProperty(
				name="Alpha Test Threshold",
				min=0.0, max=1.0,
				default=1.0,
				)
		cls.use_alpha_from_texture = BoolProperty(
				name="Use Alpha from textures",
				description="When false, the alpha channel of the diffuse maps and detail maps will be ignored for transparency. It's a GPU performance optimization",
				default=True,
				)
		cls.roughness = FloatProperty(
				name="Roughness",
				description="Lamp casts shadows",
				min=0.001, max=1,
				default=1.0,
				)
		cls.normal_map_strength = FloatProperty(
				name="Strength",
				description="How strong the normal map is. Note: a value of 1 results in a faster shader",
				default=1.0,
				)
		cls.fresnel_mode = EnumProperty(
				name="Fresnel mode",
				items=enum_fresnel_mode,
				default='COEFF',
				)
		cls.fresnel_coeff = FloatProperty(
				name="Fresnel",
				description="Set the fresnel coefficient directly",
				min=0, max=1,
				default=0.818,
				)
		cls.fresnel_ior = FloatProperty(
				name="IOR",
				description="Set the fresnel based on an Index of Refraction (IOR)",
				min=0,
				default=0.050181050905482985,
				)
		cls.fresnel_colour = FloatVectorProperty(
				name="Fresnel Colour",
				description="Unity & Marmoset call this value 'specular colour'",
				min=0, max=1,
				default=(0.818, 0.818, 0.818),
				subtype='COLOR'
				)
		cls.fresnel_colour_ior = FloatVectorProperty(
				name="Fresnel Colour IOR",
				description="",
				min=0,
				default=(0.050181050905482985, 0.050181050905482985, 0.050181050905482985),
				subtype='XYZ'
				)
		for i in range( 16 ):
			setattr( cls, 'uvSet%i' % i, IntProperty(
					name="UV Set",
					description="",
					min=0, max=7,
					default=0
					) )
			setattr( cls, 'filter%i' % i, EnumProperty(
					name="Filter",
					items=enum_filtering_modes,
					default='TRILINEAR',
					) )
			setattr( cls, 'u%i' % i, EnumProperty(
					name="U",
					items=enum_texture_addressing_modes,
					default='WRAP',
					) )
			setattr( cls, 'v%i' % i, EnumProperty(
					name="V",
					items=enum_texture_addressing_modes,
					default='WRAP',
					) )
			setattr( cls, 'border_colour%i' % i, FloatVectorProperty(
					name="Border Colour",
					description="Colour when texture addressing mode is set to Custom Border",
					min=0, max=1,
					default=(0.0, 0.0, 0.0),
					subtype='COLOR'
					) )
			setattr( cls, 'border_alpha%i' % i, FloatProperty(
					name="Border Alpha",
					description="Alpha when texture addressing mode is set to Custom Border",
					min=0, max=1,
					default=1.0
					) )
		for i in range( 4 ):
			setattr( cls, 'detail_blend_mode%i' % i, EnumProperty(
					name="Blend",
					items=enum_detail_blending_modes,
					default='NORMAL',
					) )
			setattr( cls, 'detail_unified%i' % i, BoolProperty(
					name="Unified for Normal maps",
					description="Enable to affect both diffuse & normal detail maps with the same settings",
					default=True,
					) )
			setattr( cls, 'detail_weight%i' % i, FloatProperty(
					name="Weight",
					min=0, max=1,
					default=1.0,
					) )
			setattr( cls, 'detail_offset%i' % i, FloatVectorProperty(
					name="Offset",
					description="UV Offset. Beware of clamp modes",
					default=(0.0, 0.0),
					subtype='TRANSLATION', size=2,
					) )
			setattr( cls, 'detail_scale%i' % i, FloatVectorProperty(
					name="Scale",
					description="UV Scale. Beware of clamp modes",
					default=(1.0, 1.0),
					subtype='TRANSLATION', size=2,
					) )
			setattr( cls, 'detail_weight_nm%i' % i, FloatProperty(
					name="Weight",
					min=-5, max=5,
					default=1.0,
					) )

	@classmethod
	def unregister(cls):
		del bpy.types.Material.dergo
				
class DergoImageSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		bpy.types.Image.dergo = PointerProperty(
				name="Dergo Image Settings",
				description="Dergo Image settings",
				type=cls,
				)
		cls.in_sync = BoolProperty(
				name="in_sync",
				default=False,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Image.dergo

def register():
	bpy.utils.register_class(DergoSpaceViewSettings)
	bpy.utils.register_class(DergoWorldSettings)
	bpy.utils.register_class(DergoObjectSettings)
	bpy.utils.register_class(DergoMeshSettings)
	bpy.utils.register_class(DergoLampSettings)

def unregister():
	bpy.utils.unregister_class(DergoLampSettings)
	bpy.utils.unregister_class(DergoMeshSettings)
	bpy.utils.unregister_class(DergoObjectSettings)
	bpy.utils.unregister_class(DergoWorldSettings)
	bpy.utils.unregister_class(DergoSpaceViewSettings)
