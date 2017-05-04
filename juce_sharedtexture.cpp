/*
  ==============================================================================

   

  ==============================================================================
*/

#ifdef JUCE_SHAREDTEXTURE_UI_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif

#include "juce_sharedtexture.h"

#include "spout/SpoutCopy.cpp"
#include "spout/SpoutDirectX.cpp"
#include "spout/SpoutGLDXinterop.cpp"
#include "spout/SpoutGLextensions.cpp"
#include "spout/SpoutMemoryShare.cpp"
#include "spout/SpoutReceiver.cpp"
#include "spout/SpoutSDK.cpp"
#include "spout/SpoutSender.cpp"
#include "spout/SpoutSenderNames.cpp"
#include "spout/SpoutSharedMemory.cpp"

#include "SharedTexture.cpp"



