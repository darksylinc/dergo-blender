
#include "System/WindowEventListener.h"

#include "OgreWindow.h"

namespace DERGO
{
	bool WindowEventListener::windowClosing( Ogre::Window* renderWindow )
	{
		return false;
	}
}
