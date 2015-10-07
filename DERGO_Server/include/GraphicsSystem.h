
#pragma once

#include "OgrePrerequisites.h"
#include "OgreColourValue.h"

#include "Network/NetworkListener.h"

namespace Network
{
	struct MessageHeader;
	class SmartData;
}

struct bufferevent;

namespace DERGO
{
	class GraphicsSystem : public NetworkListener
    {
    protected:
        Ogre::Root                  *mRoot;
        Ogre::RenderWindow          *mRenderWindow;
        Ogre::SceneManager          *mSceneManager;
        Ogre::Camera                *mCamera;
        Ogre::CompositorWorkspace   *mWorkspace;
        Ogre::String                mResourcePath;

		bool                mQuit;

        Ogre::ColourValue   mBackgroundColour;

        static void addResourceLocation( const Ogre::String &archName, const Ogre::String &typeName,
                                         const Ogre::String &secName );
        virtual void setupResources(void);
        virtual void registerHlms(void);
        /// Optional override method where you can perform resource group loading
        /// Must at least do ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
        virtual void loadResources(void);
        virtual void chooseSceneManager(void);
        virtual void createCamera(void);
        /// Virtual so that advanced samples such as Sample_Compositor can override this
        /// method to change the default behavior if setupCompositor() is overridden, be
        /// aware @mBackgroundColour will be ignored
        virtual Ogre::CompositorWorkspace* setupCompositor(void);

        /// Optional override method where you can create resource listeners (e.g. for loading screens)
        virtual void createResourceListener(void) {}

    public:
		GraphicsSystem( Ogre::ColourValue backgroundColour = Ogre::ColourValue( 0.2f, 0.4f, 0.6f ) );
        virtual ~GraphicsSystem();

		void initialize();
        void deinitialize(void);

		virtual void processMessage( const Network::MessageHeader &header, Network::SmartData &smartData,
									 bufferevent *bev, NetworkSystem &networkSystem );

		void update();

        bool getQuit(void) const                                { return mQuit; }

        Ogre::Root* getRoot(void) const                         { return mRoot; }
        Ogre::RenderWindow* getRenderWindow(void) const         { return mRenderWindow; }
        Ogre::SceneManager* getSceneManager(void) const         { return mSceneManager; }
        Ogre::Camera* getCamera(void) const                     { return mCamera; }
		Ogre::CompositorWorkspace* getCompositorWorkspace(void) const { return mWorkspace; }
    };
}
