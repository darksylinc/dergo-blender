
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

class DergoWorldInstantRadiositySettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		cls.enabled = BoolProperty(
				name="enabled",
				default=False,
				)
		cls.num_rays = IntProperty(
				name="Num Rays",
				description="More usually results in more accuracy",
				min=1, max=50000,
				default=128,
				)
		cls.num_ray_bounces = IntProperty(
				name="Num Ray Bounces",
				description="Increases the total number of rays (more than num_rays)",
				min=0, max=100,
				default=1,
				)
		cls.surviving_ray_fraction = FloatProperty(
				name="Surviving Ray Fraction",
				description="How many rays that fired in the previous bounce should survive for a next round of bounces",
				min=0, max=1,
				default=0.5,
				)
		cls.cell_size = FloatProperty(
				name="Cell Size",
				description="How we cluster multiple VPLs into one averaged VPL. Small values generate more VPLs, reducing performance but improving quality",
				min=0.01,
				default=3,
				)
		cls.num_spread_iterations = IntProperty(
				name="Num Spread Iterations",
				description="",
				min=0, max=255,
				default=1,
				)
		cls.spread_threshold = FloatProperty(
				name="Spread Threshold",
				description="",
				default=0.0004,
				)
		cls.bias = FloatProperty(
				name="Bias",
				description="Low bias can help with light bleeding at the cost of lower accuracy",
				min=0, max=1,
				default=0.982,
				)
		cls.vpl_max_range = FloatProperty(
				name="Max Range",
				description="How big each VPL should be. Larger ranges leak light more but also are more accurate in the sections they lit correctly, but they are also get more expensive",
				min=0,
				default=8,
				)
		cls.vpl_attenuation = FloatVectorProperty(
				name="Attenuation",
				description="Const / Linear / Quad attenuation coefficients for each VPL",
				min=0,
				default=(0.5, 0.5, 0.0),
				subtype='XYZ',
				)
		cls.vpl_threshold = FloatProperty(
				name="Threshold",
				description="If all three components of the diffuse colour of a VPL light is below this threshold the VPL is removed (useful for improving performance for VPLs that barely contribute to the scene)",
				min=0,
				default=0.0005,
				)
		cls.vpl_power_boost = FloatProperty(
				name="Power Boost",
				description="Tweaks how strong VPL lights should be",
				min=0,
				default=1.4,
				)
		cls.vpl_use_intensity_for_max_range = BoolProperty(
				name="Use Intensity for max range",
				description="When true, Intensity Range Multiplier will be used and each VPL will have a dynamic max range (can't exceed Max Range though), based on its intensity (smaller VPLs = shorter ranges, powerful VPLs = larger ranges)",
				default=True,
				)
		cls.vpl_intensity_range_multiplier = FloatProperty(
				name="Intensity Range Multiplier",
				description="",
				min=0,
				default=100.0,
				)
		cls.debug_vpl = BoolProperty(
				name="Debug VPL",
				description="Visualizes Debug VPL probes",
				default=False,
				)
		cls.use_irradiance_volumes = BoolProperty(
				name="Use Irradiance Volumes",
				description="Much faster algorithm if objects and lights don't change, but consumes more memory. Not really suitable for exteriors or very large scenes",
				default=False,
				)
		cls.irradiance_cell_size = FloatVectorProperty(
				name="Irradiance Cell Size",
				description="Smaller values are more accurate, but consume much more memory",
				min=0.01,
				default=(1.5, 1.5, 1.5),
				subtype='XYZ',
				)

	@classmethod
	def unregister(cls):
		del bpy.types.World.dergo.instant_radiosity

class Dergo_PT_world_instant_radiosity(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Instant Radiosity (GI)"
	bl_context = "world"

	@classmethod
	def poll(cls, context):
		return context.world and DergoButtonsPanel.poll(context)

	def draw_header(self, context):
		dergo_ir = context.world.dergo.instant_radiosity
		self.layout.prop(dergo_ir, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		world = context.world
		dergo_ir = context.world.dergo.instant_radiosity

		row = layout.row()
		col = row.column()
		col.prop(dergo_ir, "num_rays")
		col.prop(dergo_ir, "num_ray_bounces")
		col.prop(dergo_ir, "surviving_ray_fraction")
		col.prop(dergo_ir, "cell_size")
		col.prop(dergo_ir, "num_spread_iterations")
		col.prop(dergo_ir, "spread_threshold")
		col.prop(dergo_ir, "bias")
		layout.separator()
		row = layout.row()
		col = row.box()
		col.label( "VPLs (Virtual Point Lights)" )
		col.prop(dergo_ir, "vpl_max_range")
		col.prop(dergo_ir, "vpl_attenuation")
		col.prop(dergo_ir, "vpl_threshold")
		col.prop(dergo_ir, "vpl_power_boost")
		col.prop(dergo_ir, "vpl_use_intensity_for_max_range")
		if dergo_ir.vpl_use_intensity_for_max_range:
			col.prop(dergo_ir, "vpl_intensity_range_multiplier")
		col.prop(dergo_ir, "debug_vpl")
		layout.prop(dergo_ir, "use_irradiance_volumes")
		layout.prop(dergo_ir, "irradiance_cell_size")

class InstantRadiosity:
	@staticmethod
	def sync( dergo_world, network ):
		dergo_ir = dergo_world.instant_radiosity
		network.sendData( FromClient.InstantRadiosity, struct.pack( '=BHB2fB8fBfBB3f',\
				dergo_ir.enabled,\
				dergo_ir.num_rays, dergo_ir.num_ray_bounces, dergo_ir.surviving_ray_fraction, dergo_ir.cell_size,\
				dergo_ir.num_spread_iterations, dergo_ir.spread_threshold, dergo_ir.bias,\
				dergo_ir.vpl_max_range,\
				dergo_ir.vpl_attenuation[0], dergo_ir.vpl_attenuation[1], dergo_ir.vpl_attenuation[2],\
				dergo_ir.vpl_threshold, dergo_ir.vpl_power_boost,\
				dergo_ir.vpl_use_intensity_for_max_range, dergo_ir.vpl_intensity_range_multiplier,\
				dergo_ir.debug_vpl, dergo_ir.use_irradiance_volumes,\
				dergo_ir.irradiance_cell_size[0], dergo_ir.irradiance_cell_size[1], dergo_ir.irradiance_cell_size[2] ) )
