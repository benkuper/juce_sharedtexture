#if JUCE_WINDOWS
#include "spout/Spout.h"
#include "SharedTexture.h"
#endif

SharedTextureSender::SharedTextureSender(const String& name, int width, int height, bool enabled) :
	isInit(false),
	sharingName(name),
	enabled(enabled),
	fbo(nullptr),
	width(width),
	height(height)
{

#if JUCE_WINDOWS
	spoutSender.reset(new SpoutSender());
#elif JUCE_MAC

#endif
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

	if (fbo != nullptr) fbo->release();

	if (enabled)
	{
		image = Image(Image::ARGB, width, height, true, OpenGLImageType()); //create the openGL image
		fbo = OpenGLImageType::getFrameBufferFrom(image);
	}

	setupNativeSender();
}

void SharedTextureSender::setupNativeSender()
{
	if (enabled)
	{
#if JUCE_WINDOWS
		if (isInit) spoutSender->UpdateSender(sharingName.getCharPointer(), image.getWidth(), image.getHeight());
		else
		{
			spoutSender->CreateSender(sharingName.getCharPointer(), image.getWidth(), image.getHeight());
			isInit = true;
		}
#elif JUCE_MAC

#endif
	}
	else
	{
#if JUCE_WINDOWS
		if (isInit) spoutSender->ReleaseSender();
#elif JUCE_MAC

#endif
		isInit = false;
	}


}

void SharedTextureSender::initGL()
{
	createImageDefinition();
}

void SharedTextureSender::renderGL()
{
	if (!enabled)
	{
		if (isInit) setupNativeSender();
		return;
	}

	if (!isInit || !image.isValid() || image.getWidth() != width || image.getHeight() != height) createImageDefinition();
	if (!image.isValid())
	{
		DBG("Problem creating image");
		return;
	}

	juce::Rectangle<int> r = image.getBounds();
	image.clear(r);
	Graphics g(image);
	g.beginTransparencyLayer(1);
	sharedTextureListeners.call(&SharedTextureListener::drawSharedTexture, g, r);
	g.endTransparencyLayer();

#if JUCE_WINDOWS
	spoutSender->SendTexture(fbo->getTextureID(), GL_TEXTURE_2D, image.getWidth(), image.getHeight());
#elif JUCE_MAC

#endif
}

void SharedTextureSender::clearGL()
{
	if (fbo != nullptr) fbo->release();

#if JUCE_WINDOWS
	spoutSender->ReleaseSender();
#elif JUCE_MAC
#endif

	isInit = false;
}

void SharedTextureSender::setEnabled(bool value)
{
	enabled = value;
}


//REC

SharedTextureReceiver::SharedTextureReceiver(const String& _sharingName) :
#if JUCE_WINDOWS
	receiver(nullptr),
#endif
	enabled(true),
	sharingName(_sharingName),
	isInit(false),
	isConnected(false),
	invertImage(true),
	fbo(nullptr),
	useCPUImage(false)
{


#if JUCE_WINDOWS
	sharingName.copyToUTF8(sharingNameArr, 256);
	receiver = new SpoutReceiver();
#elif JUCE_MAC

#endif

}

SharedTextureReceiver::~SharedTextureReceiver()
{
#if JUCE_WINDOWS
	receiver = nullptr;
#endif
}

void SharedTextureReceiver::setConnected(bool value)
{
	if (isConnected == value) return;
	isConnected = value;
	listeners.call(&Listener::connectionChanged, this);
}

void SharedTextureReceiver::setUseCPUImage(bool value)
{
	if (useCPUImage == value) return;
	useCPUImage = value;
	if (!useCPUImage) outImage = Image();
}

Image& SharedTextureReceiver::getImage()
{
	return useCPUImage ? outImage : image;
}

bool SharedTextureReceiver::canDraw()
{
	return getImage().isValid() && OpenGLContext::getCurrentContext() != nullptr;
}

void SharedTextureReceiver::createReceiver()
{
#if JUCE_WINDOWS
	if (!isInit)	isInit = receiver->CreateReceiver(sharingNameArr, width, height, sharingName.isEmpty());
#elif JUCE_MAC

#endif

	if (!isInit) return;
}

void SharedTextureReceiver::createImageDefinition()
{
	if (width == 0 || height == 0) return;
	if (OpenGLContext::getCurrentContext() == nullptr) return;
	if (!OpenGLContext::getCurrentContext()->isActive()) return;

	//if (fbo != nullptr) fbo->release();
	image = Image(Image::ARGB, width, height, true, OpenGLImageType()); //create the openGL image
	outImage = Image(Image::ARGB, width, height, true); //not gl to be able to manipulate
	fbo = OpenGLImageType::getFrameBufferFrom(image);
}

void SharedTextureReceiver::initGL()
{
	createImageDefinition();
}

void SharedTextureReceiver::renderGL()
{

	if (!enabled) return;
	if (!isInit) createReceiver();
	if (!isInit) return;


	unsigned int newWidth = 0, newHeight = 0;

#if JUCE_WINDOWS
	bool connectionResult;
	receiver->CheckReceiver(sharingNameArr, newWidth, newHeight, connectionResult);

	setConnected(connectionResult);
	if (!isConnected) return;

	if (!image.isValid() || width != newWidth || height != newHeight) createImageDefinition();
	if (!image.isValid()) return;

#elif JUCE_MAC

#endif

	if (!image.isValid() || width != newWidth || height != newHeight) createImageDefinition();
	if (!image.isValid()) return;


	bool success = true;

#if JUCE_WINDOWS
	unsigned int receiveWidth = width, receiveHeight = height;
	success = receiver->ReceiveTexture(sharingNameArr, receiveWidth, receiveHeight, fbo->getTextureID(), GL_TEXTURE_2D, invertImage);
#elif JUCE_MAC

#endif

	if (success && useCPUImage)
	{
		if (!outImage.isValid()) outImage = Image(Image::ARGB, width, height, true); //not gl to be able to manipulate

		outImage.clear(outImage.getBounds());
		Graphics g(outImage);
		g.drawImage(image, outImage.getBounds().toFloat());
	}

	listeners.call(&Listener::textureUpdated, this);
}

void SharedTextureReceiver::clearGL()
{
}




juce_ImplementSingleton(SharedTextureManager)

SharedTextureManager::SharedTextureManager()
{
}

SharedTextureManager::~SharedTextureManager()
{
	while (senders.size() > 0) removeSender(senders[0]);
	while (receivers.size() > 0) removeReceiver(receivers[0]);
	sendersMap.clear();
	receiversMap.clear();
}

SharedTextureSender* SharedTextureManager::addSender(const String& name, int width, int height, bool enabled)
{
	if (sendersMap[name] != nullptr) return sendersMap[name];
	SharedTextureSender* s = new SharedTextureSender(name, width, height, enabled);
	senders.add(s);
	sendersMap.set(name, s);
	return s;
}

SharedTextureReceiver* SharedTextureManager::addReceiver(const String& name)
{
	if (receiversMap[name] != nullptr) return receiversMap[name];
	SharedTextureReceiver* r = new SharedTextureReceiver(name);
	receivers.add(r);
	receiversMap.set(name, r);
	return r;
}

void SharedTextureManager::removeSender(SharedTextureSender* sender)
{
	sendersMap.removeValue(sender);
	senders.removeObject(sender, true);
}

void SharedTextureManager::removeReceiver(SharedTextureReceiver* receiver)
{
	receiversMap.removeValue(receiver);
	receivers.removeObject(receiver, true);
}

void SharedTextureManager::initGL()
{
	for (auto& s : senders) s->initGL();
	for (auto& r : receivers) r->initGL();
}

void SharedTextureManager::renderGL()
{
	for (auto& s : senders) s->renderGL();
	for (auto& r : receivers) r->renderGL();
}

void SharedTextureManager::clearGL()
{
	for (auto& s : senders) s->clearGL();
	for (auto& r : receivers) r->clearGL();
}
