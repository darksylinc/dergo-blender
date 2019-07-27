
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
				default=True,
				)
		cls.vct_num_bounces = IntProperty(
				name="Num. Bounces",
				description="",
				min=0, max=64,
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
		cls.vct_normal_bias = FloatProperty(
				name="Normal Bias",
				description="",
				default=0.05,
				)
		cls.vct_thin_wall_counter = FloatProperty(
				name="Thin Wall",
				description="Increase this value (e.g. to 2.0f) to fight light leaking. This should generally (over-)darken the scene",
				default=1.00,
				)
		cls.vct_specular_sdf_quality = FloatProperty(
				name="Specular SDF Quality",
				description="1 is high quality and 0 is high performance. Has now effect when resolution is <= 32",
				min=0, max=1.5,
				default=0.875,
				)
		cls.vct_auto_baking_mult = BoolProperty(
				name="Auto Baking Multiplier",
				default=True,
				)
		cls.vct_baking_multiplier = FloatProperty(
				name="Baking Multiplier",
				description="",
				min=0.001,
				default=1.00,
				)
		cls.vct_rendering_multiplier = FloatProperty(
				name="Rendering Multiplier",
				description="",
				min=0.001,
				default=1.00,
				)
		cls.vct_lock_sky = BoolProperty(
				name="Lock Sky Colour",
				default=True,
				)
		cls.vct_ambient_upper_hemi = FloatVectorProperty(
				name="Sky Colour",
				description="",
				min=0, max=1,
				default=(0.3, 0.50, 0.7),
				subtype='COLOR'
				)
		cls.vct_ambient_upper_hemi_power = FloatProperty(
				name="Power",
				description="Sky Colour. Power, in 10k lumens (i.e. 97 = 97.000 lumens)",
				min=0.0, max=100,
				default=60,
				)
		cls.vct_ambient_lower_hemi = FloatVectorProperty(
				name="Ground Colour",
				description="",
				min=0, max=1,
				default=(0.6, 0.45, 0.3),
				subtype='COLOR'
				)
		cls.vct_ambient_lower_hemi_power = FloatProperty(
				name="Power",
				description="Ground Colour. Power, in 10k lumens (i.e. 97 = 97.000 lumens)",
				min=0.0, max=100,
				default=28.5,
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

			self.layout.prop(dergo, "vct_normal_bias")
			self.layout.prop(dergo, "vct_thin_wall_counter")
			self.layout.prop(dergo, "vct_specular_sdf_quality")

			self.layout.prop(dergo, "vct_auto_baking_mult")
			if not dergo.vct_auto_baking_mult:
				self.layout.prop(dergo, "vct_baking_multiplier")
			self.layout.prop(dergo, "vct_rendering_multiplier")

			self.layout.prop(dergo, "vct_lock_sky")
			if not dergo.vct_lock_sky:
				row = self.layout.row()
				col = row.column()
				col.prop(dergo, "vct_ambient_upper_hemi")
				col.prop(dergo, "vct_ambient_upper_hemi_power")
				col = row.column()
				col.prop(dergo, "vct_ambient_lower_hemi")
				col.prop(dergo, "vct_ambient_lower_hemi_power")

			self.layout.label( "Debug Visualization:" )
			self.layout.prop(dergo, "vct_debug_visual", expand=True)
