#if JUCE_WINDOWS
#include "spout/Spout.h"
#include "SharedTexture.h"
#endif

SharedTextureSender::SharedTextureSender(const String &name) :
	sharingName(name),
	enabled(true),
	isInit(false),
	fbo(nullptr),
	width(0),height(0)
{

#if JUCE_WINDOWS
	spoutSender = new SpoutSender();
#elif JUCE_MAC

#endif

	setSize(512, 512);
}

SharedTextureSender::~SharedTextureSender()
{

}

bool SharedTextureSender::canDraw()
{
	return OpenGLContext::getCurrentContext() != nullptr && image.isValid();
}

void SharedTextureSender::setSize(int w, int h)
{
	width = w;
	height = h;
}

void SharedTextureSender::createImageDefinition()
{
	if (width == 0 || height == 0) return;

	if(fbo != nullptr) fbo->release();
	image = Image(Image::ARGB, width, height, true, OpenGLImageType()); //create the openGL image
	fbo = OpenGLImageType::getFrameBufferFrom(image);

#if JUCE_WINDOWS
	if(isInit) spoutSender->UpdateSender(sharingName.getCharPointer(), image.getWidth(),image.getHeight());
	else
	{
		spoutSender->CreateSender(sharingName.getCharPointer(), image.getWidth(), image.getHeight());
		isInit = true;
	}
#elif JUCE_MAC

#endif
}

void SharedTextureSender::sendTexture()
{
	if (!image.isValid()) return;

#if JUCE_WINDOWS
	spoutSender->SendTexture(fbo->getTextureID(), GL_TEXTURE_2D, image.getWidth(),image.getHeight());
#elif JUCE_MAC

#endif
}

void SharedTextureSender::renderGL()
{
	if (!enabled) return;

	if (!image.isValid() || image.getWidth() != width || image.getHeight() != height) createImageDefinition();
	if (!image.isValid())
	{
		DBG("Problem creating image");
		return;
	}

	juce::Rectangle<int> r = image.getBounds();
	image.clear(r);
	Graphics g(image);
	g.beginTransparencyLayer(1);
	listeners.call(&Listener::drawSharedTexture, g,r);
	g.endTransparencyLayer();
	sendTexture();
}


//REC

SharedTextureReceiver::SharedTextureReceiver(const String &_sharingName) :
	sharingName(_sharingName),
	isInit(false),
	enabled(true),
	fbo(nullptr),
	receiver(nullptr)
{

	if (sharingName.isEmpty()) sharingName = "Whatever";
	sharingName.copyToUTF8(sharingNameArr, 256);

#if JUCE_WINDOWS
	receiver = new SpoutReceiver();
#elif JUCE_MAC

#endif
}

SharedTextureReceiver::~SharedTextureReceiver()
{
	receiver = nullptr;
}

bool SharedTextureReceiver::canDraw()
{
	return image.isValid() && OpenGLContext::getCurrentContext() != nullptr;
}

void SharedTextureReceiver::createReceiver()
{
#if JUCE_WINDOWS
	if(!isInit)	isInit = receiver->CreateReceiver(sharingNameArr, width, height,sharingName.isEmpty());
#elif JUCE_MAC

#endif

	if (!isInit) return;
}

void SharedTextureReceiver::createImageDefinition()
{
	if (width == 0 || height == 0) return;
	if (OpenGLContext::getCurrentContext() == nullptr) return;
	if (!OpenGLContext::getCurrentContext()->isActive()) return;
	
	if (fbo != nullptr) fbo->release();
	image = Image(Image::ARGB, width, height, true, OpenGLImageType()); //create the openGL image
	fbo = OpenGLImageType::getFrameBufferFrom(image);
}

void SharedTextureReceiver::renderGL()
{
	if (!enabled) return;
	if (!isInit) createReceiver();
	if (!isInit) return;

	unsigned int newWidth = 0, newHeight = 0;
	bool isConnected;
	receiver->CheckReceiver(sharingNameArr, newWidth, newHeight, isConnected);

	if (!isConnected) return;

	if (!image.isValid() || width != newWidth || height != newHeight) createImageDefinition();
	if (!image.isValid()) return;

	unsigned int receiveWidth = width, receiveHeight = height;

	bool result = receiver->ReceiveTexture(sharingNameArr, receiveWidth, receiveHeight, fbo->getTextureID(),GL_TEXTURE_2D);
	
}

/*
void SharedTextureReceiver::paint(Graphics & g)
{
	DBG("repaint");
	g.fillAll(Colours::orange);
	
	if (outImage.isValid())
	{
		DBG("draw");
			g.drawImage(outImage, getLocalBounds().toFloat());
	}
}

void SharedTextureReceiver::newOpenGLContextCreated()
{
	
	if (receiverIsCreated) return;
	unsigned int w = 0, h = 0;
	receiver = new SpoutReceiver(); 
	receiverIsCreated = receiver->CreateReceiver(sharingName.getCharPointer().getAddress(), w, h, true);

	if (receiverIsCreated)
	{
		if (!image.isValid()) image = Image(Image::ARGB, w, h, true, OpenGLImageType()); //create the openGL image

		buffer.initialise(context, image);
		buffer.makeCurrentAndClear();
		buffer.clear(Colours::yellow);
		buffer.release();
	}
	repaint();
}


void SharedTextureReceiver::renderOpenGL()
{
	unsigned int w, h;
	//buffer.makeCurrentRenderingTarget();
	//bool result = receiver->ReceiveTexture(sharingName.getCharPointer().getAddress(), w, h, buffer.getTextureID(),buffer.getCurrentFrameBufferTarget(),false,buffer.getFrameBufferID());
	//buffer.saveAndRelease();

	//DBG("Result : " << (int)result << "/" << (int)buffer.width << "/" << image.width);

	if (!outImage.isValid())
	{
		outImage = Image(Image::ARGB, image.width, image.height, true);
	} 

	Graphics g(outImage);
	g.fillAll(Colours::green);
	g.drawImage(image, outImage.getBounds().toFloat());

}
*/

juce_ImplementSingleton(SharedTextureManager)

SharedTextureManager::SharedTextureManager()
{
}

SharedTextureManager::~SharedTextureManager()
{
}


SharedTextureSender * SharedTextureManager::addSender(const String & name)
{
	if (sendersMap[name] != nullptr) return sendersMap[name];
	SharedTextureSender * s = new SharedTextureSender(name);
	senders.add(s);
	sendersMap.set(name, s);
	return s;
}

SharedTextureReceiver * SharedTextureManager::addReceiver(const String & name)
{
	if (receiversMap[name] != nullptr) return receiversMap[name];
	SharedTextureReceiver * r = new SharedTextureReceiver(name);
	receivers.add(r);
	receiversMap.set(name, r);
	return r;
}

void SharedTextureManager::renderGL()
{
	for (auto &s : senders) s->renderGL();
	for (auto &r : receivers) r->renderGL();
}
