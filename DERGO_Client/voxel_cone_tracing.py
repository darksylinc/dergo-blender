
import bpy
from bpy.props import (BoolProperty,
					   EnumProperty,
					   FloatProperty,
					   FloatVectorProperty,
					   IntProperty,
					   PointerProperty,
					   StringProperty)

from .network import  *
from .ui_base import DergoButtonsPanel

enum_debug_vct_visualization = (
	('DEBUG_VISUAL_VCT_ALBEDO', "Albedo", ""),
	('DEBUG_VISUAL_VCT_NORMAL', "Normal", ""),
	('DEBUG_VISUAL_VCT_EMISSIVE', "Emissive", ""),
	('DEBUG_VISUAL_VCT_NONE', "None", ""),
	('DEBUG_VISUAL_VCT_LIGHT', "Light", ""),
	)

#
#		VCT PROBE
#
class DergoObjectVoxelConeTracing:
	@staticmethod
	def registerExtraProperties(cls):
		cls.vct_is_probe = BoolProperty(
				name="Use as VCT probe",
				default=False,
				)
		cls.vct_auto_fit = BoolProperty(
				name="Auto Fit",
				default=False,
				)
		cls.vct_num_bounces = IntProperty(
				name="Num. Bounces",
				description="",
				min=0, max=12,
				default=4,
				)
		cls.vct_width = IntProperty(
				name="Width",
				description="",
				min=4, max=1024,
				default=32,
				)
		cls.vct_height = IntProperty(
				name="Height",
				description="",
				min=4, max=1024,
				default=32,
				)
		cls.vct_depth = IntProperty(
				name="Depth",
				description="",
				min=4, max=1024,
				default=32,
				)
		cls.vct_debug_visual = EnumProperty(
				name="Debug Visualization",
				items=enum_debug_vct_visualization,
				default='DEBUG_VISUAL_VCT_NONE',
				)

class Dergo_PT_empty_vct(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Voxel Cone Tracing"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		return (context.object and context.object.type == 'EMPTY' and\
				context.object.empty_draw_type == 'CUBE' and DergoButtonsPanel.poll(context))

	def draw(self, context):
		dergo = context.object.dergo
		self.layout.prop(dergo, "vct_is_probe")
		if dergo.vct_is_probe:
			self.layout.prop(dergo, "vct_auto_fit")
			self.layout.prop(dergo, "vct_num_bounces")
			self.layout.prop(dergo, "vct_width")
			self.layout.prop(dergo, "vct_height")
			self.layout.prop(dergo, "vct_depth")

			self.layout.label( "Debug Visualization:" )
			self.layout.prop(dergo, "vct_debug_visual", expand=True)
