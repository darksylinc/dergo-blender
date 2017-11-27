
#include "GraphicsSystem.h"

#include "OgreRoot.h"
#include "OgreException.h"
#include "OgreConfigFile.h"

#include "OgreRenderWindow.h"
#include "OgreCamera.h"
#include "OgreItem.h"

#include "OgreHlmsUnlit.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsManager.h"
#include "OgreArchiveManager.h"

#include "Compositor/OgreCompositorManager2.h"

#include "OgreWindowEventUtilities.h"

#include "Network/NetworkSystem.h"
#include "Network/NetworkMessage.h"
#include "Network/SmartData.h"

namespace DERGO
{
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>

// This function will locate the path to our application on OS X,
// unlike windows you can not rely on the curent working directory
// for locating your configuration files and resources.
std::string macBundlePath()
{
    char path[1024];
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    assert(mainBundle);

    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
    assert(mainBundleURL);

    CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
    assert(cfStringRef);

    CFStringGetCString(cfStringRef, path, 1024, kCFStringEncodingASCII);

    CFRelease(mainBundleURL);
    CFRelease(cfStringRef);

    return std::string(path);
}
#endif

	GraphicsSystem::GraphicsSystem( Ogre::ColourValue backgroundColour ) :
        mRoot( 0 ),
        mRenderWindow( 0 ),
        mSceneManager( 0 ),
        mCamera( 0 ),
        mWorkspace( 0 ),
        mQuit( false ),
        mBackgroundColour( backgroundColour )
    {
    }
    //-----------------------------------------------------------------------------------
    GraphicsSystem::~GraphicsSystem()
    {
        assert( !mRoot && "deinitialize() not called!!!" );
    }
    //-----------------------------------------------------------------------------------
	void GraphicsSystem::initialize()
	{
        Ogre::String pluginsPath;
        // only use plugins.cfg if not static
#ifndef OGRE_STATIC_LIB
    #if OGRE_DEBUG_MODE
		pluginsPath = mResourcePath + "Plugins.cfg";
    #else
		pluginsPath = mResourcePath + "Plugins.cfg";
    #endif
#endif

        mRoot = OGRE_NEW Ogre::Root( pluginsPath,
                                     mResourcePath + "ogre.cfg",
                                     mResourcePath + "Ogre.log" );

		if( !mRoot->restoreConfig() )
        {
            if( !mRoot->showConfigDialog() )
            {
                mQuit = true;
                return;
            }
        }

#if OGRE_PLATFORM != OGRE_PLATFORM_LINUX
		int width   = 1;
		int height  = 1;
#else
		//TODO: This workaround will be here until we fix Mesa + MultWindow
		int width   = 1280;
		int height  = 720;
#endif

        mRoot->getRenderSystem()->setConfigOption( "sRGB Gamma Conversion", "Yes" );
		mRoot->getRenderSystem()->setConfigOption( "Video Mode",
												   Ogre::StringConverter::toString( width ) + " x " +
												   Ogre::StringConverter::toString( height ) );
		mRoot->getRenderSystem()->setConfigOption( "Full Screen", "No" );
		mRoot->getRenderSystem()->setConfigOption( "VSync", "No" );
		mRenderWindow = mRoot->initialise( true, "DERGO Server - Hidden API-mandatory Render Window" );

#if OGRE_PLATFORM != OGRE_PLATFORM_LINUX
		//TODO: This workaround will be here until we fix Mesa + MultWindow
		mRenderWindow->setHidden( true );
#endif

        setupResources();
        loadResources();
        chooseSceneManager();
        createCamera();
        mWorkspace = setupCompositor();
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::deinitialize(void)
	{
        OGRE_DELETE mRoot;
        mRoot = 0;
	}
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::update()
    {
        Ogre::WindowEventUtilities::messagePump();

		//TODO: Check to set mQuit to false?
		//The dummy 1x1 mRenderWindow is by definition, always invisible.
		//if( mRenderWindow->isVisible() )
            mQuit |= !mRoot->renderOneFrame();
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::addResourceLocation( const Ogre::String &archName, const Ogre::String &typeName,
                                              const Ogre::String &secName )
    {
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
        // OS X does not set the working directory relative to the app,
        // In order to make things portable on OS X we need to provide
        // the loading with it's own bundle path location
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    Ogre::String(macBundlePath() + "/" + archName), typeName, secName);
#else
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    archName, typeName, secName);
#endif
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::setupResources(void)
    {
        // Load resource paths from config file
        Ogre::ConfigFile cf;
		cf.load(mResourcePath + "../Data/Resources.cfg");

        // Go through all sections & settings in the file
        Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

        Ogre::String secName, typeName, archName;
        while( seci.hasMoreElements() )
        {
            secName = seci.peekNextKey();
            Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();

            if( secName != "Hlms" )
            {
                Ogre::ConfigFile::SettingsMultiMap::iterator i;
                for (i = settings->begin(); i != settings->end(); ++i)
                {
                    typeName = i->first;
                    archName = i->second;
                    addResourceLocation( archName, typeName, secName );
                }
            }
        }
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::registerHlms(void)
    {
        Ogre::ConfigFile cf;
		cf.load(mResourcePath + "../Data/Resources.cfg");

		Ogre::String rootHlmsFolder = cf.getSetting( "DoNotUseAsResource", "Hlms", "" );

		if( rootHlmsFolder.empty() )
			rootHlmsFolder = "./";
		else if( *(rootHlmsFolder.end() - 1) != '/' )
			rootHlmsFolder += "/";

		//At this point rootHlmsFolder should be a valid path to the Hlms data folder

		Ogre::HlmsUnlit *hlmsUnlit = 0;
		Ogre::HlmsPbs *hlmsPbs = 0;

		//For retrieval of the paths to the different folders needed
		Ogre::String mainFolderPath;
		Ogre::StringVector libraryFoldersPaths;
		Ogre::StringVector::const_iterator libraryFolderPathIt;
		Ogre::StringVector::const_iterator libraryFolderPathEn;

		Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();

		{
			//Create & Register HlmsUnlit
			//Get the path to all the subdirectories used by HlmsUnlit
			Ogre::HlmsUnlit::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
			Ogre::Archive *archiveUnlit = archiveManager.load( rootHlmsFolder + mainFolderPath,
															   "FileSystem", true );
			Ogre::ArchiveVec archiveUnlitLibraryFolders;
			libraryFolderPathIt = libraryFoldersPaths.begin();
			libraryFolderPathEn = libraryFoldersPaths.end();
			while( libraryFolderPathIt != libraryFolderPathEn )
			{
				Ogre::Archive *archiveLibrary =
						archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
				archiveUnlitLibraryFolders.push_back( archiveLibrary );
				++libraryFolderPathIt;
			}

			//Create and register the unlit Hlms
			hlmsUnlit = OGRE_NEW Ogre::HlmsUnlit( archiveUnlit, &archiveUnlitLibraryFolders );
			Ogre::Root::getSingleton().getHlmsManager()->registerHlms( hlmsUnlit );
		}

		{
			//Create & Register HlmsPbs
			//Do the same for HlmsPbs:
			Ogre::HlmsPbs::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
			Ogre::Archive *archivePbs = archiveManager.load( rootHlmsFolder + mainFolderPath,
															 "FileSystem", true );

			//Get the library archive(s)
			Ogre::ArchiveVec archivePbsLibraryFolders;
			libraryFolderPathIt = libraryFoldersPaths.begin();
			libraryFolderPathEn = libraryFoldersPaths.end();
			while( libraryFolderPathIt != libraryFolderPathEn )
			{
				Ogre::Archive *archiveLibrary =
						archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
				archivePbsLibraryFolders.push_back( archiveLibrary );
				++libraryFolderPathIt;
			}

			//Create and register
			hlmsPbs = OGRE_NEW Ogre::HlmsPbs( archivePbs, &archivePbsLibraryFolders );
			Ogre::Root::getSingleton().getHlmsManager()->registerHlms( hlmsPbs );
		}


		Ogre::RenderSystem *renderSystem = mRoot->getRenderSystem();
		if( renderSystem->getName() == "Direct3D11 Rendering Subsystem" )
		{
			//Set lower limits 512kb instead of the default 4MB per Hlms in D3D 11.0
			//and below to avoid saturating AMD's discard limit (8MB) or
			//saturate the PCIE bus in some low end machines.
			bool supportsNoOverwriteOnTextureBuffers;
			renderSystem->getCustomAttribute( "MapNoOverwriteOnDynamicBufferSRV",
											  &supportsNoOverwriteOnTextureBuffers );

			if( !supportsNoOverwriteOnTextureBuffers )
			{
				hlmsPbs->setTextureBufferDefaultSize( 512 * 1024 );
				hlmsUnlit->setTextureBufferDefaultSize( 512 * 1024 );
			}
		}
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::loadResources(void)
    {
        registerHlms();

        // Initialise, parse scripts etc
        Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups( true );
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::chooseSceneManager(void)
    {
        Ogre::InstancingThreadedCullingMethod threadedCullingMethod =
                Ogre::INSTANCING_CULLING_SINGLETHREAD;
#if OGRE_DEBUG_MODE
        //Debugging multithreaded code is a PITA, disable it.
        const size_t numThreads = 1;
#else
        //getNumLogicalCores() may return 0 if couldn't detect
        const size_t numThreads = std::max<size_t>( 1, Ogre::PlatformInformation::getNumLogicalCores() );
        //See doxygen documentation regarding culling methods.
        //In some cases you may still want to use single thread.
        //if( numThreads > 1 )
        //	threadedCullingMethod = Ogre::INSTANCING_CULLING_THREADED;
#endif
        // Create the SceneManager, in this case a generic one
        mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC,
                                                   numThreads,
                                                   threadedCullingMethod,
                                                   "ExampleSMInstance" );

        //Set sane defaults for proper shadow mapping
        mSceneManager->setShadowDirectionalLightExtrusionDistance( 500.0f );
        mSceneManager->setShadowFarDistance( 500.0f );
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::createCamera(void)
    {
        mCamera = mSceneManager->createCamera( "Main Camera" );

        // Position it at 500 in Z direction
        mCamera->setPosition( Ogre::Vector3( 0, 5, 15 ) );
        // Look back along -Z
        mCamera->lookAt( Ogre::Vector3( 0, 0, 0 ) );
        mCamera->setNearClipDistance( 0.2f );
        mCamera->setFarClipDistance( 1000.0f );
        mCamera->setAutoAspectRatio( true );
    }
    //-----------------------------------------------------------------------------------
    Ogre::CompositorWorkspace* GraphicsSystem::setupCompositor(void)
    {
        Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();

		//const Ogre::String workspaceName( "DERGO Workspace" );
		const Ogre::String workspaceName( "DergoHdrWorkspace" );
        if( !compositorManager->hasWorkspaceDefinition( workspaceName ) )
        {
            compositorManager->createBasicWorkspaceDef( workspaceName, mBackgroundColour,
                                                        Ogre::IdString() );
        }

        return compositorManager->addWorkspace( mSceneManager, mRenderWindow, mCamera,
                                                workspaceName, true );
    }
    //-----------------------------------------------------------------------------------
}
