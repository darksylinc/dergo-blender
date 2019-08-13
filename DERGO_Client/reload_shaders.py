import bpy

from . import engine
from .network import  *

class HlmsReloadOperator(bpy.types.Operator):
	"""Reloads Hlms Dergo shaders"""
	bl_idname = "object.hlms_reload"
	bl_label = "Reload Hlms Dergo Shaders"

	@classmethod
	def poll(cls, context):
		return context.scene.render.engine == "DERGO3D"

	def execute(self, context):
		engine.dergo.network.sendData( FromClient.ReloadShaders, None )
		return {'FINISHED'}


def register():
	bpy.utils.register_class(HlmsReloadOperator)


def unregister():
	bpy.utils.unregister_class(HlmsReloadOperator)

