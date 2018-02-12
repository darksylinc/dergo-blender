
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

enum_shadow_filtering_modes = (
    ('PCF2x2', "PCF 2x2", "Percentage Close Filtering, 2x2 kernel (low)"),
    ('PCF3x3', "PCF 3x3", "Percentage Close Filtering, 3x3 kernel (mid)"),
    ('PCF4x4', "PCF 4x4", "Percentage Close Filtering, 4x4 kernel (high)"),
    ('ESM', "ESM", "Exponential Shadow Maps (high)"),
    )

BlenderShadowFilteringModeToOgre = { 'PCF2x2' : 0, 'PCF3x3' : 1, 'PCF4x4' : 2, 'ESM' : 3 }

#
#		GLOBAL SHADOWS SETTINGS
#

class DergoWorldShadowsSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		cls.enabled = BoolProperty(
				name="enabled",
				default=True,
				)
		cls.width = IntProperty(
				name="Width",
				description="",
				min=1, max=8192,
				default=2048,
				)
		cls.height = IntProperty(
				name="Height",
				description="",
				min=1, max=8192,
				default=2048,
				)
		cls.num_lights = IntProperty(
		        name="Max Lights",
				description="",
				min=1, max=16,
				default=3,
				)
		cls.pssm = BoolProperty(
		        name="PSSM",
				description="",
				default=True,
				)
		cls.num_splits = IntProperty(
		        name="Num Splits",
				description="",
				min=2, max=4,
				default=3,
				)
		cls.filtering = EnumProperty(
		        name="filtering",
				items=enum_shadow_filtering_modes,
				default='PCF4x4',
				)
		cls.point_resolution = IntProperty(
		        name="Point Cubemap",
				description="Resolution for the cubemap used for point lights",
				min=128, max=4096,
				default=1024,
				)
		cls.pssm_lambda = FloatProperty(
		        name="Lambda",
				description="PSSM Lambda",
				min=0, max=1.0,
				default=0.95,
				)
		cls.pssm_split_padding = FloatProperty(
		        name="Split Padding",
				description="PSSM Split padding",
				min=0, max=2.0,
				default=1.0,
				)
		cls.pssm_split_blend = FloatProperty(
		        name="Split Blend",
				description="PSSM Split Blend. Smoothly blends between each split, rather than having a harsh change in quality",
				min=0, max=1.0,
				default=0.125,
				)
		cls.pssm_split_fade = FloatProperty(
		        name="Split Fade",
				description="PSSM Split Fade. Smoothly fade out the shadows past the limit",
				min=0, max=1.0,
				default=0.313,
				)
		cls.max_distance = FloatProperty(
		        name="Max Distance",
				description="Shadows Max distance. Past it, there's no more shadows",
				default=500,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.World.dergo.shadows

class Dergo_PT_world_shadow_settings(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Shadows Settings"
	bl_context = "world"

	@classmethod
	def poll(cls, context):
		return context.world and DergoButtonsPanel.poll(context)

	def draw_header(self, context):
		dergo_shadows = context.world.dergo.shadows
		self.layout.prop(dergo_shadows, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		world = context.world
		dergo_shadows = context.world.dergo.shadows

		row = layout.row()
		col = row.column()
		col.prop(dergo_shadows, "width")
		col.prop(dergo_shadows, "height")
		col.prop(dergo_shadows, "num_lights")
		col.prop(dergo_shadows, "filtering")
		col.prop(dergo_shadows, "point_resolution")
		col.prop(dergo_shadows, "pssm")
		if dergo_shadows.pssm:
			col.prop(dergo_shadows, "num_splits")
			col.prop(dergo_shadows, "pssm_lambda")
			col.prop(dergo_shadows, "pssm_split_padding")
			col.prop(dergo_shadows, "pssm_split_blend")
			col.prop(dergo_shadows, "pssm_split_fade")
		col.prop(dergo_shadows, "max_distance")

class ShadowsSettings:
	@staticmethod
	def sync( dergo_world, network ):
		dergo_shadows = dergo_world.shadows
		network.sendData( FromClient.ShadowsSettings, struct.pack( '=BHH4BH5f',\
		        dergo_shadows.enabled, dergo_shadows.width, dergo_shadows.height,
				dergo_shadows.num_lights, dergo_shadows.pssm, dergo_shadows.num_splits,
				BlenderShadowFilteringModeToOgre[dergo_shadows.filtering],
				dergo_shadows.point_resolution, dergo_shadows.pssm_lambda,
				dergo_shadows.pssm_split_padding, dergo_shadows.pssm_split_blend,
				dergo_shadows.pssm_split_fade, dergo_shadows.max_distance ) )
