
#include "OgrePrerequisites.h"

namespace Ogre
{
	class HlmsPbs;
	struct IdString;
}

namespace DERGO
{
	class ShadowsUtils
    {
	public:
		struct Settings
		{
			bool enabled;
			Ogre::uint32 width;
			Ogre::uint32 height;
			Ogre::uint8 numLights;
			bool usePssm;
			Ogre::uint8 numSplits;
			Ogre::uint8 filtering;
			Ogre::uint32 pointRes;
			float pssmLambda;
			float pssmSplitPadding;
			float pssmSplitBlend;
			float pssmSplitFade;
			float maxDistance;
		};

		static void tagAllNodesUsingShadowNodes( Ogre::CompositorManager2 *compositorManager );

		static void setAllPassSceneToShadowNode( Ogre::CompositorManager2 *compositorManager,
												 Ogre::IdString shadowNodeName );

		static void applyShadowsSettings( const Settings &shadowSettings,
										  Ogre::CompositorManager2 *compositorManager,
										  Ogre::RenderSystem *renderSystem,
										  Ogre::SceneManager *sceneManager,
										  Ogre::HlmsManager *hlmsManager,
										  Ogre::HlmsPbs *hlmsPbs );
    };
}
