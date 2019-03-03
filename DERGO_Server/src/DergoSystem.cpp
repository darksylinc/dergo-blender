
#include "DergoSystem.h"
#include "VertexUtils.h"

#include "System/WindowEventListener.h"

#include "OgreRoot.h"
#include "OgreException.h"

#include "OgreWindow.h"
#include "OgreCamera.h"
#include "OgreItem.h"

#include "OgreHlmsUnlit.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsPbsDatablock.h"
#include "OgreHlmsManager.h"
#include "OgreForwardPlusBase.h"

#include "Compositor/OgreCompositorManager2.h"
#include "Compositor/OgreCompositorWorkspace.h"
#include "Compositor/OgreCompositorNode.h"
#include "Compositor/OgreCompositorNodeDef.h"
#include "Compositor/Pass/PassClear/OgreCompositorPassClearDef.h"
#include "Compositor/OgreCompositorShadowNode.h"

#include "Network/NetworkSystem.h"
#include "Network/NetworkMessage.h"
#include "Network/SmartData.h"
#include "OgreMeshManager2.h"
#include "OgreMesh2.h"
#include "OgreSubMesh2.h"
#include "Vao/OgreStagingBuffer.h"

#include "InstantRadiosity/OgreInstantRadiosity.h"
#include "OgreIrradianceVolume.h"

#include "Cubemaps/OgreParallaxCorrectedCubemap.h"

#include "OgreSceneFormatExporter.h"

#include "OgreTextureGpuManager.h"
#include "OgreWindowEventUtilities.h"

#include "OgreImage2.h"

#include "Utils/HdrUtils.h"

namespace DERGO
{
	Ogre::String toStr64( uint64_t val )
	{
		Ogre::StringStream stream;
		stream << val;

		return stream.str();
	}

	DergoSystem::BlenderItemVec::iterator DergoSystem::BlenderMesh::findItem( uint32_t itemId )
	{
		BlenderItemVec::iterator itor = items.begin();
		BlenderItemVec::iterator end  = items.end();

		while( itor != end && itor->id != itemId )
			++itor;

		return itor;
	}

	DergoSystem::DergoSystem( Ogre::ColourValue backgroundColour ) :
		GraphicsSystem( backgroundColour ),
		m_enableInstantRadiosity( false ),
		m_instantRadiosity( 0 ),
		m_irradianceVolume( 0 ),
		m_irradianceCellSize( Ogre::Vector3( 1.5f ) ),
		m_irDirty( false ),
		m_parallaxCorrectedCubemap( 0 ),
		m_windowEventListener( 0 )
	{
		m_windowEventListener = new WindowEventListener();
		mAlwaysAskForConfig = false;
	}
	//-----------------------------------------------------------------------------------
	DergoSystem::~DergoSystem()
	{
		delete m_windowEventListener;
		m_windowEventListener = 0;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::initialize()
	{
		GraphicsSystem::initialize();

		struct Presets
		{
			Ogre::uint32 width;
			Ogre::uint32 height;
			Ogre::uint32 numSlices;
			Ogre::uint32 lightsPerCell;
			float minDistance;
			float maxDistance;
		};

		const Presets c_presets[] =
		{
			{ 4, 4, 5, 96, 3.0f, 200.0f },
			{ 4, 4, 4, 96, 3.0f, 100.0f },
			{ 4, 4, 4, 64, 3.0f, 200.0f },
			{ 4, 4, 4, 32, 3.0f, 200.0f },
			{ 4, 4, 7, 64, 3.0f, 150.0f },
			{ 4, 4, 3, 128, 3.0f, 200.0f },
		};

#if OGRE_DEBUG_MODE
		const Presets &preset = c_presets[0];
		mSceneManager->setForward3D( true, preset.width, preset.height,
									preset.numSlices, preset.lightsPerCell,
									preset.minDistance, preset.maxDistance );
#else
		mSceneManager->setForwardClustered( true, 16, 8, 24, 96, 0, 5, 500 );
#endif

		mSceneManager->setVisibilityMask( 1u );

		Demo::HdrUtils::init( mRenderWindow->getMsaa() );

		//Create a default datablock to silence that pesky Log warning.
		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );
		hlmsPbs->createDatablock( "##INTERNAL## DEFAULT", "##INTERNAL## DEFAULT",
								  Ogre::HlmsMacroblock(), Ogre::HlmsBlendblock(),
								  Ogre::HlmsParamVec() );

		m_instantRadiosity = new Ogre::InstantRadiosity( mSceneManager, hlmsManager );
		m_irradianceVolume = new Ogre::IrradianceVolume( hlmsManager );

		Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
		Ogre::CompositorWorkspaceDef *workspaceDef = compositorManager->getWorkspaceDefinition(
														 "LocalCubemapsProbeWorkspace" );
		m_parallaxCorrectedCubemap = new Ogre::ParallaxCorrectedCubemap(
										 Ogre::Id::generateNewId<Ogre::ParallaxCorrectedCubemap>(),
										 mRoot, mSceneManager,
										 workspaceDef, 250, 1u << 25u );

		Ogre::ResourceGroupManager &resourceGroupManager = Ogre::ResourceGroupManager::getSingleton();
		resourceGroupManager.setLoadingListener( this );

		//Enable LTC area lights (up to 16 for now)
		hlmsPbs->setAreaLightForwardSettings( 0u, 16u );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::loadResources()
	{
		GraphicsSystem::loadResources();

		Ogre::Hlms *hlms = mRoot->getHlmsManager()->getHlms( Ogre::HLMS_PBS );
		OGRE_ASSERT_HIGH( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );
		hlmsPbs->loadLtcMatrix();
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::deinitialize()
	{
		reset();

		delete m_parallaxCorrectedCubemap;
		m_parallaxCorrectedCubemap = 0;

		delete m_instantRadiosity;
		m_instantRadiosity = 0;

		if( mWorkspace )
		{
			Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
			compositorManager->removeWorkspace( mWorkspace );
			mWorkspace = 0;
		}
		GraphicsSystem::deinitialize();
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::chooseSceneManager()
	{
		GraphicsSystem::chooseSceneManager();

		Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
		ShadowsUtils::tagAllNodesUsingShadowNodes( compositorManager );

		ShadowsUtils::Settings shadowSettings;
		memset( &shadowSettings, 0, sizeof(shadowSettings) );
		shadowSettings.enabled			= true;
		shadowSettings.width			= 2048u;
		shadowSettings.height			= 2048u;
		shadowSettings.numLights		= 3u;
		shadowSettings.usePssm			= true;
		shadowSettings.numSplits		= 3u;
		shadowSettings.filtering		= Ogre::HlmsPbs::PCF_4x4;
		shadowSettings.pointRes			= 1024u;
		shadowSettings.pssmLambda		= 0.95f;
		shadowSettings.pssmSplitPadding	= 1.0f;
		shadowSettings.pssmSplitBlend	= 0.125f;
		shadowSettings.pssmSplitFade	= 0.313;
		shadowSettings.maxDistance		= 500.0f;

		setShadowsSettings( shadowSettings );
		memcpy( &m_shadowsSettings, &shadowSettings, sizeof(shadowSettings) );
	}
	//-----------------------------------------------------------------------------------
	Ogre::CompositorWorkspace* DergoSystem::setupCompositor(void)
	{
		Ogre::CompositorWorkspace *retVal = GraphicsSystem::setupCompositor();
		retVal->setEnabled( false );
		return retVal;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncWorld( Network::SmartData &smartData )
	{
		const Ogre::Vector3 skyColour3		= smartData.read<Ogre::Vector3>();
		const float skyPower				= smartData.read<float>();
		Ogre::Vector3 upperHemiColour		= smartData.read<Ogre::Vector3>();
		const float upperHemiPower			= smartData.read<float>();
		Ogre::Vector3 lowerHemiColour		= smartData.read<Ogre::Vector3>();
		const float lowerHemiPower			= smartData.read<float>();
		const Ogre::Vector3 hemisphereDir	= smartData.read<Ogre::Vector3>();
		const float exposure				= smartData.read<float>();
		const float minAutoExposure			= smartData.read<float>();
		const float maxAutoExposure			= smartData.read<float>();
		const float bloomThreshold			= smartData.read<float>();
		const float envmapScale				= smartData.read<float>();

		Ogre::ColourValue skyColour( skyColour3.x, skyColour3.y, skyColour3.z );
		WindowMap::iterator itor = m_renderWindows.begin();
		WindowMap::iterator end  = m_renderWindows.end();

		while( itor != end )
		{
			Window &window = itor->second;
			Demo::HdrUtils::setSkyColour( skyColour, skyPower, window.workspace );
			++itor;
		}
		Demo::HdrUtils::setExposure( exposure, minAutoExposure, maxAutoExposure );
		Demo::HdrUtils::setBloomThreshold( Ogre::max( bloomThreshold - 2.0f, 0.0f ),
										   Ogre::max( bloomThreshold, 0.01f ) );
		upperHemiColour *= upperHemiPower;
		lowerHemiColour *= lowerHemiPower;
		Ogre::ColourValue upperHemi( upperHemiColour.x, upperHemiColour.y, upperHemiColour.z );
		Ogre::ColourValue lowerHemi( lowerHemiColour.x, lowerHemiColour.y, lowerHemiColour.z );
		mSceneManager->setAmbientLight( upperHemi, lowerHemi, hemisphereDir, envmapScale );

		{
			//Set sky colour to cubemap probe workspace
			Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
			Ogre::CompositorNodeDef *nodeDef =
					compositorManager->getNodeDefinitionNonConst( "LocalCubemapProbeRendererNode" );

			assert( nodeDef->getNumTargetPasses() >= 1 );

			Ogre::CompositorTargetDef *targetDef = nodeDef->getTargetPass( 0 );
			const Ogre::CompositorPassDefVec &passDefs = targetDef->getCompositorPasses();

			assert( passDefs.size() >= 1 );
			Ogre::CompositorPassDef *passDef = passDefs[0];

			//Change the definition (for new cubemap probes)
			passDef->setAllClearColours( skyColour * skyPower );

			//Change active nodes (for current cubemap probes)
			const Ogre::CubemapProbeVec &probes = m_parallaxCorrectedCubemap->getProbes();
			Ogre::CubemapProbeVec::const_iterator itor = probes.begin();
			Ogre::CubemapProbeVec::const_iterator end  = probes.end();

			while( itor != end )
			{
				Ogre::CubemapProbe *probe = *itor;
				Ogre::CompositorWorkspace *workspace = probe->getWorkspace();

				Ogre::CompositorNode *node = workspace->findNode( "LocalCubemapProbeRendererNode" );

				const Ogre::CompositorPassVec passes = node->_getPasses();
				assert( passes.size() >= 1 );
				Ogre::CompositorPass *pass = passes[0];
				Ogre::RenderPassDescriptor *renderPassDesc = pass->getRenderPassDesc();
				renderPassDesc->setClearColour( skyColour * skyPower );
				++itor;
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::rebuildInstantRadiosity()
	{
		Ogre::InstantRadiosity::AreaOfInterestVec areasOfInterest;
		areasOfInterest.reserve( m_empties.size() );

		BlenderEmptyVec::const_iterator itor = m_empties.begin();
		BlenderEmptyVec::const_iterator end  = m_empties.end();

		while( itor != end )
		{
			const BlenderEmpty &empty = *itor;

			if( empty.isAoI )
			{
				float irRadius = 0; //TODO
				Ogre::Matrix4 rotMatrix;
				rotMatrix.makeTransform( Ogre::Vector3::ZERO, Ogre::Vector3::UNIT_SCALE, empty.qRot );
				Ogre::Aabb aabb( empty.position, empty.halfSize );
				aabb.transformAffine( rotMatrix );
				Ogre::InstantRadiosity::AreaOfInterest aoi( aabb, irRadius );
				areasOfInterest.push_back( aoi );
			}

			++itor;
		}

		m_instantRadiosity->mAoI = areasOfInterest;
		m_instantRadiosity->build();
		m_irDirty = false;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::updateIrradianceVolume()
	{
		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlmsManager->getHlms( Ogre::HLMS_PBS ) ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlmsManager->getHlms(Ogre::HLMS_PBS) );

		if( !hlmsPbs->getIrradianceVolume() )
			return;

		Ogre::Vector3 volumeOrigin;
		Ogre::Real lightMaxPower;
		Ogre::uint32 numBlocksX, numBlocksY, numBlocksZ;
		m_instantRadiosity->suggestIrradianceVolumeParameters( m_irradianceCellSize,
															   volumeOrigin, lightMaxPower,
															   numBlocksX, numBlocksY, numBlocksZ );
		m_irradianceVolume->createIrradianceVolumeTexture( numBlocksX, numBlocksY, numBlocksZ );
		m_instantRadiosity->fillIrradianceVolume( m_irradianceVolume, m_irradianceCellSize,
												  volumeOrigin, lightMaxPower, false );
	}
	//-----------------------------------------------------------------------------------
	template <typename T> bool setIfChanged( T &valueToChange, const T newValue )
	{
		bool hasChanged = valueToChange != newValue;
		if( hasChanged )
			valueToChange = newValue;
		return hasChanged;
	}

	void DergoSystem::syncInstantRadiosity( Network::SmartData &smartData )
	{
		const bool enabled							= smartData.read<Ogre::uint8>() != 0;
		const size_t numRays						= smartData.read<Ogre::uint16>();
		const size_t numRayBounces					= smartData.read<Ogre::uint8>();
		const float survivingRayFraction			= smartData.read<float>();
		const float cellSize						= smartData.read<float>();
		const Ogre::uint32 numSpreadIterations		= smartData.read<Ogre::uint8>();
		const float spreadThreshold					= smartData.read<float>();
		const float bias							= smartData.read<float>();
		const float vplMaxRange						= smartData.read<float>();
		const float vplConstAtten					= smartData.read<float>();
		const float vplLinearAtten					= smartData.read<float>();
		const float vplQuadAtten					= smartData.read<float>();
		const float vplThreshold					= smartData.read<float>();
		const float vplPowerBoost					= smartData.read<float>();
		const bool vplUseIntensityForMaxRange		= smartData.read<Ogre::uint8>() != 0;
		const double vplIntensityRangeMultiplier	= smartData.read<float>();
		const bool debugVpl							= smartData.read<Ogre::uint8>() != 0;
		const bool useIrradianceVolumes				= smartData.read<Ogre::uint8>() != 0;
		const Ogre::Vector3 irradianceCellSize		= smartData.read<Ogre::Vector3>();

		bool needsRebuild = false;
		bool needsIrradianceVolumeRebuild = false;
		needsRebuild |= setIfChanged( m_instantRadiosity->mNumRays, numRays );
		needsRebuild |= setIfChanged( m_instantRadiosity->mNumRayBounces, numRayBounces );
		needsRebuild |= setIfChanged( m_instantRadiosity->mSurvivingRayFraction, survivingRayFraction );
		needsRebuild |= setIfChanged( m_instantRadiosity->mCellSize, cellSize );
		needsRebuild |= setIfChanged( m_instantRadiosity->mNumSpreadIterations, numSpreadIterations );
		needsRebuild |= setIfChanged( m_instantRadiosity->mSpreadThreshold, spreadThreshold );
		needsRebuild |= setIfChanged( m_instantRadiosity->mBias, bias );

		bool vplHasChanged = false;
		needsIrradianceVolumeRebuild |= setIfChanged( m_instantRadiosity->mVplMaxRange, vplMaxRange );
		vplHasChanged |= setIfChanged( m_instantRadiosity->mVplConstAtten, vplConstAtten );
		vplHasChanged |= setIfChanged( m_instantRadiosity->mVplLinearAtten, vplLinearAtten );
		vplHasChanged |= setIfChanged( m_instantRadiosity->mVplQuadAtten, vplQuadAtten );
		vplHasChanged |= setIfChanged( m_instantRadiosity->mVplThreshold, vplThreshold );
		needsIrradianceVolumeRebuild |= setIfChanged( m_instantRadiosity->mVplPowerBoost, vplPowerBoost );
		needsIrradianceVolumeRebuild |= setIfChanged( m_instantRadiosity->mVplUseIntensityForMaxRange,
													  vplUseIntensityForMaxRange );
		needsIrradianceVolumeRebuild |= setIfChanged( m_instantRadiosity->mVplIntensityRangeMultiplier,
													  vplIntensityRangeMultiplier );
		needsIrradianceVolumeRebuild |= setIfChanged( m_irradianceCellSize, irradianceCellSize );

		vplHasChanged |= needsIrradianceVolumeRebuild;
		needsIrradianceVolumeRebuild |= needsRebuild;

		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );

		if( !enabled )
		{
			m_instantRadiosity->clear();
			hlmsPbs->setIrradianceVolume( 0 );
			m_irradianceVolume->destroyIrradianceVolumeTexture();
			m_irradianceVolume->freeMemory();
			mSceneManager->getForwardPlus()->setEnableVpls( false );

			m_instantRadiosity->mAoI.clear();
			m_irDirty = false;
		}
		else
		{
			if( needsRebuild || !m_enableInstantRadiosity )
			{
				rebuildInstantRadiosity();
				mSceneManager->getForwardPlus()->setEnableVpls( true );
			}
			else if( vplHasChanged &&
					 m_instantRadiosity->getUseIrradianceVolume() == useIrradianceVolumes )
			{
				m_instantRadiosity->updateExistingVpls();
			}

			if( m_instantRadiosity->getUseIrradianceVolume() != useIrradianceVolumes )
			{
				if( useIrradianceVolumes )
					hlmsPbs->setIrradianceVolume( m_irradianceVolume );
				else
				{
					hlmsPbs->setIrradianceVolume( 0 );
					m_irradianceVolume->destroyIrradianceVolumeTexture();
					m_irradianceVolume->freeMemory();
				}

				m_instantRadiosity->setUseIrradianceVolume( useIrradianceVolumes );
				updateIrradianceVolume(); //Will implicitly call updateExistingVpls
			}
			else if( needsIrradianceVolumeRebuild )
			{
				updateIrradianceVolume(); //Will implicitly call updateExistingVpls
			}
		}

		if( debugVpl != m_instantRadiosity->getEnableDebugMarkers() )
			m_instantRadiosity->setEnableDebugMarkers( debugVpl );

		m_enableInstantRadiosity = enabled;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncParallaxCorrectedCubemaps( Network::SmartData &smartData )
	{
		const bool enabled			= smartData.read<Ogre::uint8>() != 0;
		const Ogre::uint32 width	= smartData.read<Ogre::uint16>();
		const Ogre::uint32 height	= smartData.read<Ogre::uint16>();

		Ogre::TextureGpu *cubemapTex = m_parallaxCorrectedCubemap->getBindTexture();

		if( m_parallaxCorrectedCubemap->getEnabled() &&
			(cubemapTex->getWidth() != width || cubemapTex->getHeight() != height) )
		{
			m_parallaxCorrectedCubemap->setEnabled( false, 0, 0, Ogre::PFG_UNKNOWN );
		}

		bool wasEnabled = m_parallaxCorrectedCubemap->getEnabled();

		m_parallaxCorrectedCubemap->setEnabled( enabled, width, height, Ogre::PFG_RGBA16_FLOAT );

		cubemapTex = m_parallaxCorrectedCubemap->getBindTexture();

		if( !wasEnabled && enabled )
		{
			//We need to reset all existing probes
			BlenderEmptyVec::const_iterator itor = m_empties.begin();
			BlenderEmptyVec::const_iterator end  = m_empties.end();

			while( itor != end )
			{
				const BlenderEmpty &empty = *itor;
				if( empty.probe )
				{
					empty.probe->setTextureParams( cubemapTex->getWidth(), cubemapTex->getHeight(),
												   false, Ogre::PFG_RGBA16_FLOAT, empty.pccIsStatic );
					if( !empty.probe->isInitialized() )
						empty.probe->initWorkspace();
				}
				++itor;
			}
		}

		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );

		if( enabled )
			hlmsPbs->setParallaxCorrectedCubemap( m_parallaxCorrectedCubemap );
		else
			hlmsPbs->setParallaxCorrectedCubemap( 0 );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncShadowsSettings( Network::SmartData &smartData )
	{
		ShadowsUtils::Settings shadowSettings;
		memset( &shadowSettings, 0, sizeof(shadowSettings) );
		shadowSettings.enabled			= smartData.read<Ogre::uint8>() != 0;
		shadowSettings.width			= smartData.read<Ogre::uint16>();
		shadowSettings.height			= smartData.read<Ogre::uint16>();
		shadowSettings.numLights		= smartData.read<Ogre::uint8>();
		shadowSettings.usePssm			= smartData.read<Ogre::uint8>() != 0;
		shadowSettings.numSplits		= smartData.read<Ogre::uint8>();
		shadowSettings.filtering		= smartData.read<Ogre::uint8>();
		shadowSettings.pointRes			= smartData.read<Ogre::uint16>();
		shadowSettings.pssmLambda		= smartData.read<float>();
		shadowSettings.pssmSplitPadding	= smartData.read<float>();
		shadowSettings.pssmSplitBlend	= smartData.read<float>();
		shadowSettings.pssmSplitFade	= smartData.read<float>();
		shadowSettings.maxDistance		= smartData.read<float>();

		if( memcmp( &shadowSettings, &m_shadowsSettings, sizeof(shadowSettings) ) == 0 )
			return; //Settings didn't change. We're done.

		setShadowsSettings( shadowSettings );
		memcpy( &m_shadowsSettings, &shadowSettings, sizeof(shadowSettings) );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::setShadowsSettings( const ShadowsUtils::Settings &shadowSettings )
	{
		Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();

		{
			//We need to reset all existing probes
			BlenderEmptyVec::const_iterator itor = m_empties.begin();
			BlenderEmptyVec::const_iterator end  = m_empties.end();

			while( itor != end )
			{
				const BlenderEmpty &empty = *itor;
				if( empty.probe )
					empty.probe->destroyWorkspace();
				++itor;
			}
		}
		{
			WindowMap::iterator itor = m_renderWindows.begin();
			WindowMap::iterator end  = m_renderWindows.end();

			while( itor != end )
			{
				Window &window = itor->second;
				compositorManager->removeWorkspace( window.workspace );
				window.workspace = 0;
				++itor;
			}
		}

		const bool workspaceWasActive = mWorkspace != 0;
		if( workspaceWasActive )
			stopCompositor();

		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );

		ShadowsUtils::applyShadowsSettings( shadowSettings, compositorManager, renderSystem,
											mSceneManager, hlmsManager, hlmsPbs );

		if( workspaceWasActive )
			restartCompositor();

		{
			WindowMap::iterator itor = m_renderWindows.begin();
			WindowMap::iterator end  = m_renderWindows.end();

			while( itor != end )
			{
				Window &window = itor->second;
				window.workspace = compositorManager->addWorkspace( mSceneManager,
																	window.renderWindow->getTexture(),
																	window.camera,
																	"DergoHdrWorkspace", true );
				++itor;
			}
		}
		{
			//We need to reset all existing probes
			BlenderEmptyVec::const_iterator itor = m_empties.begin();
			BlenderEmptyVec::const_iterator end  = m_empties.end();

			while( itor != end )
			{
				const BlenderEmpty &empty = *itor;
				if( empty.probe )
					empty.probe->initWorkspace();
				++itor;
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::destroyMeshVaos( Ogre::Mesh *mesh )
	{
		Ogre::VertexBufferPacked *vertexBuffer = 0;

		Ogre::VaoManager *vaoManager = mRoot->getRenderSystem()->getVaoManager();

		const Ogre::Mesh::SubMeshVec &subMeshes = mesh->getSubMeshes();
		Ogre::Mesh::SubMeshVec::const_iterator itor = subMeshes.begin();
		Ogre::Mesh::SubMeshVec::const_iterator end  = subMeshes.end();

		while( itor != end )
		{
			Ogre::SubMesh *subMesh = *itor;

			assert( subMesh->mVao[0].empty() || subMesh->mVao[1].empty() ||
					subMesh->mVao[0][0] == subMesh->mVao[1][0] );
			subMesh->mVao[1].clear();

			Ogre::VertexArrayObjectArray::const_iterator it = subMesh->mVao[0].begin();
			Ogre::VertexArrayObjectArray::const_iterator en = subMesh->mVao[0].end();

			while( it != en )
			{
				Ogre::VertexArrayObject *vao = *it;

				assert( vao->getVertexBuffers().size() == 1 );
				assert( vao->getIndexBuffer() );

				vertexBuffer = vao->getVertexBuffers()[0];
				vaoManager->destroyIndexBuffer( vao->getIndexBuffer() );
				vaoManager->destroyVertexArrayObject( vao );
				++it;
			}

			subMesh->mVao[0].clear();

			++itor;
		}

		if( vertexBuffer )
			vaoManager->destroyVertexBuffer( vertexBuffer );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncMesh( Network::SmartData &smartData )
	{
		uint32_t meshId = smartData.read<uint32_t>();
		Ogre::String meshName = smartData.getString();

		const Ogre::uint32 numFaces			= smartData.read<Ogre::uint32>();
		const Ogre::uint32 numRawVertices	= smartData.read<Ogre::uint32>();
		const bool hasColour				= smartData.read<Ogre::uint8>() != 0;
		const Ogre::uint8 numUVs			= smartData.read<Ogre::uint8>();
		uint8_t tangentUVSource				= smartData.read<uint8_t>();

		Ogre::VertexElement2VecVec vertexElements( 1 );
		vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );
		vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_NORMAL ) );
		if( hasColour )
		{
			vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_UBYTE4_NORM,
															   Ogre::VES_DIFFUSE ) );
		}
		for( Ogre::uint8 i=0; i<numUVs; ++i )
		{
			vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT2,
															   Ogre::VES_TEXTURE_COORDINATES ) );
		}

		bool hasNormalMapping = numUVs > 0 && tangentUVSource != 255;
		GenerateTangentsTask *tangentTask = 0;
		if( hasNormalMapping )
		{
			vertexElements[0].push_back( Ogre::VertexElement2( Ogre::VET_FLOAT4, Ogre::VES_TANGENT ) );

			tangentUVSource = std::min<uint8_t>( numUVs - 1u, tangentUVSource );
		}

		//Read face data
		std::vector<BlenderFace> blenderFaces;
		std::vector<BlenderFaceColour> blenderFaceColour;
		std::vector<BlenderFaceUv> blenderFaceUv;
		std::vector<BlenderRawVertex> blenderRawVertices;
		blenderFaces.resize( numFaces );
		if( hasColour )
			blenderFaceColour.resize( numFaces );
		blenderFaceUv.resize( numFaces * numUVs );
		blenderRawVertices.resize( numRawVertices );

		for( uint32_t i=0; i<numFaces; ++i )
		{
			smartData.read( reinterpret_cast<uint8_t*>(&blenderFaces[i]), c_sizeOfBlenderFace );
		}

		if( !blenderFaceColour.empty() )
		{
			smartData.read( reinterpret_cast<uint8_t*>(&blenderFaceColour[0]),
							sizeof(BlenderFaceColour) * numFaces );
		}
		if( !blenderFaceUv.empty() )
		{
			smartData.read( reinterpret_cast<uint8_t*>(&blenderFaceUv[0]),
							sizeof(BlenderFaceUv) * numFaces * numUVs );
		}
		if( !blenderRawVertices.empty() )
		{
			smartData.read( reinterpret_cast<uint8_t*>(&blenderRawVertices[0]),
							sizeof(BlenderRawVertex) * numRawVertices );
		}

		// A face can either be 3 vertices (1 tri) or 6 vertices (2 tris).
		//Go through the faces and calculate the actual number of vertices
		//needed, and offsets for each thread to start from.
		uint32_t numVertices = 0;
		std::vector<uint32_t> vertexStartThreadIdx;

		vertexStartThreadIdx.resize( mSceneManager->getNumWorkerThreads() + 1, 0 );

		{
			const size_t numThreads = mSceneManager->getNumWorkerThreads();
			const uint32_t numFacesPerThread = Ogre::alignToNextMultiple( numFaces,
																		  numThreads ) / numThreads;


			std::vector<BlenderFace>::const_iterator itor = blenderFaces.begin();
			std::vector<BlenderFace>::const_iterator end  = blenderFaces.end();
			while( itor != end )
			{
				if( itor->numIndicesInFace == 4 )
					numVertices += 6;
				else
					numVertices += 3;

				const size_t threadIdx = ( itor - blenderFaces.begin() ) / numFacesPerThread + 1;
				vertexStartThreadIdx[threadIdx] = numVertices;

				++itor;
			}
		}

		//Deindex vertex data
		const Ogre::uint32 bytesPerVertex = Ogre::VaoManager::calculateVertexSize( vertexElements[0] );
		const uint32_t bytesPerVertexWithoutTangent =
				bytesPerVertex - (hasNormalMapping ? (sizeof(float) * 4) : 0);

		unsigned char *vertexData = reinterpret_cast<unsigned char*>( OGRE_MALLOC_SIMD(
																		  numVertices * bytesPerVertex,
																		  Ogre::MEMCATEGORY_GEOMETRY ) );

		Ogre::FastArray<uint16_t> materialIds;
		materialIds.resize( numVertices / 3 );

		//We need this to free the pointer in case we raise an exception too early.
		Ogre::FreeOnDestructor dataPtrContainer( vertexData );

		DeindexTask deindexTask( vertexData, bytesPerVertex, numVertices,
								 numUVs, vertexStartThreadIdx, &blenderFaces,
								 &blenderFaceColour, &blenderFaceUv, &blenderRawVertices,
								 &materialIds );

		mSceneManager->executeUserScalableTask( &deindexTask, true );

		//Remove duplicates (we now have 3 vertices per triangle!)
		Ogre::FastArray<uint32_t> vertexConversionLut;
		size_t optimizedNumVertices = 0;

		Ogre::Aabb aabb( Ogre::Aabb::BOX_NULL );

		if( numVertices != 0 )
		{
			if( numVertices < 40000 )
			{
				//Optimize memory and GPU performance.
				optimizedNumVertices = VertexUtils::shrinkVertexBuffer( vertexData, vertexConversionLut,
																		bytesPerVertex, numVertices );

				if( hasNormalMapping )
				{
					tangentTask = new GenerateTangentsTask( vertexData, bytesPerVertex,
															optimizedNumVertices, 0, sizeof(float)*3,
															bytesPerVertexWithoutTangent,
															sizeof(float)*3*2 +
																sizeof(float) * 2 * tangentUVSource,
															vertexConversionLut.begin(),
															vertexConversionLut.size(),
															mSceneManager->getNumWorkerThreads() );
					mSceneManager->executeUserScalableTask( tangentTask, false );
				}
			}
			else
			{
				if( hasNormalMapping )
				{
					tangentTask = new GenerateTangentsTask( vertexData, bytesPerVertex, numVertices, 0,
															sizeof(float)*3,
															bytesPerVertexWithoutTangent,
															sizeof(float)*3*2 +
																sizeof(float) * 2 * tangentUVSource,
															(uint32_t*)0, 0,
															mSceneManager->getNumWorkerThreads() );
					mSceneManager->executeUserScalableTask( tangentTask, false );
				}

				//Mesh is too big. O(N!) complexity. Just rely on the sheer power of the GPU.
				//TODO: Client option to force optimized or force non-optimized, or auto)
				optimizedNumVertices = numVertices;
				vertexConversionLut.resize( numVertices );
				for( uint32_t i=0; i<numVertices; ++i )
					vertexConversionLut[i] = i;
			}

			//Calculate AABB
			Ogre::Vector3 vMin(  std::numeric_limits<Ogre::Real>::max() );
			Ogre::Vector3 vMax( -std::numeric_limits<Ogre::Real>::max() );

			std::vector<BlenderRawVertex>::const_iterator rawVerticesIt = blenderRawVertices.begin();
			std::vector<BlenderRawVertex>::const_iterator rawVerticesEn = blenderRawVertices.end();

			while( rawVerticesIt != rawVerticesEn )
			{
				vMax.makeCeil( rawVerticesIt->vPos );
				vMin.makeFloor( rawVerticesIt->vPos );
				++rawVerticesIt;
			}

			aabb.setExtents( vMin, vMax );
		}

		//Read material table
		const uint16_t materialTableSize = smartData.read<uint16_t>();
		std::vector<uint32_t> materialTable;
		materialTable.resize( materialTableSize );

		if( !materialTable.empty() )
		{
			smartData.read( reinterpret_cast<uint8_t*>( &materialTable[0] ),
							sizeof(uint32_t) * materialTable.size() );
		}

		//Holds references to materialTable[], each entry is unique (i.e. no duplicates)
		std::vector<uint16_t> uniqueMaterials;

		//Split into submeshes based on material assignment.
		std::vector< std::vector<uint32_t> > indices;
		if( numVertices != 0 )
		{
			uint32_t currentVertex = 0;
			Ogre::FastArray<uint16_t>::const_iterator itor = materialIds.begin();
			Ogre::FastArray<uint16_t>::const_iterator end  = materialIds.end();

			while( itor != end )
			{
				std::vector<uint16_t>::const_iterator itSubMesh = std::find( uniqueMaterials.begin(),
																			 uniqueMaterials.end(),
																			 *itor );
				if( itSubMesh == uniqueMaterials.end() )
				{
					//New material found. Need to split another submesh.
					uniqueMaterials.push_back( *itor );
					itSubMesh = uniqueMaterials.end() - 1;
					indices.push_back( std::vector<uint32_t>() );
				}

				size_t subMeshIdx = itSubMesh - uniqueMaterials.begin();

				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );
				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );
				indices[subMeshIdx].push_back( vertexConversionLut[currentVertex++] );

				++itor;
			}
		}

		if( tangentTask )
		{
			mSceneManager->waitForPendingUserScalableTask();
			delete tangentTask;
			tangentTask = 0;
		}

		//We've got all the data the way we want/need. Now deal with Ogre.
		BlenderMeshMap::const_iterator meshEntryIt = m_meshes.find( meshId );
		if( meshEntryIt == m_meshes.end() )
		{
			//We don't have this mesh.
			createMesh( meshId, meshName, optimizedNumVertices, vertexElements,
						dataPtrContainer, indices, aabb );
		}
		else
		{
			//Check if we can update it (i.e. reuse existing buffers).
			Ogre::Mesh *meshPtr = meshEntryIt->second.meshPtr;
			bool canReuse = true;

			if( indices.size() != meshPtr->getNumSubMeshes() )
				canReuse = false;

			for( uint16_t i=0; i<meshPtr->getNumSubMeshes() && canReuse; ++i )
			{
				Ogre::SubMesh *subMesh = meshPtr->getSubMesh( i );

				const Ogre::VertexBufferPackedVec &vertexBuffers =
						subMesh->mVao[0][0]->getVertexBuffers();

				//Vertex format changed! (e.g. added or removed UVs)
				if( vertexElements[0] != vertexBuffers[0]->getVertexElements() )
					canReuse = false;

				//Current buffer can't hold it, or it's too big
				if( optimizedNumVertices > vertexBuffers[0]->getNumElements() ||
					optimizedNumVertices < (vertexBuffers[0]->getNumElements() >> 2) )
				{
					canReuse = false;
				}

				Ogre::IndexBufferPacked *indexBuffer = subMesh->mVao[0][0]->getIndexBuffer();
				if( indices[i].size() > indexBuffer->getNumElements() ||
					indices[i].size() < (indexBuffer->getNumElements() >> 2u) )
				{
					canReuse = false;
				}
			}

			if( canReuse )
			{
				updateMesh( meshEntryIt->second, optimizedNumVertices,
							dataPtrContainer, indices, aabb );
			}
			else
			{
				//Warning: meshEntryIt gets invalidated
				recreateMesh( meshEntryIt->first, meshEntryIt->second, optimizedNumVertices,
							  vertexElements, dataPtrContainer, indices, aabb );
			}
		}

		//Now setup/update the materials
		meshEntryIt = m_meshes.find( meshId );
		const BlenderMesh &meshEntry = meshEntryIt->second;
		Ogre::Mesh *meshPtr = meshEntry.meshPtr;
		for( size_t i=0; i<meshPtr->getNumSubMeshes(); ++i )
		{
			const uint16_t tableIdx = uniqueMaterials[i];
			Ogre::String materialIdStr;
			if( tableIdx < materialTable.size() )
			{
				const uint32_t materialId = materialTable[tableIdx];
				materialIdStr = Ogre::StringConverter::toString( materialId );
			}
			meshPtr->getSubMesh( i )->setMaterialName( materialIdStr );

			BlenderItemVec::const_iterator itItem = meshEntry.items.begin();
			BlenderItemVec::const_iterator enItem = meshEntry.items.end();

			const Ogre::IdString materialIdHash = materialIdStr;
			while( itItem != enItem )
			{
				itItem->item->getSubItem( i )->setDatablock( materialIdHash );
				++itItem;
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::createMesh( uint32_t meshId, const Ogre::String &meshName,
								  uint32_t optimizedNumVertices,
								  const Ogre::VertexElement2VecVec &vertexElements,
								  Ogre::FreeOnDestructor &vertexDataPtrContainer,
								  const std::vector< std::vector<uint32_t> > &indices,
								  const Ogre::Aabb &aabb )
	{
		Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().createManual(
					toStr64(meshId), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );

		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

		Ogre::VertexBufferPackedVec vertexBuffers;

		if( optimizedNumVertices != 0 )
		{
			//Create actual GPU buffers.
			Ogre::VertexBufferPacked *vertexBuffer = vaoManager->createVertexBuffer(
						vertexElements[0], optimizedNumVertices, Ogre::BT_DEFAULT,
					vertexDataPtrContainer.ptr, true );
			vertexDataPtrContainer.ptr = 0;
			vertexBuffers.push_back( vertexBuffer );
		}

		const Ogre::IndexBufferPacked::IndexType indexType = optimizedNumVertices > 0xffff ?
					Ogre::IndexBufferPacked::IT_32BIT : Ogre::IndexBufferPacked::IT_16BIT;

		std::vector< std::vector<uint32_t> >::const_iterator itor = indices.begin();
		std::vector< std::vector<uint32_t> >::const_iterator end  = indices.end();
		while( itor != end )
		{
			const size_t indexBytes =
					itor->size() * (indexType == Ogre::IndexBufferPacked::IT_16BIT ? 2 : 4);
			uint16_t *indexData = reinterpret_cast<uint16_t*>(
						OGRE_MALLOC_SIMD( indexBytes, Ogre::MEMCATEGORY_GEOMETRY ) );
			Ogre::FreeOnDestructor safeIndexPtr( indexData );

			if( indexType == Ogre::IndexBufferPacked::IT_32BIT )
				memcpy( indexData, &(*itor)[0], itor->size() * sizeof(uint32_t) );
			else
			{
				for( size_t i=0; i<itor->size(); ++i )
					indexData[i] = static_cast<uint16_t>( (*itor)[i] );
			}

			Ogre::IndexBufferPacked *indexBuffer = vaoManager->createIndexBuffer( indexType,
																				  itor->size(),
																				  Ogre::BT_DEFAULT,
																				  indexData, true );
			safeIndexPtr.ptr = 0;

			Ogre::VertexArrayObject *vao = vaoManager->createVertexArrayObject( vertexBuffers,
																				indexBuffer,
																				Ogre::OT_TRIANGLE_LIST );
			Ogre::SubMesh *subMesh = meshPtr->createSubMesh();
			subMesh->mVao[0].push_back( vao );
			subMesh->mVao[1].push_back( vao );

			subMesh->setMaterialName( "##INTERNAL## DEFAULT" );

			++itor;
		}

		meshPtr->_setBounds( aabb );

		BlenderMesh meshEntry;
		meshEntry.meshPtr			= meshPtr.get();
		meshEntry.userFriendlyName	= meshName;
		m_meshes[meshId] = meshEntry;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::updateMesh( const BlenderMesh &meshEntry, uint32_t optimizedNumVertices,
								  Ogre::FreeOnDestructor &vertexDataPtrContainer,
								  const std::vector< std::vector<uint32_t> > &indices,
								  const Ogre::Aabb &aabb )
	{
		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

		Ogre::Mesh *meshPtr = meshEntry.meshPtr;

		//Upload vertex data (all submeshes share it)
		if( meshPtr->getNumSubMeshes() > 0 )
		{
			Ogre::SubMesh *subMesh = meshPtr->getSubMesh( 0 );
			Ogre::VertexBufferPacked *vertexBuffer = subMesh->mVao[0][0]->getVertexBuffers()[0];
			vertexBuffer->upload( vertexDataPtrContainer.ptr, 0, optimizedNumVertices );
		}

		for( uint16_t i=0; i<meshPtr->getNumSubMeshes(); ++i )
		{
			Ogre::SubMesh *subMesh = meshPtr->getSubMesh( i );

			Ogre::IndexBufferPacked *indexBuffer = subMesh->mVao[0][0]->getIndexBuffer();
			if( indexBuffer->getIndexType() == Ogre::IndexBufferPacked::IT_32BIT )
			{
				indexBuffer->upload( &indices[i][0], 0, indices[i].size() );
			}
			else
			{
				//const_cast so we can update the shadow copy without
				//malloc'ing extra memory and perform the copy in place.
				uint16_t * RESTRICT_ALIAS shadowCopy = reinterpret_cast<uint16_t * RESTRICT_ALIAS>(
													const_cast<void*>( indexBuffer->getShadowCopy() ));
				Ogre::StagingBuffer *stagingBuffer = vaoManager->getStagingBuffer(
							indices[i].size() * sizeof(uint16_t), true );

				uint16_t * RESTRICT_ALIAS index16 = reinterpret_cast<uint16_t * RESTRICT_ALIAS>(
							stagingBuffer->map( indices[i].size() * sizeof(uint16_t) ) );

				for( size_t j=0; j<indices[i].size(); ++j )
				{
					shadowCopy[j]	= static_cast<uint16_t>( indices[i][j] );
					index16[j]		= static_cast<uint16_t>( indices[i][j] );
				}

				stagingBuffer->unmap( Ogre::StagingBuffer::
									  Destination( indexBuffer, 0, 0,
												   indices[i].size() * sizeof(uint16_t) ) );
				stagingBuffer->removeReferenceCount();
			}

			subMesh->mVao[0][0]->setPrimitiveRange( 0, indices[i].size() );
			subMesh->setMaterialName( "##INTERNAL## DEFAULT" );
		}

		meshPtr->_setBounds( aabb );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::recreateMesh( uint32_t meshId, BlenderMesh meshEntry, uint32_t optimizedNumVertices,
									const Ogre::VertexElement2VecVec &vertexElements,
									Ogre::FreeOnDestructor &vertexDataPtrContainer,
									const std::vector< std::vector<uint32_t> > &indices,
									const Ogre::Aabb &aabb )
	{
		//Destroy all items, but first saving their state.
		ItemDataVec itemsData;
		itemsData.reserve( meshEntry.items.size() );
		BlenderItemVec::const_iterator itor = meshEntry.items.begin();
		BlenderItemVec::const_iterator end  = meshEntry.items.end();

		while( itor != end )
		{
			Ogre::SceneNode *sceneNode = itor->item->getParentSceneNode();

			ItemData itemData;
			itemData.id			= itor->id;
			itemData.name		= itor->item->getName();
			itemData.position	= sceneNode->getPosition();
			itemData.rotation	= sceneNode->getOrientation();
			itemData.scale		= sceneNode->getScale();
			itemsData.push_back( itemData );

			sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
			mSceneManager->destroyItem( itor->item );

			++itor;
		}

		meshEntry.items.clear();

		//Destroy mesh
		Ogre::String userFriendlyName = meshEntry.userFriendlyName;

		destroyMeshVaos( meshEntry.meshPtr );
		Ogre::MeshManager::getSingleton().remove( meshEntry.meshPtr->getName() );
		meshEntry.meshPtr = 0;
		m_meshes.erase( meshId );

		//Create mesh again.
		createMesh( meshId, userFriendlyName, optimizedNumVertices,
					vertexElements, vertexDataPtrContainer, indices, aabb );

		//Restore the items.
		BlenderMesh &newBlenderMesh = m_meshes[meshId];

		ItemDataVec::const_iterator itItem = itemsData.begin();
		ItemDataVec::const_iterator enItem = itemsData.end();

		while( itItem != enItem )
		{
			createItem( newBlenderMesh, *itItem );
			++itItem;
		}
	}
	//-----------------------------------------------------------------------------------
	bool DergoSystem::syncItem( Network::SmartData &smartData )
	{
		bool retVal = true;

		ItemData itemData;

		uint32_t meshId	= smartData.read<uint32_t>();
		itemData.id		= smartData.read<uint32_t>();

		itemData.name = smartData.getString();

		itemData.position.x = smartData.read<float>();
		itemData.position.y = smartData.read<float>();
		itemData.position.z = smartData.read<float>();
		itemData.rotation.w = smartData.read<float>();
		itemData.rotation.x = smartData.read<float>();
		itemData.rotation.y = smartData.read<float>();
		itemData.rotation.z = smartData.read<float>();
		itemData.scale.x = smartData.read<float>();
		itemData.scale.y = smartData.read<float>();
		itemData.scale.z = smartData.read<float>();

		BlenderMeshMap::iterator itMeshEntry = m_meshes.find( meshId );
		if( itMeshEntry != m_meshes.end() )
		{
			BlenderItemVec::iterator itItem = itMeshEntry->second.findItem( itemData.id );
			if( itItem != itMeshEntry->second.items.end() )
			{
				itItem->item->setName( itemData.name );
				Ogre::Node *node = itItem->item->getParentNode();
				node->setPosition( itemData.position );
				node->setOrientation( itemData.rotation );
				node->setScale( itemData.scale );
			}
			else
			{
				createItem( itMeshEntry->second, itemData );
			}
		}
		else
		{
			//Shouldn't happen! Tell client to resync
			assert( false );
			retVal = false;
		}

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::createItem( BlenderMesh &blenderMesh, const ItemData &itemData )
	{
        Ogre::Item *item = mSceneManager->createItem( blenderMesh.meshPtr->getName() );
		item->setName( itemData.name );
		item->setVisibilityFlags( 1u );

		Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode();
		sceneNode->setPosition( itemData.position );
		sceneNode->setOrientation( itemData.rotation );
		sceneNode->setScale( itemData.scale );
		sceneNode->attachObject( item );

		blenderMesh.items.push_back( BlenderItem( itemData.id, item ) );
	}
	//-----------------------------------------------------------------------------------
	bool DergoSystem::destroyItem( Network::SmartData &smartData )
	{
		bool retVal = true;

		uint32_t meshId = smartData.read<uint32_t>();
		uint32_t itemId = smartData.read<uint32_t>();

		BlenderMeshMap::iterator itMeshEntry = m_meshes.find( meshId );
		if( itMeshEntry != m_meshes.end() )
		{
			BlenderItemVec::iterator itemIt = itMeshEntry->second.findItem( itemId );

			//The client may request us to delete the same object twice. Just ignore
			//the remaining ones. Unfortunately due to how Blender works on name
			//changes it is hard to send exactly one delete without duplicates.
			if( itemIt != itMeshEntry->second.items.end() )
			{
				Ogre::SceneNode *sceneNode = itemIt->item->getParentSceneNode();
				sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
				mSceneManager->destroyItem( itemIt->item );

				Ogre::efficientVectorRemove( itMeshEntry->second.items, itemIt );
			}
		}
		else
		{
			//Shouldn't happen! Tell client to resync
			assert( false );
			retVal = false;
		}

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncLight( Network::SmartData &smartData )
	{
		const uint32_t lightId = smartData.read<uint32_t>();
		const Ogre::String lightName = smartData.getString();

		BlenderLightVec::iterator itor = std::lower_bound( m_lights.begin(), m_lights.end(),
														   lightId, BlenderLightCmp() );

		if( itor == m_lights.end() || itor->id != lightId )
		{
			//Doesn't exist. Create.
			Ogre::Light *light = mSceneManager->createLight();
			Ogre::SceneNode *lightNode = mSceneManager->getRootSceneNode()->createChildSceneNode();
			lightNode->attachObject( light );
			itor = m_lights.insert( itor, BlenderLight( lightId, light ) );
		}

		Ogre::Light *light = itor->light;
		light->setName( lightName );

		const uint8_t lightType = smartData.read<uint8_t>();
		const bool castShadow	= smartData.read<uint8_t>() != 0;
		const float powerSign	= smartData.read<uint8_t>() == 0 ? 1.0f : -1.0f;
		const bool lockSpecular	= smartData.read<uint8_t>() != 0;
		const bool hasObbRestraint = smartData.read<uint8_t>() != 0;
		const Ogre::Vector3 colour = smartData.read<Ogre::Vector3>();
		const float powerScale	= smartData.read<float>();

		if( lightType != light->getType() )
		{
			assert( lightType < Ogre::Light::NUM_LIGHT_TYPES );
			light->setType( static_cast<Ogre::Light::LightTypes>( lightType ) );
		}

		light->setCastShadows( castShadow );
		light->setDiffuseColour( colour.x, colour.y, colour.z );
		light->setSpecularColour( colour.x, colour.y, colour.z );
		light->setPowerScale( powerScale * powerSign );

		const Ogre::Vector3 vPos	= smartData.read<Ogre::Vector3>();
		const Ogre::Quaternion qRot	= smartData.read<Ogre::Quaternion>();
		light->getParentNode()->setPosition( vPos );
		light->getParentNode()->setOrientation( qRot );

		//const bool useRangeInsteadOfRadius	= smartData.read<uint8_t>() != 0;
		const float radius				= smartData.read<float>();
		const float rangeOrThreshold	= smartData.read<float>();

		if( true /*thresholdMode*/ )
		{
			light->setAttenuationBasedOnRadius( radius, rangeOrThreshold );
		}
		else
		{
			// range mode
			light->setAttenuation( rangeOrThreshold, 0.5f, 0.0f, 0.5f / (radius * radius) );
		}

		if( lightType == Ogre::Light::LT_SPOTLIGHT )
		{
			const float spotOuterAngle	= smartData.read<float>();
			const float spotInnerAngle	= smartData.read<float>() * spotOuterAngle;
			const float spotFalloff		= smartData.read<float>();

			light->setSpotlightRange( Ogre::Radian( spotInnerAngle ),
									  Ogre::Radian( spotOuterAngle ),
									  spotFalloff );
		}
		else if( lightType == Ogre::Light::LT_AREA_LTC )
		{
			const float rectX	= smartData.read<float>();
			const float rectY	= smartData.read<float>();
			light->setRectSize( Ogre::Vector2( rectX, rectY ) );
		}

		if( !lockSpecular )
		{
			const Ogre::Vector3 specularCol = smartData.read<Ogre::Vector3>();
			light->setSpecularColour( specularCol.x, specularCol.y, specularCol.z );
		}

		if( hasObbRestraint )
		{
			const Ogre::Vector3 vPosObb		= smartData.read<Ogre::Vector3>();
			const Ogre::Quaternion qRotObb	= smartData.read<Ogre::Quaternion>();
			const Ogre::Vector3 vScaleObb	= smartData.read<Ogre::Vector3>();

			Ogre::Node *obbRestraint = light->getObbRestraint();
			if( !obbRestraint )
			{
				obbRestraint = mSceneManager->createSceneNode();
				light->setObbRestraint( obbRestraint );
			}

			obbRestraint->setPosition( vPosObb );
			obbRestraint->setOrientation( qRotObb );
			obbRestraint->setScale( vScaleObb );
		}
		else
		{
			Ogre::SceneNode *obbRestraint = static_cast<Ogre::SceneNode*>( light->getObbRestraint() );
			if( obbRestraint )
			{
				mSceneManager->destroySceneNode( obbRestraint );
				light->setObbRestraint( 0 );
			}
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::destroyLight( Network::SmartData &smartData )
	{
		const uint32_t lightId = smartData.read<uint32_t>();

		BlenderLightVec::iterator itor = std::lower_bound( m_lights.begin(), m_lights.end(),
														   lightId, BlenderLightCmp() );

		if( itor != m_lights.end() && itor->id == lightId )
		{
			Ogre::SceneNode *sceneNode = itor->light->getParentSceneNode();
			sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
			mSceneManager->destroyLight( itor->light );

			m_lights.erase( itor );
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncEmpty( Network::SmartData &smartData )
	{
		const uint32_t emptyId = smartData.read<uint32_t>();

		bool sharedIrPccChanged = false;
		bool pccChanged = false;

		BlenderEmptyVec::iterator itor = std::lower_bound( m_empties.begin(), m_empties.end(),
														   emptyId, BlenderEmptyCmp() );

		if( itor == m_empties.end() || itor->id != emptyId )
		{
			//Doesn't exist. Create.
			itor = m_empties.insert( itor, BlenderEmpty( emptyId ) );
			sharedIrPccChanged = true;
		}

		BlenderEmpty &empty = *itor;

		const bool isPccProbe				= smartData.read<Ogre::uint8>() != 0;
		const bool pccIsStatic				= smartData.read<Ogre::uint8>() != 0;
		const Ogre::uint8 pccNumIterations	= smartData.read<Ogre::uint8>();
		const bool isIrAoI					= smartData.read<Ogre::uint8>() != 0;
		const float irRadius				= smartData.read<float>();
		const Ogre::Vector3 vPos			= smartData.read<Ogre::Vector3>();
		const Ogre::Quaternion qRot			= smartData.read<Ogre::Quaternion>();
		const Ogre::Vector3 vHalfSize		= smartData.read<Ogre::Vector3>();
		const Ogre::Vector3 pccCamPos		= smartData.read<Ogre::Vector3>();
		const Ogre::Vector3 pccInnerRegion	= smartData.read<Ogre::Vector3>();

		pccChanged |= setIfChanged( empty.pccIsStatic, pccIsStatic );
		if( isPccProbe && pccChanged && empty.probe )
			empty.probe->setStatic( empty.pccIsStatic );

		pccChanged |= setIfChanged( empty.pccNumIterations, pccNumIterations );

		if( isPccProbe && !empty.probe )
		{
			empty.probe = m_parallaxCorrectedCubemap->createProbe();
			empty.probe->mNumIterations = empty.pccNumIterations;
			Ogre::TextureGpu *cubemapTex = m_parallaxCorrectedCubemap->getBindTexture();
			if( cubemapTex )
			{
				empty.probe->setTextureParams( cubemapTex->getWidth(), cubemapTex->getHeight(), false,
											   Ogre::PFG_RGBA16_FLOAT, pccIsStatic );
				empty.probe->initWorkspace();
			}
		}
		else if( !isPccProbe && empty.probe )
		{
			m_parallaxCorrectedCubemap->destroyProbe( empty.probe );
			empty.probe = 0;
		}

		m_irDirty	|= setIfChanged( empty.isAoI, isIrAoI );
		sharedIrPccChanged |= setIfChanged( empty.position, vPos );
		sharedIrPccChanged |= setIfChanged( empty.qRot, qRot );
		sharedIrPccChanged |= setIfChanged( empty.halfSize, vHalfSize );
		pccChanged |= setIfChanged( empty.pccCamPos, pccCamPos );
		pccChanged |= setIfChanged( empty.pccInnerRegion, pccInnerRegion );

		m_irDirty	|= sharedIrPccChanged;
		pccChanged	|= sharedIrPccChanged;

		if( empty.probe && pccChanged )
		{
			Ogre::Aabb probeShape( vPos, vHalfSize );
			Ogre::Matrix3 orientationMat;
			qRot.ToRotationMatrix( orientationMat );
			empty.probe->mNumIterations = empty.pccNumIterations;
			empty.probe->set( empty.pccCamPos, probeShape, empty.pccInnerRegion,
							  orientationMat, probeShape );
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::destroyEmpty( Network::SmartData &smartData )
	{
		const uint32_t emptyId = smartData.read<uint32_t>();

		BlenderEmptyVec::iterator itor = std::lower_bound( m_empties.begin(), m_empties.end(),
														   emptyId, BlenderEmptyCmp() );

		if( itor != m_empties.end() && itor->id == emptyId )
		{
			if( itor->probe )
				m_parallaxCorrectedCubemap->destroyProbe( itor->probe );
			if( itor->isAoI )
				m_irDirty = true;
			m_empties.erase( itor );
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncMaterial( Network::SmartData &smartData )
	{
		const uint32_t materialId = smartData.read<uint32_t>();
		const Ogre::String matName = smartData.getString();

		BlenderMaterialVec::iterator itor = std::lower_bound( m_materials.begin(), m_materials.end(),
															  materialId, BlenderMaterialCmp() );

		if( itor == m_materials.end() || itor->id != materialId )
		{
			//Doesn't exist. Create.
			Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
			Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
			assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
			Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );

			const Ogre::String materialIdStr = Ogre::StringConverter::toString( materialId );
			Ogre::HlmsDatablock *datablock = hlmsPbs->createDatablock( materialIdStr, matName,
																	   Ogre::HlmsMacroblock(),
																	   Ogre::HlmsBlendblock(),
																	   Ogre::HlmsParamVec() );
			itor = m_materials.insert( itor, BlenderMaterial( materialId, datablock ) );
		}

		assert( dynamic_cast<Ogre::HlmsPbsDatablock*>( itor->datablock ) );

		Ogre::HlmsPbsDatablock *datablock = static_cast<Ogre::HlmsPbsDatablock*>( itor->datablock );

		const uint32_t brdfType			= smartData.read<uint32_t>();
		const uint8_t materialWorkflow  = smartData.read<uint8_t>();
		const uint8_t cullMode			= smartData.read<uint8_t>();
		const uint8_t cullModeShadow	= smartData.read<uint8_t>();
		const bool twoSided				= smartData.read<uint8_t>() != 0;
		const uint8_t transparencyMode	= smartData.read<uint8_t>();

        assert( materialWorkflow <= Ogre::HlmsPbsDatablock::MetallicWorkflow );

		datablock->setBrdf( static_cast<Ogre::PbsBrdf::PbsBrdf>(brdfType) );
        datablock->setWorkflow( static_cast<Ogre::HlmsPbsDatablock::Workflows>( materialWorkflow ) );

		{
			assert( cullMode <= Ogre::CULL_ANTICLOCKWISE );
			assert( cullModeShadow <= Ogre::CULL_ANTICLOCKWISE );

			if( twoSided != datablock->getTwoSidedLighting() )
				datablock->setTwoSidedLighting( twoSided, false );

			if( twoSided )
			{
				//Perform what datablock->setTwoSidedLighting( twoSided, true ); would do,
				//but manually (so we can update without having to flush all renderables).
				Ogre::HlmsMacroblock macroblock;
				if( cullMode != 0 )
					macroblock.mCullMode = static_cast<Ogre::CullingMode>( cullMode );
				else
					macroblock.mCullMode = Ogre::CULL_NONE;

				datablock->setMacroblock( macroblock, false );

				if( cullModeShadow != 0 )
					macroblock.mCullMode = static_cast<Ogre::CullingMode>( cullModeShadow );
				else
					macroblock.mCullMode = Ogre::CULL_ANTICLOCKWISE;

				datablock->setMacroblock( macroblock, true );
			}
			else
			{
				Ogre::HlmsMacroblock macroblock;
				if( cullMode != 0 )
					macroblock.mCullMode = static_cast<Ogre::CullingMode>( cullMode );

				datablock->setMacroblock( macroblock, false );

				if( cullModeShadow != 0 )
				{
					macroblock.mCullMode = static_cast<Ogre::CullingMode>( cullModeShadow );
					datablock->setMacroblock( macroblock, true );
				}
			}
		}

		float transparencyValue = 1.0f;
		bool useAlphaFromTextures = true;
		if( transparencyMode != Ogre::HlmsPbsDatablock::None )
		{
			transparencyValue		= smartData.read<float>();
			useAlphaFromTextures	= smartData.read<uint8_t>() != 0;
		}

		if( datablock->getTransparency() != transparencyValue ||
			datablock->getTransparencyMode() != transparencyMode ||
			datablock->getUseAlphaFromTextures() != useAlphaFromTextures )
		{
			assert( transparencyMode <= Ogre::HlmsPbsDatablock::Fade );
			datablock->setTransparency( transparencyValue,
										static_cast<Ogre::HlmsPbsDatablock::TransparencyModes>(
											transparencyMode ),
										useAlphaFromTextures );
		}

		const uint8_t alphaTestCmpFunc	= smartData.read<uint8_t>();
		assert( alphaTestCmpFunc < Ogre::NUM_COMPARE_FUNCTIONS );
		datablock->setAlphaTest( static_cast<Ogre::CompareFunction>( alphaTestCmpFunc ) );

		if( alphaTestCmpFunc != Ogre::CMPF_ALWAYS_PASS && alphaTestCmpFunc != Ogre::CMPF_ALWAYS_FAIL )
		{
			const float alphaTestThreshold	= smartData.read<float>();
			datablock->setAlphaTestThreshold( alphaTestThreshold );
		}

		const Ogre::Vector3 kD			= smartData.read<Ogre::Vector3>();
		const Ogre::Vector3 kS			= smartData.read<Ogre::Vector3>();
		const float roughness			= smartData.read<float>();
		const float normalMapWeight		= smartData.read<float>();
		const Ogre::Vector3 emissiveCol	= smartData.read<Ogre::Vector3>();
		const Ogre::Vector3 fresnel		= smartData.read<Ogre::Vector3>();

		datablock->setDiffuse( kD );
		datablock->setSpecular( kS );
		datablock->setRoughness( roughness );
		datablock->setEmissive( emissiveCol );

		datablock->setNormalMapWeight( normalMapWeight );

        if( datablock->getWorkflow() == Ogre::HlmsPbsDatablock::MetallicWorkflow )
			datablock->setMetalness( fresnel.x );
		else
			datablock->setFresnel( fresnel, (fresnel.x != fresnel.y || fresnel.y != fresnel.z) );

		for( int i=0; i<Ogre::NUM_PBSM_TEXTURE_TYPES; ++i )
		{
			const uint8_t packedData	= smartData.read<uint8_t>();
			const uint8_t uvSet			= smartData.read<uint8_t>();

			Ogre::HlmsSamplerblock samplerblock;

			samplerblock.mU = static_cast<Ogre::TextureAddressingMode>( packedData & 0x03u );
			samplerblock.mV = static_cast<Ogre::TextureAddressingMode>( (packedData >> 2u) & 0x03u );
			const Ogre::TextureFilterOptions texFilter =
					static_cast<Ogre::TextureFilterOptions>( (packedData >> 4u) & 0x03u );

			samplerblock.setFiltering( texFilter );

			if( samplerblock.mU == Ogre::TAM_BORDER || samplerblock.mV == Ogre::TAM_BORDER )
				samplerblock.mBorderColour = smartData.read<Ogre::ColourValue>();

			datablock->setSamplerblock( static_cast<Ogre::PbsTextureTypes>(i), samplerblock );
			if( i < Ogre::NUM_PBSM_SOURCES )
				datablock->setTextureUvSource( static_cast<Ogre::PbsTextureTypes>(i), uvSet );
		}

		for( int i=0; i<4; ++i )
		{
			const uint8_t blendMode	= smartData.read<uint8_t>();
			assert( blendMode < Ogre::NUM_PBSM_BLEND_MODES );
			datablock->setDetailMapBlendMode( i, static_cast<Ogre::PbsBlendModes>( blendMode ) );
			const float weight				= smartData.read<float>();
			const Ogre::Vector4 offsetScale	= smartData.read<Ogre::Vector4>();

			datablock->setDetailMapWeight( i, weight );
			datablock->setDetailMapOffsetScale( i, offsetScale );
		}

		for( int i=0; i<4; ++i )
		{
			const float weight = smartData.read<float>();
			datablock->setDetailNormalWeight( i, weight );
		}
	}
	//-----------------------------------------------------------------------------------
	bool DergoSystem::syncMaterialTexture( Network::SmartData &smartData )
	{
		const uint32_t materialId	= smartData.read<uint32_t>();
		const uint8_t slot			= smartData.read<uint8_t>();
		const uint64_t textureId	= smartData.read<uint64_t>();
		const uint8_t textureMapType= smartData.read<uint8_t>();

		bool retVal = false;
		BlenderMaterialVec::iterator itor = std::lower_bound( m_materials.begin(), m_materials.end(),
															  materialId, BlenderMaterialCmp() );

		if( itor != m_materials.end() && itor->id == materialId )
		{
			retVal = true;

			assert( slot < Ogre::NUM_PBSM_TEXTURE_TYPES );

			assert( reinterpret_cast<Ogre::HlmsPbsDatablock*>( itor->datablock ) );
			Ogre::HlmsPbsDatablock *pbsDatablock = static_cast<Ogre::HlmsPbsDatablock*>( itor->datablock );

			if( textureId )
			{
				assert( textureMapType < Ogre::CommonTextureTypes::NumCommonTextureTypes );

				Ogre::TextureGpuManager *textureManager =
						mRoot->getRenderSystem()->getTextureGpuManager();

				const Ogre::String aliasName = toStr64( textureId );

				Ogre::TextureGpu *texture = textureManager->findTextureNoThrow( aliasName );
				pbsDatablock->setTexture( slot, texture );
			}
			else
			{
				pbsDatablock->setTexture( slot, 0 );
			}

			/*static bool bLoaded = false;
			if( !bLoaded )
			{
				Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
				Ogre::HlmsTextureManager *hlmsTextureMgr = hlmsManager->getTextureManager();
				const Ogre::String texturePath = "/home/matias/Projects/SDK/OgreLatest/Samples/Media/materials/textures/Cubemaps/SaintPetersBasilica.dds";
				Ogre::Image image;
				openImageFromFile( texturePath, image );
				if( image.getWidth() > 0 && image.getHeight() > 0 )
				{
					assert( textureMapType < Ogre::HlmsTextureManager::NUM_TEXTURE_TYPES );

					hlmsTextureMgr->createOrRetrieveTexture(
								"aliasName3D", texturePath,
								Ogre::HlmsTextureManager::TEXTURE_TYPE_ENV_MAP,
								&image );
				}
				bLoaded = true;
			}
			{
				Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
				Ogre::HlmsTextureManager *hlmsTextureMgr = hlmsManager->getTextureManager();

				Ogre::HlmsTextureManager::TextureLocation texLocation =
						hlmsTextureMgr->createOrRetrieveTexture(
							"aliasName3D", "",
							Ogre::HlmsTextureManager::TEXTURE_TYPE_ENV_MAP );
				pbsDatablock->setTexture( Ogre::PBSM_REFLECTION,
										  texLocation.xIdx, texLocation.texture );
			}*/
		}

		assert( retVal );

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	Ogre::DataStreamPtr DergoSystem::resourceLoading( const Ogre::String &name,
													  const Ogre::String &group,
													  Ogre::Resource *resource )
	{
		return Ogre::DataStreamPtr();
	}
	void DergoSystem::resourceStreamOpened( const Ogre::String &name, const Ogre::String &group,
											Ogre::Resource *resource, Ogre::DataStreamPtr& dataStream )
	{
	}
	bool DergoSystem::resourceCollision( Ogre::Resource *resource,
										 Ogre::ResourceManager *resourceManager )
	{
		return false;
	}
	//-----------------------------------------------------------------------------------
	bool DergoSystem::grouplessResourceExists( const Ogre::String &name )
	{
		std::ifstream infile( name.c_str() );
		return infile.good();
	}
	//-----------------------------------------------------------------------------------
	Ogre::DataStreamPtr DergoSystem::grouplessResourceLoading( const Ogre::String &name )
	{
		Ogre::DataStreamPtr retVal;

		std::ifstream *ifs = OGRE_NEW_T(std::ifstream, Ogre::MEMCATEGORY_GENERAL)(
								 name.c_str(), std::ios::binary|std::ios::in );
		if( ifs->is_open() )
		{
			const Ogre::String::size_type extPos = name.find_last_of( '.' );
			if( extPos != Ogre::String::npos )
			{
				const Ogre::String texExt = name.substr( extPos+1 );
				retVal = Ogre::DataStreamPtr( OGRE_NEW Ogre::FileStreamDataStream( name, ifs ) );
			}
		}
		else
		{
			OGRE_DELETE_T( ifs, basic_ifstream, Ogre::MEMCATEGORY_GENERAL );
		}

		return retVal;
	}
	//-----------------------------------------------------------------------------------
	Ogre::DataStreamPtr DergoSystem::grouplessResourceOpened( const Ogre::String &name,
															  Ogre::Archive *archive,
															  Ogre::DataStreamPtr &dataStream )
	{
		return dataStream;
	}
	//-----------------------------------------------------------------------------------
//	void DergoSystem::openImageFromFile( const Ogre::String &filename, Ogre::Image2 &outImage )
//	{
//		std::ifstream ifs( filename.c_str(), std::ios::binary|std::ios::in );
//		if( ifs.is_open() )
//		{
//			const Ogre::String::size_type extPos = filename.find_last_of( '.' );
//			if( extPos != Ogre::String::npos )
//			{
//				const Ogre::String texExt = filename.substr( extPos+1 );
//				Ogre::DataStreamPtr dataStream( OGRE_NEW Ogre::FileStreamDataStream( filename,
//																					 &ifs, false ) );
//				outImage.load( dataStream, texExt );
//			}

//			ifs.close();
//		}
//	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::syncTexture( Network::SmartData &smartData )
	{
		const uint64_t textureId		= smartData.read<uint64_t>();
		const uint8_t textureMapType	= smartData.read<uint8_t>();
		const Ogre::String texturePath	= smartData.getString();

		const Ogre::String aliasName = toStr64( textureId );
		const Ogre::IdString aliasNameHash( aliasName );

		TexAliasToFullPathMap::iterator itor = m_textures.find( aliasNameHash );

		if( itor == m_textures.end() )
		{
			Ogre::Image2 image;

			assert( textureMapType < Ogre::CommonTextureTypes::NumCommonTextureTypes );

			Ogre::TextureGpuManager *textureManager =
					mRoot->getRenderSystem()->getTextureGpuManager();

			textureManager->createOrRetrieveTexture(
						texturePath, aliasName,
						Ogre::GpuPageOutStrategy::Discard,
						static_cast<Ogre::CommonTextureTypes::CommonTextureTypes>(textureMapType),
						"Listener Group" );
			m_textures[aliasNameHash] = texturePath;
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::reset()
	{
		m_enableInstantRadiosity = false;
		m_instantRadiosity->clear();
		m_instantRadiosity->freeMemory();
		m_instantRadiosity->mAoI.clear();
		m_irDirty = false;

		m_parallaxCorrectedCubemap->setEnabled( false, 0, 0, Ogre::PFG_UNKNOWN );
		m_parallaxCorrectedCubemap->destroyAllProbes();

		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );
		hlmsPbs->setParallaxCorrectedCubemap( 0 );
		hlmsPbs->setIrradianceVolume( 0 );

		{
			BlenderMeshMap::iterator itor = m_meshes.begin();
			BlenderMeshMap::iterator end  = m_meshes.end();

			while( itor != end )
			{
				//Remove all items for this mesh
				BlenderItemVec::const_iterator itItem = itor->second.items.begin();
				BlenderItemVec::const_iterator enItem = itor->second.items.end();

				while( itItem != enItem )
				{
					Ogre::SceneNode *sceneNode = itItem->item->getParentSceneNode();
					sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
					mSceneManager->destroyItem( itItem->item );

					++itItem;
				}

				//Remove the mesh.
				destroyMeshVaos( itor->second.meshPtr );
				Ogre::MeshManager::getSingleton().remove( itor->second.meshPtr->getName() );
				itor->second.meshPtr = 0;

				++itor;
			}

			m_meshes.clear();
		}

		{
			BlenderLightVec::const_iterator itor = m_lights.begin();
			BlenderLightVec::const_iterator end  = m_lights.end();

			while( itor != end )
			{
				Ogre::SceneNode *sceneNode = itor->light->getParentSceneNode();
				sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
				mSceneManager->destroyLight( itor->light );
				++itor;
			}

			m_lights.clear();
		}

		{
			//PCC Probes have already been destroyed
			m_empties.clear();
		}

		{
			BlenderMaterialVec::const_iterator itor = m_materials.begin();
			BlenderMaterialVec::const_iterator end  = m_materials.end();

			while( itor != end )
			{
				Ogre::Hlms *hlms = itor->datablock->getCreator();
				hlms->destroyDatablock( itor->datablock->getName() );
				++itor;
			}

			m_materials.clear();
		}

		{
			Ogre::TextureGpuManager *textureManager = mRoot->getRenderSystem()->getTextureGpuManager();

			TexAliasToFullPathMap::const_iterator itor = m_textures.begin();
			TexAliasToFullPathMap::const_iterator end  = m_textures.end();

			while( itor != end )
			{
				textureManager->destroyTexture( textureManager->findTextureNoThrow( itor->first ) );
				++itor;
			}

			m_textures.clear();
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::exportToFile( Network::SmartData &smartData )
	{
		const Ogre::String &fullPath = smartData.getString();

		const bool objects          = smartData.read<uint8_t>() != 0;
		const bool lights           = smartData.read<uint8_t>() != 0;
		const bool materials        = smartData.read<uint8_t>() != 0;
		const bool textures         = smartData.read<uint8_t>() != 0;
		const bool meshes           = smartData.read<uint8_t>() != 0;
		const bool sceneSettings    = smartData.read<uint8_t>() != 0;
		const bool instantRadiosity = smartData.read<uint8_t>() != 0;
		const bool parallaxCorrectedCubemaps = smartData.read<uint8_t>() != 0;

		Ogre::uint32 exportFlags = 0;

		if( objects )
		{
			exportFlags |= Ogre::SceneFlags::SceneNodes | Ogre::SceneFlags::Items |
						   Ogre::SceneFlags::Entities;
		}
		if( lights )
			exportFlags |= Ogre::SceneFlags::SceneNodes | Ogre::SceneFlags::Lights;
		if( materials )
			exportFlags |= Ogre::SceneFlags::Materials;
		if( textures )
			exportFlags |= Ogre::SceneFlags::TexturesOriginal;
		if( meshes )
			exportFlags |= Ogre::SceneFlags::Meshes;
		if( sceneSettings )
			exportFlags |= Ogre::SceneFlags::SceneSettings;
		if( instantRadiosity )
			exportFlags |= Ogre::SceneFlags::InstantRadiosity;
		if( parallaxCorrectedCubemaps )
			exportFlags |= Ogre::SceneFlags::ParallaxCorrectedCubemap;

		Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
		Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_PBS );
		assert( dynamic_cast<Ogre::HlmsPbs*>( hlms ) );
		Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlms );

		Ogre::ParallaxCorrectedCubemapBase *oldPcc = hlmsPbs->getParallaxCorrectedCubemap();
		hlmsPbs->setParallaxCorrectedCubemap( m_parallaxCorrectedCubemap );

		Ogre::SceneFormatExporter exporter( mRoot, mSceneManager, m_instantRadiosity );
		exporter.setListener( this );
		exporter.exportSceneToFile( fullPath, exportFlags );

		hlmsPbs->setParallaxCorrectedCubemap( oldPcc );
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::processMessage( const Network::MessageHeader &header,
									  Network::SmartData &smartData,
									  bufferevent *bev, NetworkSystem &networkSystem )
	{
		switch( header.messageType )
		{
		case Network::FromClient::ConnectionTest:
		{
			char str[128];
			size_t maxVal = std::min( 127u, header.sizeBytes );
			memcpy( str, smartData.getCurrentPtr(), maxVal );
		    str[maxVal] = '\0';
			printf( "%s", str );
			networkSystem.send( bev, Network::FromServer::ConnectionTest,
								"Hello you too", sizeof("Hello you too") );
		}
			break;
		case Network::FromClient::WorldParams:
			syncWorld( smartData );
			break;
		case Network::FromClient::InstantRadiosity:
			syncInstantRadiosity( smartData );
			break;
		case Network::FromClient::ParallaxCorrectedCubemaps:
			syncParallaxCorrectedCubemaps( smartData );
			break;
		case Network::FromClient::ShadowsSettings:
			syncShadowsSettings( smartData );
			break;
		case Network::FromClient::Mesh:
			syncMesh( smartData );
			break;
		case Network::FromClient::Item:
			if( !syncItem( smartData ) )
				networkSystem.send( bev, Network::FromServer::Resync, 0, 0 );
			break;
		case Network::FromClient::ItemRemove:
			if( !destroyItem( smartData ) )
				networkSystem.send( bev, Network::FromServer::Resync, 0, 0 );
			break;
		case Network::FromClient::Light:
			syncLight( smartData );
			break;
		case Network::FromClient::LightRemove:
			destroyLight( smartData );
			break;
		case Network::FromClient::Empty:
			syncEmpty( smartData );
			break;
		case Network::FromClient::EmptyRemove:
			destroyEmpty( smartData );
			break;
		case Network::FromClient::Material:
			syncMaterial( smartData );
			break;
		case Network::FromClient::MaterialTexture:
			if( !syncMaterialTexture( smartData ) )
				networkSystem.send( bev, Network::FromServer::Resync, 0, 0 );
			break;
		case Network::FromClient::Texture:
			syncTexture( smartData );
			break;
		case Network::FromClient::Reset:
			reset();
			break;
		case Network::FromClient::ExportToFile:
			exportToFile( smartData );
			break;
		case Network::FromClient::Render:
		{
			const bool returnResult		= smartData.read<uint8_t>() != 0;
			const uint64_t windowId		= smartData.read<uint64_t>();
			const Ogre::uint16 width	= smartData.read<Ogre::uint16>();
			const Ogre::uint16 height	= smartData.read<Ogre::uint16>();

			Ogre::Camera *camera = mCamera;

			if( returnResult )
			{
				if( width != mRenderWindow->getWidth() || height != mRenderWindow->getHeight() )
					mRenderWindow->requestResolution( width, height );
			}
			else
			{
				WindowMap::const_iterator itor = m_renderWindows.find( windowId );

				if( itor == m_renderWindows.end() )
				{
					Ogre::NameValuePairList params;
					params["vsync"] = "false"; //TODO: Only one window should be VSync'ed
					params["gamma"] = "true";
					params["FSAA"] = "0";

					if( mRoot->getRenderSystem()->getName() != "Direct3D11 Rendering Subsystem" )
					{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
						HGLRC currentGlContext = wglGetCurrentContext();
						params["externalGLContext"] = Ogre::StringConverter::toString(
														reinterpret_cast<size_t>(currentGlContext) );
#else
//						GLXContext currentGlContext = currentGlContext = glXGetCurrentContext();
//						params["externalGLContext"] = Ogre::StringConverter::toString(
//														reinterpret_cast<size_t>(currentGlContext) );
						params["externalGLContext"] = "Yes";
#endif
					}

					Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
					Window newWindow;

					const Ogre::String windowIdStr = toStr64( windowId );
#if OGRE_PLATFORM != OGRE_PLATFORM_LINUX
					newWindow.renderWindow = mRoot->createRenderWindow( "DERGO Window: " + windowIdStr,
																		width, height, false, &params );
#else
					//TODO: This workaround will be here until we fix Mesa + MultWindow
					newWindow.renderWindow = mRenderWindow;
#endif
					Ogre::TextureGpu *windowTex = newWindow.renderWindow->getTexture();

					newWindow.camera = mSceneManager->createCamera( "Camera: " + windowIdStr );
					newWindow.camera->setAutoAspectRatio( true );
					newWindow.workspace = compositorManager->addWorkspace( mSceneManager,
																		   windowTex,
																		   newWindow.camera,
																		   "DergoHdrWorkspace", true );
																		   //"DERGO Workspace", true );
					m_renderWindows[windowId] = newWindow;
					itor = m_renderWindows.find( windowId );

					Ogre::WindowEventUtilities::addWindowEventListener( newWindow.renderWindow,
																		m_windowEventListener );
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
					HWND hwnd = 0;
					newWindow.renderWindow->getCustomAttribute( "WINDOW", &hwnd );
					SetWindowPos( hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
					LONG_PTR cStyle = GetClassLongPtr( hwnd, GCL_STYLE );
					SetClassLongPtr( hwnd, GCL_STYLE, cStyle | CS_NOCLOSE );
#endif
				}

				Window window = itor->second;

				window.workspace->setEnabled( true );
				camera = window.camera;
			}

			//Read camera parameters
			const float focalLength	= smartData.read<float>();
			const float sensorSize	= smartData.read<float>();
			const float nearClip	= smartData.read<float>();
			const float farClip		= smartData.read<float>();
			Ogre::Vector3 camPos	= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camUp		= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camRight	= smartData.read<Ogre::Vector3>();
			Ogre::Vector3 camForward= smartData.read<Ogre::Vector3>();
			bool isPerspective		= smartData.read<uint8_t>() != 0;

			if( !isPerspective )
			{
				float orthoWidth = camRight.length();
				float orthoHeight = camUp.length();
				camera->setOrthoWindow( orthoWidth, orthoHeight );
			}

			const float fov = 2.0f * atanf( 0.5f * sensorSize / focalLength );

			camera->setProjectionType( isPerspective ? Ogre::PT_PERSPECTIVE : Ogre::PT_ORTHOGRAPHIC );
			camera->setFOVy( Ogre::Radian(fov) );
			camera->setNearClipDistance( nearClip );
			camera->setFarClipDistance( farClip );

			camUp.normalise();
			camRight.normalise();
			camForward.normalise();

			camPos += camForward * nearClip * 2.0f;
			camera->setPosition( camPos );

			Ogre::Quaternion qRot( camRight, camUp, camForward );
			qRot.normalise();
			camera->setOrientation( qRot );

			if( m_irDirty && m_enableInstantRadiosity )
				rebuildInstantRadiosity();

			if( returnResult )
			{
				//update();
				mSceneManager->updateSceneGraph();
				mWorkspace->_beginUpdate( true );
				mWorkspace->_update();
				mWorkspace->_endUpdate( true );
				mSceneManager->clearFrameData();

				Ogre::CompositorNode *internalTextureNode = mWorkspace->findNode( "InternalTextureNode" );

				Ogre::TextureGpu *rtt = internalTextureNode->getDefinedTexture( "internalTexture" );

				Ogre::Image2 tmpImage;
				tmpImage.convertFromTexture( rtt, 0, 0, true );

				Network::SmartData toClient( 2 * sizeof(Ogre::uint16) + tmpImage.getSizeBytes() );
				toClient.write<uint16_t>( width );
				toClient.write<uint16_t>( height );
				memcpy( toClient.getCurrentPtr(), tmpImage.getRawBuffer(), tmpImage.getSizeBytes() );
				networkSystem.send( bev, Network::FromServer::Result,
									toClient.getBasePtr(), toClient.getCapacity() );
			}
			break;
		}
		case Network::FromClient::InitAsync:
		{
			WindowMap::const_iterator itor = m_renderWindows.begin();
			WindowMap::const_iterator end  = m_renderWindows.end();

			while( itor != end )
			{
				const Window &window = itor->second;
				window.workspace->setEnabled( false );
				++itor;
			}
			break;
		}
		case Network::FromClient::FinishAsync:
		{
			mWorkspace->setEnabled( false );

			WindowMap::const_iterator itor = m_renderWindows.begin();
			WindowMap::const_iterator end  = m_renderWindows.end();

			while( itor != end )
			{
				const Window &window = itor->second;
				if( window.workspace->getEnabled() != !window.renderWindow->isHidden() )
				{
					//Hide windows that we won't be updating, show those that are going to be updated
					window.renderWindow->setHidden( !window.workspace->getEnabled() );
				}
				++itor;
			}

			static size_t frame = 0;
			if( !(frame % 8) )
				update();
			++frame;
			break;
		}
		default:
			break;
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::allConnectionsTerminated()
	{
		reset();

		Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
		WindowMap::const_iterator itor = m_renderWindows.begin();
		WindowMap::const_iterator end  = m_renderWindows.end();

		while( itor != end )
		{
			const Window &window = itor->second;
			Ogre::WindowEventUtilities::removeWindowEventListener( window.renderWindow,
																   m_windowEventListener );
			compositorManager->removeWorkspace( window.workspace );
			mSceneManager->destroyCamera( window.camera );
			mRoot->getRenderSystem()->destroyRenderWindow( window.renderWindow );
			++itor;
		}

		m_renderWindows.clear();
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::savingChangeTextureName( Ogre::String &inOutTexName )
	{
		TexAliasToFullPathMap::const_iterator itor = m_textures.find( inOutTexName );
		if( itor != m_textures.end() )
		{
			inOutTexName = itor->second;
			//Turn absolute path into relative
			size_t pos = inOutTexName.find_last_of( "/\\" );
			if( pos != inOutTexName.npos )
				inOutTexName.erase( 0, pos + 1u );
		}
	}
	//-----------------------------------------------------------------------------------
	void DergoSystem::savingChangeTextureNameOriginal( const Ogre::String &aliasName,
													   Ogre::String &inOutResourceName,
													   Ogre::String &inOutFilename )
	{
		TexAliasToFullPathMap::const_iterator itor = m_textures.find( inOutFilename );
		if( itor != m_textures.end() )
		{
			inOutFilename = itor->second;
			//Turn absolute path into relative
			size_t pos = inOutFilename.find_last_of( "/\\" );
			if( pos != inOutFilename.npos )
				inOutFilename.erase( 0, pos + 1u );
		}
	}
}
