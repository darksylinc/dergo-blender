
import bpy
from bpy.props import (BoolProperty,
					   EnumProperty,
					   FloatProperty,
					   IntProperty,
					   PointerProperty)

import math

enum_attenuation_mode = (
	('RANGE', "Range", "Light affects everything that is within the range. Very intuitive but not physically based."),
	('RADIUS', "Radius", "Specify the radius of the light (e.g. light bulb is a couple centimeters, sun is ~696km). "
				"Technically light never fades 100% over distance. Use the threshold to determine the % of luminance "
				"at which the light is considered too dim and is cut off (for performance reasons)."),
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
				"lumThreshold = 1 means nothing is affected by the light.",
				min=0, max=1.0,
				default=0.00392,
				)
		cls.range = FloatProperty(
				name="Range",
				description="Everything inside the range is affected by the light.",
				min=0, default=5,
				)
		cls.spot_falloff = FloatProperty(
				name="Falloff",
				min=0.001, default=1.0,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.Lamp.dergo

def register():
	bpy.utils.register_class(DergoSpaceViewSettings)
	bpy.utils.register_class(DergoLampSettings)

def unregister():
	bpy.utils.unregister_class(DergoLampSettings)
	bpy.utils.unregister_class(DergoSpaceViewSettings)
