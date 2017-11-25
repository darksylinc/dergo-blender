
import bpy

from . import engine
from .network import  *
from .ui import DergoButtonsPanel

class Dergo_PT_world(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "World"
	bl_context = "world"

	@classmethod
	def poll(cls, context):
		return context.world and DergoButtonsPanel.poll(context)

	def draw(self, context):
		layout = self.layout
		world = context.world
		dworld = context.world.dergo

		row = layout.row()
		col = row.column()
		col.prop(dworld, "sky")
		col.prop(dworld, "sky_power")
		col = row.column()
		col.prop(dworld, "ambient_upper_hemi")
		col.prop(dworld, "ambient_upper_hemi_power")
		#col.active = world.use_sky_blend
		col = row.column()
		col.prop(dworld, "ambient_lower_hemi")
		col.prop(dworld, "ambient_lower_hemi_power")

		row = layout.row()
		row.prop( dworld, "ambient_hemi_dir" )

		row = layout.row()
		row.column().prop( dworld, "exposure" )
		row.column().prop( dworld, "min_auto_exposure" )
		row.column().prop( dworld, "max_auto_exposure" )

		row = layout.row()
		row.column().prop( dworld, "bloom_threshold" )
		row.column().prop( dworld, "envmap_scale" )
