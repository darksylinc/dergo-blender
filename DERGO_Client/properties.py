
import bpy
from bpy.props import (BoolProperty,
					   EnumProperty,
					   FloatProperty,
					   IntProperty,
					   PointerProperty)

import math

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

def register():
	bpy.utils.register_class(DergoSpaceViewSettings)

def unregister():
	bpy.types.SpaceView3D.dergo = None
	bpy.utils.unregister_class(DergoSpaceViewSettings)
