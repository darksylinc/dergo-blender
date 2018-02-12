
class DergoButtonsPanel():
	bl_space_type = "PROPERTIES"
	bl_region_type = "WINDOW"
	bl_context = "render"

	@classmethod
	def poll(cls, context):
		rd = context.scene.render
		return rd.engine == 'DERGO3D'
