import bpy

# ImportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ImportHelper
from bpy.props import BoolProperty, StringProperty
from bpy.types import Operator

from . import engine
from .imports.import_materials import MaterialImporter

class ImportMaterial(Operator, ImportHelper):
	"""This appears in the tooltip of the operator and in the generated docs"""
	bl_idname = "dergo.import_materials_from_file"  # important since its how bpy.ops.dergo.import_materials_from_file is constructed
	bl_label = "Import Ogre Materials from JSON file (via DERGO)"

	# ImportHelper mixin class uses this
	filename_ext = ".material.json"

	filter_glob = StringProperty(
			default="*.material.json.scenefolder",
			options={'HIDDEN'},
			maxlen=255,  # Max internal buffer length, longer would be clamped.
			)

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		MaterialImporter.importFromFile( self.filepath )
		return {'FINISHED'}


# Only needed if you want to add into a dynamic menu
def menu_material_import(self, context):
	self.layout.operator( ImportMaterial.bl_idname, text="Import Ogre Materials from JSON file (via DERGO)" )


def register():
	bpy.utils.register_class( ImportMaterial )
	bpy.types.INFO_MT_file_import.append(menu_material_import)


def unregister():
	bpy.utils.unregister_class( ImportMaterial )
	bpy.types.INFO_MT_file_import.remove(menu_material_import)


if __name__ == "__main__":
	register()

	# test call
#	bpy.ops.export_test.some_data('INVOKE_DEFAULT')
