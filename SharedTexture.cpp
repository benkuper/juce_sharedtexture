
SharedTextureSender::SharedTextureSender(const String& name, int width, int height, bool enabled) :
	isInit(false),
	sharingName(name),
	sharingNameChanged(false),
	enabled(enabled),
	fbo(nullptr),
	width(width),
	height(height)
{

#if JUCE_WINDOWS
	spoutSender = GetSpout();
#elif JUCE_MAC

#endif
}

SharedTextureSender::~SharedTextureSender()
{
#if JUCE_WINDOWS
	spoutSender->ReleaseSender();
	spoutSender->Release();
	spoutSender = nullptr;
#endif
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
		//MessageManager::callAsync([&] {

		image = Image(Image::ARGB, width, height, true, OpenGLImageType()); //create the openGL image
		fbo = OpenGLImageType::getFrameBufferFrom(image);
		//});
	}

	setupNativeSender();
}

void SharedTextureSender::setupNativeSender(bool forceRecreation)
{
	if (enabled)
	{
#if JUCE_WINDOWS
		if (isInit && !forceRecreation) spoutSender->UpdateSender(sharingName.getCharPointer(), image.getWidth(), image.getHeight());
		else
		{
			if (isInit) spoutSender->ReleaseSender();
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

	sharingNameChanged = false;
}

void SharedTextureSender::initGL()
{
	//createImageDefinition();
}

void SharedTextureSender::renderGL()
{
	if (!enabled)
	{
		if (isInit) setupNativeSender();
		return;
	}

	if (sharingNameChanged)
	{
		setupNativeSender(true);
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
	spoutSender->SendTexture(fbo->getTextureID(), juce::gl::GL_TEXTURE_2D, image.getWidth(), image.getHeight());
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

void SharedTextureSender::setSharingName(String value)
{
	if (sharingName == value) return;
	sharingName = value;
	sharingNameChanged = true;
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
	width(0),
	height(0),
    invertImage(true),
    fbo(nullptr),
    useCPUImage(false)
{

#if JUCE_WINDOWS

#elif JUCE_MAC

#endif

}

SharedTextureReceiver::~SharedTextureReceiver()
{
#if JUCE_WINDOWS
	if (receiver != nullptr)
	{
		receiver->ReleaseReceiver();
		receiver->Release();
	}
	receiver = nullptr;
#endif
}

void SharedTextureReceiver::setSharingName(const String& name)
{
	if (name == sharingName) return;
	sharingName = name;
#if JUCE_WINDOWS
	receiver->SetReceiverName(sharingName.toStdString().c_str());
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
	if (!isInit)
	{
		receiver = GetSpout();
		receiver->SetReceiverName(sharingName.toStdString().c_str());

		//DBG("Set Sender Name : " << sharingName << " : " << receiver->GetSenderName());
		createImageDefinition();
		isInit = true;
	}
#elif JUCE_MAC

#endif

	if (!isInit) return;
}


void SharedTextureReceiver::createImageDefinition()
{
	if (OpenGLContext::getCurrentContext() == nullptr) return;
	if (!OpenGLContext::getCurrentContext()->isActive()) return;

#if JUCE_WINDOWS
	if (receiver == nullptr) return;

	width = jmax<int>(receiver->GetSenderWidth(), 1);
	height = jmax<int>(receiver->GetSenderHeight(), 1);
#endif


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
	if (!isInit)
	{
		createReceiver();
		return;
	}


	bool success = true;

#if JUCE_WINDOWS
	//unsigned int receiveWidth = width, receiveHeight = height;

	success = receiver->ReceiveTexture(fbo->getTextureID(), juce::gl::GL_TEXTURE_2D, invertImage);
	//DBG("Receiver Texture : " << (int)success << " / Get Sender Name [" << sharingName << "] : " << receiver->GetSenderName() << " ( " << (int)receiver->GetSenderWidth() << "x" << (int)receiver->GetSenderHeight() << ")");

	if (success)
	{
		if (receiver->IsUpdated()) createImageDefinition();
	}

#elif JUCE_MAC

#endif

	setConnected(success);

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
}

SharedTextureSender* SharedTextureManager::addSender(const String& name, int width, int height, bool enabled)
{
	SharedTextureSender* s = new SharedTextureSender(name, width, height, enabled);
	senders.add(s);
	return s;
}

SharedTextureReceiver* SharedTextureManager::addReceiver(const String& name)
{
	SharedTextureReceiver* r = new SharedTextureReceiver(name);
	receivers.add(r);
	return r;
}

void SharedTextureManager::removeSender(SharedTextureSender* sender)
{
	senders.removeObject(sender, false);
	listeners.call(&Listener::senderRemoved, sender);
	delete sender;
}

void SharedTextureManager::removeReceiver(SharedTextureReceiver* receiver)
{
	receivers.removeObject(receiver, false);
	listeners.call(&Listener::receiverRemoved, receiver);
	delete receiver;
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
