import json
import bpy

class MaterialImporter:
	@staticmethod
	def getValueFrom( datablock, key, defaultValue=None ):
		retVal = defaultValue
		try:
			retVal = datablock[key]
		except:
			pass
		return retVal

	@staticmethod
	def getTexValueFrom( datablock, texTypeName, defaultValue=[1, 1, 1, 1], valueName='value' ):
		retVal = defaultValue
		try:
			texTypeBlock = datablock[texTypeName]
			retVal = texTypeBlock[valueName]
		except:
			pass
		return retVal

	@staticmethod
	def copyColour( dst, src ):
		minSize = min( len( dst ), len( src ) )
		for i in range(minSize):
			dst[i] = src[i]

	@staticmethod
	def importFromFile( filepath ):
		materialFile = open( filepath, 'r' )

		materialData = json.loads( materialFile.read() )

		contextOverride = bpy.context.copy()

		if 'pbs' in materialData:
			for datablockName, datablockData in materialData['pbs'].items():
				# Blender has a limit on names, so we must truncate
				datablockName = datablockName[:63]

				# Retrieve values
				workflow = MaterialImporter.getValueFrom( datablockData, 'workflow', 'metallic' )
				diffuseValue = MaterialImporter.getTexValueFrom( datablockData, 'diffuse' )
				specularValue = MaterialImporter.getTexValueFrom( datablockData, 'specular' )
				metalnessValue = MaterialImporter.getTexValueFrom( datablockData, 'metalness', 1 )
				roughness = MaterialImporter.getTexValueFrom( datablockData, 'roughness', 1 )

				# Create Blender material and copy these values into them
				if not datablockName in bpy.data.materials:
					mat = bpy.data.materials.new( name=datablockName )
					# Blender has a limit on names, so we need to take
					# that into account in case it got truncated
					datablockName = mat.name
				else:
					mat = bpy.data.materials[datablockName]

				contextOverride['material'] = mat
				bpy.ops.material.dergo_fix_material( contextOverride )

				dmat = mat.dergo
				MaterialImporter.copyColour( mat.diffuse_color, diffuseValue )
				MaterialImporter.copyColour( mat.specular_color, specularValue )
				dmat.roughness = roughness
				if workflow == 'metallic':
					dmat.workflow = 'METALLIC'
					dmat.metallic = metalnessValue
