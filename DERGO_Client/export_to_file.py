import bpy

# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import BoolProperty, StringProperty
from bpy.types import Operator

from . import engine
from .network import  *

class ExportSomeData(Operator, ExportHelper):
	"""This appears in the tooltip of the operator and in the generated docs"""
	bl_idname = "dergo.export_to_file"  # important since its how bpy.ops.dergo.export_to_file is constructed
	bl_label = "Export Ogre Scene (via DERGO)"

	# ExportHelper mixin class uses this
	filename_ext = ".scenefolder"

	filter_glob = StringProperty(
	        default="*.scenefolder",
			options={'HIDDEN'},
			maxlen=255,  # Max internal buffer length, longer would be clamped.
			)

	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	meshes = BoolProperty(
	        name="Meshes",
			description="Export meshes (*.mesh files)",
			default=True,
			)
	objects = BoolProperty(
	        name="Objects",
			description="Export scene objects (multiple instances of meshes, their position and orientation, etc)",
			default=True,
			)
	lights = BoolProperty(
	        name="Lights",
			description="Export lights",
			default=True,
			)
	materials = BoolProperty(
	        name="Materials",
			description="Export the materials used as .material.json",
			default=True,
			)
	textures = BoolProperty(
	        name="Textures",
			description="Copy the textures to the scene folder",
			default=True,
			)
	world_settings = BoolProperty(
	        name="World Settings",
			description="Save the world settings such as ambient light",
			default=True,
			)
	world_settings = BoolProperty(
	        name="World Settings",
			description="Save the world settings such as ambient light",
			default=True,
			)
	instant_radiosity = BoolProperty(
	        name="Instant Radiosity",
			description="Save the Instant Radiosity settings",
			default=True,
			)

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		asUtfBytes = self.filepath.encode('utf-8')
		stringLength = len( asUtfBytes )
		dataToSend = bytearray( struct.pack( '=I', len( asUtfBytes ) ) )
		dataToSend.extend( asUtfBytes )

		dataToSend.extend( struct.pack( '=7B', \
		self.objects, self.lights, self.materials, self.textures, self.meshes, \
		self.world_settings, self.instant_radiosity ) )
		engine.dergo.network.sendData( FromClient.ExportToFile, dataToSend )
		return {'FINISHED'}


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
	self.layout.operator( ExportSomeData.bl_idname, text="Export Ogre Scene (via DERGO)" )


def register():
	bpy.utils.register_class( ExportSomeData )
	bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
	bpy.utils.unregister_class( ExportSomeData )
	bpy.types.INFO_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
	register()

	# test call
#	bpy.ops.export_test.some_data('INVOKE_DEFAULT')
