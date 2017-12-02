
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

#
#		GLOBAL INSTANT RADIOSITY SETTINGS
#

class DergoWorldPccSettings(bpy.types.PropertyGroup):
	@classmethod
	def register(cls):
		cls.enabled = BoolProperty(
				name="enabled",
				default=False,
				)
		cls.width = IntProperty(
				name="Width",
				description="",
				min=1, max=8192,
				default=1024,
				)
		cls.height = IntProperty(
				name="Height",
				description="",
				min=0, max=8192,
				default=1024,
				)

	@classmethod
	def unregister(cls):
		del bpy.types.World.dergo.pcc

class Dergo_PT_world_pcc(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Parallax Correctec Cubemaps"
	bl_context = "world"

	@classmethod
	def poll(cls, context):
		return context.world and DergoButtonsPanel.poll(context)

	def draw_header(self, context):
		dergo_pcc = context.world.dergo.pcc
		self.layout.prop(dergo_pcc, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		world = context.world
		dergo_pcc = context.world.dergo.pcc

		row = layout.row()
		col = row.column()
		col.prop(dergo_pcc, "width")
		col.prop(dergo_pcc, "height")

class ParallaxCorrectedCubemaps:
	@staticmethod
	def sync( dergo_world, network ):
		dergo_pcc = dergo_world.pcc
		network.sendData( FromClient.ParallaxCorrectedCubemaps, struct.pack( '=BHH',\
				dergo_pcc.enabled, dergo_pcc.width, dergo_pcc.height ) )

#
#		PCC PROBE
#
class DergoObjectParallaxCorrectedCubemaps:
	@staticmethod
	def registerExtraProperties(cls):
		cls.pcc_is_probe = BoolProperty(
				name="Use as PCC probe",
				default=False,
				)
		cls.pcc_inner_region = FloatVectorProperty(
				name="Inner Region",
				description="It indicates a % of the OBB's size that will have smooth. interpolation with other probes. When region = 1.0; stepping outside the OBB's results in a lighting 'pop'. Smaller values = smoother transitions, but at the cost of quality & precision while inside the OBB (as results get mixed up with other probes')",
				min=0, max=1,
				default=(0.98, 0.98, 0.98),
				subtype='XYZ',
				)
		cls.pcc_camera_pos = StringProperty(
				name="Camera position",
				description="The pivot of the selected object will act as camera position. Shold be inside the probe"
				)

class Dergo_PT_empty_pcc(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Parallax Correctec Cubemaps"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		return (context.object and context.object.type == 'EMPTY' and\
				context.object.empty_draw_type == 'CUBE' and DergoButtonsPanel.poll(context))

	def draw(self, context):
		dergo = context.object.dergo
		self.layout.prop(dergo, "pcc_is_probe")
		self.layout.prop(dergo, "pcc_inner_region")
		self.layout.prop_search(dergo, "pcc_camera_pos", context.scene, "objects")
		if dergo.pcc_camera_pos in context.scene.objects:
			cameraPos = context.scene.objects[dergo.pcc_camera_pos].location
			loc, rot, halfSize = context.object.matrix_world.decompose()
			halfSize *= context.object.empty_draw_size
			cameraPos = cameraPos - loc
			rot.invert()
			cameraPos = rot * cameraPos
			if	abs( cameraPos.x ) > halfSize.x or\
				abs( cameraPos.y ) > halfSize.y or\
				abs( cameraPos.z ) > halfSize.z:
					self.layout.label("Warning: Camera pos is outside probe")

class Dergo_PT_empty_linked_empty(DergoButtonsPanel, bpy.types.Panel):
	bl_label = "Linked Area"
	bl_context = "data"

	@classmethod
	def poll(cls, context):
		dergo = context.object.dergo
		return (context.object and context.object.type == 'EMPTY' and\
				context.object.empty_draw_type == 'CUBE' and DergoButtonsPanel.poll(context) and\
				(dergo.pcc_is_probe or dergo.ir_is_area_of_interest))

	def draw(self, context):
		dergo = context.object.dergo
		self.layout.prop_search(dergo, "linked_area", context.scene, "objects")
