
#include "Utils/ShadowsUtils.h"

#include "Compositor/OgreCompositorManager2.h"
#include "Compositor/OgreCompositorShadowNode.h"
#include "Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h"
#include "OgreHlmsManager.h"
#include "OgreHlmsPbs.h"

namespace DERGO
{
	static const Ogre::uint32 c_shadowMapNodeBit = 1u << 31u;

	void ShadowsUtils::tagAllNodesUsingShadowNodes( Ogre::CompositorManager2 *compositorManager )
	{
		using namespace Ogre;

		const CompositorManager2::CompositorNodeDefMap &nodeDefMap =
				compositorManager->getNodeDefinitions();

		CompositorManager2::CompositorNodeDefMap::const_iterator itor = nodeDefMap.begin();
		CompositorManager2::CompositorNodeDefMap::const_iterator end  = nodeDefMap.end();

		while( itor != end )
		{
			CompositorNodeDef *nodeDef = itor->second;

			const size_t numTargetPasses = nodeDef->getNumTargetPasses();

			for( size_t i=0; i<numTargetPasses; ++i )
			{
				CompositorTargetDef *targetDef = nodeDef->getTargetPass( i );
				const CompositorPassDefVec passDefs = targetDef->getCompositorPasses();

				CompositorPassDefVec::const_iterator itPassDef = passDefs.begin();
				CompositorPassDefVec::const_iterator enPassDef = passDefs.end();

				while( itPassDef != enPassDef )
				{
					CompositorPassDef *passDef = *itPassDef;
					if( passDef->getType() == PASS_SCENE )
					{
						assert( dynamic_cast<CompositorPassSceneDef*>( passDef ) );
						CompositorPassSceneDef *passSceneDef =
								static_cast<CompositorPassSceneDef*>( passDef );
						if( passSceneDef->mShadowNode != IdString() )
							passSceneDef->mIdentifier |= c_shadowMapNodeBit;
					}
					++itPassDef;
				}
			}
			++itor;
		}
	}
	//-------------------------------------------------------------------------
	void ShadowsUtils::setAllPassSceneToShadowNode( Ogre::CompositorManager2 *compositorManager,
													Ogre::IdString shadowNodeName )
	{
		using namespace Ogre;

		const CompositorManager2::CompositorNodeDefMap &nodeDefMap =
				compositorManager->getNodeDefinitions();

		CompositorManager2::CompositorNodeDefMap::const_iterator itor = nodeDefMap.begin();
		CompositorManager2::CompositorNodeDefMap::const_iterator end  = nodeDefMap.end();

		while( itor != end )
		{
			CompositorNodeDef *nodeDef = itor->second;

			const size_t numTargetPasses = nodeDef->getNumTargetPasses();

			for( size_t i=0; i<numTargetPasses; ++i )
			{
				CompositorTargetDef *targetDef = nodeDef->getTargetPass( i );
				const CompositorPassDefVec passDefs = targetDef->getCompositorPasses();

				CompositorPassDefVec::const_iterator itPassDef = passDefs.begin();
				CompositorPassDefVec::const_iterator enPassDef = passDefs.end();

				while( itPassDef != enPassDef )
				{
					CompositorPassDef *passDef = *itPassDef;
					if( passDef->getType() == PASS_SCENE )
					{
						assert( dynamic_cast<CompositorPassSceneDef*>( passDef ) );
						CompositorPassSceneDef *passSceneDef =
								static_cast<CompositorPassSceneDef*>( passDef );
						if( passSceneDef->mIdentifier & c_shadowMapNodeBit )
							passSceneDef->mShadowNode = shadowNodeName;
					}
					++itPassDef;
				}
			}
			++itor;
		}
	}
	//-------------------------------------------------------------------------
	void ShadowsUtils::applyShadowsSettings( const Settings &shadowSettings,
											 Ogre::CompositorManager2 *compositorManager,
											 Ogre::RenderSystem *renderSystem,
											 Ogre::SceneManager *sceneManager,
											 Ogre::HlmsManager *hlmsManager,
											 Ogre::HlmsPbs *hlmsPbs )
	{
		double sqrtNumShadowmaps = shadowSettings.numLights;
		if( shadowSettings.usePssm )
			sqrtNumShadowmaps += shadowSettings.numSplits - 1u;
		sqrtNumShadowmaps = sqrt( sqrtNumShadowmaps );
		Ogre::uint32 xNumShadowmaps = floor( sqrtNumShadowmaps );
		Ogre::uint32 yNumShadowmaps = ceil( sqrtNumShadowmaps );

		if( xNumShadowmaps * yNumShadowmaps < shadowSettings.numLights )
			xNumShadowmaps = yNumShadowmaps;

		Ogre::uint32 currentShadowmap = 0;

		Ogre::ShadowNodeHelper::ShadowParamVec shadowParams;
		Ogre::ShadowNodeHelper::ShadowParam shadowParam;
		memset( &shadowParam, 0, sizeof(shadowParam) );
		shadowParam.technique = shadowSettings.usePssm ? Ogre::SHADOWMAP_PSSM : Ogre::SHADOWMAP_FOCUSED;
		shadowParam.numPssmSplits = shadowSettings.numSplits;
		for( size_t i=0; i<4u; ++i )
		{
			shadowParam.resolution[i].x = shadowSettings.width;
			shadowParam.resolution[i].y = shadowSettings.height;
		}
		for( size_t i=0; i<shadowSettings.numSplits; ++i )
		{
			shadowParam.atlasStart[i].x = (currentShadowmap % xNumShadowmaps) * shadowSettings.width;
			shadowParam.atlasStart[i].y = (currentShadowmap / xNumShadowmaps) * shadowSettings.height;
			++currentShadowmap;
		}
		if( shadowSettings.usePssm )
			shadowParam.addLightType( Ogre::Light::LT_DIRECTIONAL );
		else
		{
			shadowParam.addLightType( Ogre::Light::LT_DIRECTIONAL );
			shadowParam.addLightType( Ogre::Light::LT_POINT );
			shadowParam.addLightType( Ogre::Light::LT_SPOTLIGHT );
		}
		shadowParams.push_back( shadowParam );

		shadowParam.technique = Ogre::SHADOWMAP_FOCUSED;
		shadowParam.numPssmSplits = 1u;
		shadowParam.addLightType( Ogre::Light::LT_DIRECTIONAL );
		shadowParam.addLightType( Ogre::Light::LT_POINT );
		shadowParam.addLightType( Ogre::Light::LT_SPOTLIGHT );
		for( size_t i=1u; i<shadowSettings.numLights; ++i )
		{
			shadowParam.atlasStart[0].x = (currentShadowmap % xNumShadowmaps) * shadowSettings.width;
			shadowParam.atlasStart[0].y = (currentShadowmap / xNumShadowmaps) * shadowSettings.height;
			shadowParams.push_back( shadowParam );
			++currentShadowmap;
		}

		if( compositorManager->hasShadowNodeDefinition( "ShadowMapDebuggingShadowNode" ) )
			compositorManager->removeShadowNodeDefinition( "ShadowMapDebuggingShadowNode" );

		if( !shadowSettings.enabled )
			setAllPassSceneToShadowNode( compositorManager, Ogre::IdString() );
		else
		{
			setAllPassSceneToShadowNode( compositorManager, "ShadowMapDebuggingShadowNode" );

			const bool useEsm = shadowSettings.filtering == Ogre::HlmsPbs::ExponentialShadowMaps;
			Ogre::ShadowNodeHelper::createShadowNodeWithSettings( compositorManager,
																  renderSystem->getCapabilities(),
																  "ShadowMapDebuggingShadowNode",
																  shadowParams,
																  useEsm, shadowSettings.pointRes,
																  shadowSettings.pssmLambda,
																  shadowSettings.pssmSplitPadding,
																  shadowSettings.pssmSplitBlend,
																  shadowSettings.pssmSplitFade );

			if( useEsm == hlmsManager->getShadowMappingUseBackFaces() )
				hlmsManager->setShadowMappingUseBackFaces( !useEsm );

			sceneManager->setShadowDirectionalLightExtrusionDistance( shadowSettings.maxDistance );
			sceneManager->setShadowFarDistance( shadowSettings.maxDistance );

			hlmsPbs->setShadowSettings( static_cast<Ogre::HlmsPbs::ShadowFilter>(
											shadowSettings.filtering ) );
		}
    }
    //-----------------------------------------------------------------------------------
}
