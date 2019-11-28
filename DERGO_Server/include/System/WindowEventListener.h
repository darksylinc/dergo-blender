
#pragma once

#include "OgreWindowEventUtilities.h"

namespace DERGO
{
	class WindowEventListener : public Ogre::WindowEventListener
	{
		virtual bool windowClosing( Ogre::Window *renderWindow );
	};
}  // namespace DERGO
