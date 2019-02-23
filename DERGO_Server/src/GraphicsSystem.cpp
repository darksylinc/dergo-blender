
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

#include "OgreTextureGpuManager.h"

#include "OgreWindowEventUtilities.h"
#include "OgreWindow.h"

#include "OgreFileSystemLayer.h"

#include "OgreHlmsDiskCache.h"
#include "OgreGpuProgramManager.h"
#include "Network/NetworkSystem.h"
#include "Network/NetworkMessage.h"
#include "Network/SmartData.h"

#include "OgreLogManager.h"

#if OGRE_USE_SDL2
    #include <SDL_syswm.h>
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE || OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
    #include "OSX/macUtils.h"
    #if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        #include "System/iOS/iOSUtils.h"
    #else
        #include "System/OSX/OSXUtils.h"
    #endif
#endif

namespace DERGO
{
    GraphicsSystem::GraphicsSystem( Ogre::ColourValue backgroundColour ) :
    #if OGRE_USE_SDL2
        mSdlWindow( 0 ),
        mInputHandler( 0 ),
    #endif
        mRoot( 0 ),
        mRenderWindow( 0 ),
        mSceneManager( 0 ),
        mCamera( 0 ),
        mWorkspace( 0 ),
        mPluginsFolder( "./" ),
        mQuit( false ),
        mAlwaysAskForConfig( true ),
        mUseHlmsDiskCache( true ),
        mUseMicrocodeCache( true ),
        mBackgroundColour( backgroundColour )
    {
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
        // Note:  macBundlePath works for iOS too. It's misnamed.
        mResourcePath = Ogre::macBundlePath() + "/Contents/Resources/";
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        mResourcePath = Ogre::macBundlePath() + "/";
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
        mPluginsFolder = mResourcePath;
#endif
        if( isWriteAccessFolder( mPluginsFolder, "Ogre.log" ) )
            mWriteAccessFolder = mPluginsFolder;
        else
        {
            Ogre::FileSystemLayer filesystemLayer( OGRE_VERSION_NAME );
            mWriteAccessFolder = filesystemLayer.getWritablePath( "" );
        }
    }
    //-----------------------------------------------------------------------------------
    GraphicsSystem::~GraphicsSystem()
    {
        if( mRoot )
        {
            Ogre::LogManager::getSingleton().logMessage(
                        "WARNING: GraphicsSystem::deinitialize() not called!!!", Ogre::LML_CRITICAL );
        }
    }
    //-----------------------------------------------------------------------------------
    bool GraphicsSystem::isWriteAccessFolder( const Ogre::String &folderPath,
                                              const Ogre::String &fileToSave )
    {
        if( !Ogre::FileSystemLayer::createDirectory( folderPath ) )
            return false;

        std::ofstream of( (folderPath + fileToSave).c_str(),
                          std::ios::out | std::ios::binary | std::ios::app );
        if( !of )
            return false;

        return true;
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::initialize()
    {
        Ogre::String pluginsPath;
        // only use plugins.cfg if not static
    #ifndef OGRE_STATIC_LIB
    #if OGRE_DEBUG_MODE && !((OGRE_PLATFORM == OGRE_PLATFORM_APPLE) || (OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS))
        pluginsPath = mPluginsFolder + "Plugins.cfg";
    #else
        pluginsPath = mPluginsFolder + "Plugins.cfg";
    #endif
    #endif

        mRoot = OGRE_NEW Ogre::Root( pluginsPath,
                                     mWriteAccessFolder + "ogre.cfg",
                                     mWriteAccessFolder + "Ogre.log" );

        //mStaticPluginLoader.install( mRoot );

        if( mAlwaysAskForConfig || !mRoot->restoreConfig() )
        {
            if( !mRoot->showConfigDialog() )
            {
                mQuit = true;
                return;
            }
        }

    #if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        {
            Ogre::RenderSystem *renderSystem =
                    mRoot->getRenderSystemByName( "Metal Rendering Subsystem" );
            mRoot->setRenderSystem( renderSystem );
        }
    #endif

        mRoot->getRenderSystem()->setConfigOption( "sRGB Gamma Conversion", "Yes" );
        mRoot->initialise(false);

        Ogre::ConfigOptionMap& cfgOpts = mRoot->getRenderSystem()->getConfigOptions();

        int width   = 1280;
        int height  = 720;

    #if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        {
            Ogre::Vector2 screenRes = iOSUtils::getScreenResolutionInPoints();
            width = static_cast<int>( screenRes.x );
            height = static_cast<int>( screenRes.y );
        }
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

#if OGRE_PROFILING
        Ogre::Profiler::getSingleton().setEnabled( true );
    #if OGRE_PROFILING == OGRE_PROFILING_INTERNAL
        Ogre::Profiler::getSingleton().endProfile( "" );
    #endif
    #if OGRE_PROFILING == OGRE_PROFILING_INTERNAL_OFFLINE
        Ogre::Profiler::getSingleton().getOfflineProfiler().setDumpPathsOnShutdown(
                    mWriteAccessFolder + "ProfilePerFrame",
                    mWriteAccessFolder + "ProfileAccum" );
    #endif
#endif
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::deinitialize(void)
    {
        saveTextureCache();
        saveHlmsDiskCache();

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
#if (OGRE_PLATFORM == OGRE_PLATFORM_APPLE) || (OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS)
        // OS X does not set the working directory relative to the app,
        // In order to make things portable on OS X we need to provide
        // the loading with it's own bundle path location
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    Ogre::String( Ogre::macBundlePath() + "/" + archName ), typeName, secName );
#else
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    archName, typeName, secName);
#endif
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::loadTextureCache(void)
    {
#if !OGRE_NO_JSON
        Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();
        Ogre::Archive *rwAccessFolderArchive = archiveManager.load( mWriteAccessFolder,
                                                                    "FileSystem", true );
        try
        {
            const Ogre::String filename = "textureMetadataCache.json";
            if( rwAccessFolderArchive->exists( filename ) )
            {
                Ogre::DataStreamPtr stream = rwAccessFolderArchive->open( filename );
                std::vector<char> fileData;
                fileData.resize( stream->size() + 1 );
                if( !fileData.empty() )
                {
                    stream->read( &fileData[0], stream->size() );
                    //Add null terminator just in case (to prevent bad input)
                    fileData.back() = '\0';
                    Ogre::TextureGpuManager *textureManager =
                            mRoot->getRenderSystem()->getTextureGpuManager();
                    textureManager->importTextureMetadataCache( stream->getName(), &fileData[0], false );
                }
            }
            else
            {
                Ogre::LogManager::getSingleton().logMessage(
                            "[INFO] Texture cache not found at " + mWriteAccessFolder +
                            "/textureMetadataCache.json" );
            }
        }
        catch( Ogre::Exception &e )
        {
            Ogre::LogManager::getSingleton().logMessage( e.getFullDescription() );
        }

        archiveManager.unload( rwAccessFolderArchive );
#endif
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::saveTextureCache(void)
    {
        if( mRoot->getRenderSystem() )
        {
            Ogre::TextureGpuManager *textureManager = mRoot->getRenderSystem()->getTextureGpuManager();
            if( textureManager )
            {
                Ogre::String jsonString;
                textureManager->exportTextureMetadataCache( jsonString );
                const Ogre::String path = mWriteAccessFolder + "/textureMetadataCache.json";
                std::ofstream file( path.c_str(), std::ios::binary | std::ios::out );
                if( file.is_open() )
                    file.write( jsonString.c_str(), static_cast<std::streamsize>( jsonString.size() ) );
                file.close();
            }
        }
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::loadHlmsDiskCache(void)
    {
        if( !mUseMicrocodeCache && !mUseHlmsDiskCache )
            return;

        Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
        Ogre::HlmsDiskCache diskCache( hlmsManager );

        Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();

        Ogre::Archive *rwAccessFolderArchive = archiveManager.load( mWriteAccessFolder,
                                                                    "FileSystem", true );

        if( mUseMicrocodeCache )
        {
            //Make sure the microcode cache is enabled.
            Ogre::GpuProgramManager::getSingleton().setSaveMicrocodesToCache( true );
            const Ogre::String filename = "microcodeCodeCache.cache";
            if( rwAccessFolderArchive->exists( filename ) )
            {
                Ogre::DataStreamPtr shaderCacheFile = rwAccessFolderArchive->open( filename );
                Ogre::GpuProgramManager::getSingleton().loadMicrocodeCache( shaderCacheFile );
            }
        }

        if( mUseHlmsDiskCache )
        {
            for( size_t i=Ogre::HLMS_LOW_LEVEL + 1u; i<Ogre::HLMS_MAX; ++i )
            {
                Ogre::Hlms *hlms = hlmsManager->getHlms( static_cast<Ogre::HlmsTypes>( i ) );
                if( hlms )
                {
                    Ogre::String filename = "hlmsDiskCache" +
                                            Ogre::StringConverter::toString( i ) + ".bin";

                    try
                    {
                        if( rwAccessFolderArchive->exists( filename ) )
                        {
                            Ogre::DataStreamPtr diskCacheFile = rwAccessFolderArchive->open( filename );
                            diskCache.loadFrom( diskCacheFile );
                            diskCache.applyTo( hlms );
                        }
                    }
                    catch( Ogre::Exception& )
                    {
                        Ogre::LogManager::getSingleton().logMessage(
                                    "Error loading cache from " + mWriteAccessFolder + "/" +
                                    filename + "! If you have issues, try deleting the file "
                                    "and restarting the app" );
                    }
                }
            }
        }

        archiveManager.unload( mWriteAccessFolder );
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::saveHlmsDiskCache(void)
    {
        if( mRoot->getRenderSystem() && Ogre::GpuProgramManager::getSingletonPtr() &&
            (mUseMicrocodeCache || mUseHlmsDiskCache) )
        {
            Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
            Ogre::HlmsDiskCache diskCache( hlmsManager );

            Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();

            Ogre::Archive *rwAccessFolderArchive = archiveManager.load( mWriteAccessFolder,
                                                                        "FileSystem", false );

            if( mUseHlmsDiskCache )
            {
                for( size_t i=Ogre::HLMS_LOW_LEVEL + 1u; i<Ogre::HLMS_MAX; ++i )
                {
                    Ogre::Hlms *hlms = hlmsManager->getHlms( static_cast<Ogre::HlmsTypes>( i ) );
                    if( hlms )
                    {
                        diskCache.copyFrom( hlms );

                        Ogre::DataStreamPtr diskCacheFile =
                                rwAccessFolderArchive->create( "hlmsDiskCache" +
                                                               Ogre::StringConverter::toString( i ) +
                                                               ".bin" );
                        diskCache.saveTo( diskCacheFile );
                    }
                }
            }

            if( Ogre::GpuProgramManager::getSingleton().isCacheDirty() && mUseMicrocodeCache )
            {
                const Ogre::String filename = "microcodeCodeCache.cache";
                Ogre::DataStreamPtr shaderCacheFile = rwAccessFolderArchive->create( filename );
                Ogre::GpuProgramManager::getSingleton().saveMicrocodeCache( shaderCacheFile );
            }

            archiveManager.unload( mWriteAccessFolder );
        }
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

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE || OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        Ogre::String rootHlmsFolder = Ogre::macBundlePath() + '/' +
                                  cf.getSetting( "DoNotUseAsResource", "Hlms", "" );
#else
        Ogre::String rootHlmsFolder = mResourcePath + cf.getSetting( "DoNotUseAsResource", "Hlms", "" );
#endif

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

        loadTextureCache();
        loadHlmsDiskCache();

        // Initialise, parse scripts etc
        Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups( true );
    }
    //-----------------------------------------------------------------------------------
    void GraphicsSystem::chooseSceneManager(void)
    {
#if OGRE_DEBUG_MODE
        //Debugging multithreaded code is a PITA, disable it.
        const size_t numThreads = 1;
#else
        //getNumLogicalCores() may return 0 if couldn't detect
        const size_t numThreads = std::max<size_t>( 1, Ogre::PlatformInformation::getNumLogicalCores() );
#endif
        // Create the SceneManager, in this case a generic one
        mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC,
                                                   numThreads,
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

        return compositorManager->addWorkspace( mSceneManager, mRenderWindow->getTexture(), mCamera,
                                                workspaceName, true );
    }
	//-----------------------------------------------------------------------------------
	void GraphicsSystem::stopCompositor(void)
	{
		if( mWorkspace )
		{
			Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
			compositorManager->removeWorkspace( mWorkspace );
			mWorkspace = 0;
		}
	}
	//-----------------------------------------------------------------------------------
	void GraphicsSystem::restartCompositor(void)
	{
		stopCompositor();
		mWorkspace = setupCompositor();
	}
    //-----------------------------------------------------------------------------------
}
