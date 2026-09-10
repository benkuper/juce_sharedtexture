// #include "JuceHeader.h"

using namespace juce::gl;

// Global SharedTextureManager instance
SharedTextureManager* g_SharedTextureManager = nullptr;
static juce::CriticalSection sharedTextureManagerLock;

#define Init2DViewport(w, h) \
    glViewport(0, 0, w, h);  \
    Init2DMatrix(w, h);

#define Init2DMatrix(w, h)       \
    glMatrixMode(GL_PROJECTION); \
    glLoadIdentity();            \
    glOrtho(0, w, 0, h, 0, 1);   \
    glMatrixMode(GL_MODELVIEW);  \
    glLoadIdentity();

#define Draw2DTexRect_Rectangle(x, y, w, h, texW, texH) \
    glBegin(GL_QUADS);                                  \
    glTexCoord2f(0, 0);                                 \
    glVertex2f(x, y);                                   \
    glTexCoord2f(texW, 0);                              \
    glVertex2f(x + w, y);                               \
    glTexCoord2f(texW, texH);                           \
    glVertex2f(x + w, y + h);                           \
    glTexCoord2f(0, texH);                              \
    glVertex2f(x, y + h);                               \
    glEnd();

SharedTextureSender::SharedTextureSender(const juce::String& name, int width, int height, bool enabled) : isInit(false),
sharingName(name),
sharingNameChanged(false),
enabled(enabled),
fbo(nullptr),
width(width),
height(height) {
#if JUCE_WINDOWS
	sender = GetSpout();
#elif JUCE_MAC
	sender = nullptr;
#endif
}

SharedTextureSender::~SharedTextureSender() {
	const juce::ScopedLock stateGuard(stateLock);
#if JUCE_WINDOWS
	if (sender != nullptr) {
		sender->ReleaseSender();
		sender->Release();
		sender = nullptr;
	}
#elif JUCE_MAC
	[sender stop];
#endif
}

bool SharedTextureSender::canDraw() {
	const juce::ScopedLock stateGuard(stateLock);
	return juce::OpenGLContext::getCurrentContext() != nullptr && image.isValid();
}

void SharedTextureSender::setSize(int w, int h) {
	const juce::ScopedLock stateGuard(stateLock);
	width = w;
	height = h;
}

void SharedTextureSender::setSharedTextureId(const GLuint newTextureId) {
	const juce::ScopedLock stateGuard(stateLock);
	sharedTextureId = newTextureId;
	externalFBO = nullptr;
}

void SharedTextureSender::setExternalFBO(juce::OpenGLFrameBuffer* newFBO) {
	const juce::ScopedLock stateGuard(stateLock);
	externalFBO = newFBO;
	sharedTextureId = 0;
}

void SharedTextureSender::setDrawFunction(std::function<void()> newDrawFunction) {
	const juce::ScopedLock stateGuard(stateLock);
	drawFunction = newDrawFunction;
}

void SharedTextureSender::createImageDefinition() {
	const juce::ScopedLock stateGuard(stateLock);
	if (width == 0 || height == 0) return;

	if (sharedTextureId != 0 || externalFBO != nullptr) return;

	if (fbo != nullptr) fbo->release();

	if (enabled) {
		// MessageManager::callAsync([&] {

		image = juce::Image(juce::Image::ARGB, width, height, true, juce::OpenGLImageType()); // create the openGL image
		fbo = juce::OpenGLImageType::getFrameBufferFrom(image);
		//});
	}

	setupNativeSender();
}

void SharedTextureSender::setupNativeSender(bool forceRecreation) {
	const juce::ScopedLock stateGuard(stateLock);
	if (enabled) {
#if JUCE_WINDOWS
		if (isInit && !forceRecreation) {
			sender->UpdateSender(sharingName.getCharPointer(), width, height);
			juce::Logger::writeToLog("[SharedTexture] Sender updated: \"" + sharingName + "\" " + juce::String(width) + "x" + juce::String(height));
		}
		else {
			if (isInit) sender->ReleaseSender();
			sender->CreateSender(sharingName.getCharPointer(), width, height);
			isInit = true;
			juce::Logger::writeToLog("[SharedTexture] Sender created: \"" + sharingName + "\" " + juce::String(width) + "x" + juce::String(height));
		}

		sender->SetSenderName(sharingName.getCharPointer());
#elif JUCE_MAC
		sender.name = (NSString*)sharingName.toCFString();
		isInit = true;
		juce::Logger::writeToLog("[SharedTexture] Syphon sender created: \"" + sharingName + "\" " + juce::String(width) + "x" + juce::String(height));
#endif
	}
	else {
#if JUCE_WINDOWS
		if (isInit) {
			sender->ReleaseSender();
			juce::Logger::writeToLog("[SharedTexture] Sender released: \"" + sharingName + "\"");
		}
#elif JUCE_MAC

#endif
		isInit = false;
	}

	sharingNameChanged = false;
}

void SharedTextureSender::initGL() {
	const juce::ScopedLock stateGuard(stateLock);
	// createImageDefinition();
#if JUCE_MAC
	NSOpenGLContext* nsgl = (NSOpenGLContext*)juce::OpenGLContext::getCurrentContext()->getRawContext();
	sender = [[SyphonOpenGLServer alloc]initWithName:(NSString*)sharingName.toCFString() context : nsgl.CGLContextObj options : nil];
#endif
}

void SharedTextureSender::renderGL() {
	const juce::ScopedLock stateGuard(stateLock);
	if (!enabled) {
		if (isInit) setupNativeSender();
		return;
	}
	else {
		if (!isInit) setupNativeSender();
	}

	if (!isInit) return;

	if (sharingNameChanged) {
		setupNativeSender(true);
	}

	GLuint textureIdToSend = sharedTextureId;
	if (externalFBO != nullptr) {
		const int externalWidth = externalFBO->getWidth();
		const int externalHeight = externalFBO->getHeight();
		if (externalWidth != width || externalHeight != height) {
			width = externalWidth;
			height = externalHeight;
			setupNativeSender();
		}
		textureIdToSend = externalFBO->getTextureID();
	}

	if (textureIdToSend == 0) {
		if (!isInit || !image.isValid() || image.getWidth() != width || image.getHeight() != height) createImageDefinition();
		if (!image.isValid()) {
			juce::Logger::writeToLog("[SharedTexture] Failed to create sender image: " + juce::String(width) + "x" + juce::String(height));
			return;
		}

		juce::Rectangle<int> r = image.getBounds();
		image.clear(r);
		juce::Graphics g(image);
		g.beginTransparencyLayer(1);
		sharedTextureListeners.call(&SharedTextureListener::drawSharedTexture, g, r);
		g.endTransparencyLayer();
		if (fbo != nullptr)
			textureIdToSend = fbo->getTextureID();
	}
	if (textureIdToSend == 0) return;

#if JUCE_WINDOWS
	sender->SendTexture(textureIdToSend, juce::gl::GL_TEXTURE_2D, width, height);
#elif JUCE_MAC
	if (drawFunction) {
		NSRect region = NSMakeRect(0, 0, width, height);

		if ([sender bindToDrawFrameOfSize : region.size inContext : YES]) {
			drawFunction();
			[sender unbindAndPublish] ;
		}
	}
	else {
		[sender publishFrameTexture:textureIdToSend
			textureTarget : juce::gl::GL_TEXTURE_2D
			imageRegion : NSMakeRect(0, 0, width, height)
			textureDimensions : NSMakeSize(width, height)
			flipped : false] ;
	}
#endif
}

void SharedTextureSender::clearGL() {
	const juce::ScopedLock stateGuard(stateLock);
	juce::Logger::writeToLog("[SharedTexture] Sender clearGL: \"" + sharingName + "\"");
	if (fbo != nullptr) fbo->release();

#if JUCE_WINDOWS
	sender->ReleaseSender();
#elif JUCE_MAC
#endif

	isInit = false;
}

void SharedTextureSender::setSharingName(juce::String value) {
	const juce::ScopedLock stateGuard(stateLock);
	if (sharingName == value) return;
	sharingName = value;
	sharingNameChanged = true;
}

void SharedTextureSender::setEnabled(bool value) {
	const juce::ScopedLock stateGuard(stateLock);
	enabled = value;
}

// REC

SharedTextureReceiver::SharedTextureReceiver(const juce::String& _sharingName, const juce::String& _sharingAppName) :
#if JUCE_WINDOWS
	receiver(nullptr),
#endif
	enabled(true),
	sharingName(_sharingName),
	sharingAppName(_sharingAppName),
	isInit(false),
	isConnected(false),
	width(0),
	height(0),
	invertImage(false),
	fbo(nullptr),
	useCPUImage(SHAREDTEXTURE_USE_CPU_IMAGE) {

#if JUCE_WINDOWS

#elif JUCE_MAC
#endif
}

SharedTextureReceiver::~SharedTextureReceiver() {
	const juce::ScopedLock stateGuard(stateLock);
#if JUCE_WINDOWS
	if (receiver != nullptr) {
		receiver->ReleaseReceiver();
		receiver->Release();
		receiver = nullptr;
	}
#elif JUCE_MAC
	[receiver stop];
	[receiver release] ;
#endif

	if (!useCPUImage) {
		if (fbo != nullptr) fbo->release();
		delete fbo;
	}
}

void SharedTextureReceiver::setSharingName(const juce::String& name, const juce::String& appName) {
	const juce::ScopedLock stateGuard(stateLock);
	if (name == sharingName && appName == sharingAppName) return;
	sharingName = name;
	sharingAppName = appName;

#if JUCE_WINDOWS
	if (receiver == nullptr) return;
	receiver->SetReceiverName(sharingName.toStdString().c_str());
#elif JUCE_MAC
	isInit = false;
#endif
}

void SharedTextureReceiver::setConnected(bool value) {
	const juce::ScopedLock stateGuard(stateLock);
	if (isConnected == value) return;
	isConnected = value;
	listeners.call(&Listener::connectionChanged, this);
}

void SharedTextureReceiver::setUseCPUImage(bool value) {
	const juce::ScopedLock stateGuard(stateLock);
	if (useCPUImage == value) return;
	useCPUImage = value;
	if (!useCPUImage) outImage = juce::Image();
}

juce::Image& SharedTextureReceiver::getImage() {
	const juce::ScopedLock stateGuard(stateLock);
	return useCPUImage ? outImage : image;
}

bool SharedTextureReceiver::canDraw() {
	const juce::ScopedLock stateGuard(stateLock);
	return getImage().isValid() && juce::OpenGLContext::getCurrentContext() != nullptr;
}

void SharedTextureReceiver::createReceiver() {
	const juce::ScopedLock stateGuard(stateLock);
#if JUCE_WINDOWS
	if (!isInit) {
		receiver = GetSpout();

		if (!receiver) {
			if (!createReceiverFailureLogged) {
				juce::Logger::writeToLog("[SharedTexture] Failed to create Spout receiver: GetSpout() returned null");
				createReceiverFailureLogged = true;
			}
			return;
		}

		// IMPORTANT: Use the modern Spout 2.007+ pattern.
		//
		// We deliberately avoid CreateReceiver(name, w, h) here because its
		// internal CheckReceiver() falls back to Spout's "active sender" when
		// the named sender is not (yet) in the registry, silently connecting
		// to the wrong sender. SetReceiverName + ReceiveTexture (in renderGL)
		// locks selection to exactly the requested sender with no fallback.
		receiver->SetReceiverName(sharingName.toStdString().c_str());

		// Bootstrap a small placeholder FBO so renderGL has something to receive
		// into. On the first successful ReceiveTexture, IsUpdated() will be true
		// and createImageDefinition() will resize the FBO to match the sender.
		if (width <= 0) width = 16;
		if (height <= 0) height = 16;

		createImageDefinition();
		isInit = true;
		createReceiverFailureLogged = false;
		juce::Logger::writeToLog("[SharedTexture] Spout receiver initialised for \"" + sharingName + "\" (awaiting first frame)");
	}
#elif JUCE_MAC
	if (!isInit)
		@try {
		if (receiver)[receiver release];
		receiver = nullptr;

		SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
		NSArray* servers = [directory servers];

		NSDictionary* targetServer = nil;
		for (NSDictionary* serverInfo in servers) {
			NSString* serverName = [serverInfo objectForKey:SyphonServerDescriptionNameKey];
			NSString* appName = [serverInfo objectForKey:SyphonServerDescriptionAppNameKey];

			juce::String sName = juce::String::fromUTF8([serverName UTF8String]);
			juce::String aName = juce::String::fromUTF8([appName UTF8String]);

			if (sName == sharingName && aName == sharingAppName) {
				targetServer = serverInfo;
				break;
			}
		}

		if (targetServer == nil) {
			if (!createReceiverFailureLogged) {
				juce::String available;
				for (NSDictionary* serverInfo in servers) {
					NSString* sn = [serverInfo objectForKey:SyphonServerDescriptionNameKey];
					NSString* an = [serverInfo objectForKey:SyphonServerDescriptionAppNameKey];
					if (available.isNotEmpty()) available += ", ";
					available += "\"" + juce::String::fromUTF8([sn UTF8String]) + "\" app: \"" + juce::String::fromUTF8([an UTF8String]) + "\"";
				}
				juce::Logger::writeToLog("[SharedTexture] Could not find Syphon server: \"" + sharingName + "\" app: \"" + sharingAppName + "\" (available: " + (available.isEmpty() ? "none" : available) + ")");
				createReceiverFailureLogged = true;
			}
			return;
		}

		// NSLog(@"Target server description: %@", targetServer);

		NSOpenGLContext* nsgl = (NSOpenGLContext*)juce::OpenGLContext::getCurrentContext()->getRawContext();
		receiver = [[SyphonOpenGLClient alloc]initWithServerDescription:targetServer context : nsgl.CGLContextObj options : nil newFrameHandler : nil];

		if (receiver == nil) {
			if (!createReceiverFailureLogged) {
				juce::Logger::writeToLog("[SharedTexture] Failed to create Syphon client for \"" + sharingName + "\"");
				createReceiverFailureLogged = true;
			}
			return;
		}

		createImageDefinition();
		isInit = true;
		createReceiverFailureLogged = false;
		juce::Logger::writeToLog("[SharedTexture] Syphon receiver created: \"" + sharingName + "\" app: \"" + sharingAppName + "\"");
	}
	@catch (NSException* exception) {
		NSLog(@"Exception in createReceiver: %@", exception.reason);
	}

#endif

	if (!isInit) return;
}

void SharedTextureReceiver::createImageDefinition() {
	const juce::ScopedLock stateGuard(stateLock);
	if (juce::OpenGLContext::getCurrentContext() == nullptr) return;
	if (!juce::OpenGLContext::getCurrentContext()->isActive()) return;

#if JUCE_WINDOWS
	if (receiver == nullptr) return;

	width = juce::jmax<int>(receiver->GetSenderWidth(), 1);
	height = juce::jmax<int>(receiver->GetSenderHeight(), 1);
#elif JUCE_MAC
	if (receiver == nullptr) {
		NSLog(@"Receiver is null in createImageDefinition");
		return;
	}
#endif

	if (width == 0 || height == 0) return;

	if (useCPUImage) {
		image = juce::Image(juce::Image::ARGB, width, height, true, juce::OpenGLImageType()); // create the openGL image
		outImage = juce::Image(juce::Image::ARGB, width, height, true);                       // not gl to be able to manipulate
		fbo = juce::OpenGLImageType::getFrameBufferFrom(image);
	}
	else {
		if (fbo != nullptr) fbo->release();
		delete fbo;

		fbo = new juce::OpenGLFrameBuffer();
		fbo->initialise(*juce::OpenGLContext::getCurrentContext(), width, height);
	}
}

void SharedTextureReceiver::initGL() {
	const juce::ScopedLock stateGuard(stateLock);
	createImageDefinition();
}

void SharedTextureReceiver::renderGL() {
	const juce::ScopedLock stateGuard(stateLock);
	if (!enabled) return;
	if (!isInit) {
		createReceiver();
		return;
	}

	bool success = false;

#if JUCE_WINDOWS
	// Check if fbo is null before using it
	if (fbo == nullptr) {
		// Try to initialize it again
		createImageDefinition();
		// If still null, we can't proceed
		if (fbo == nullptr) {
			return;
		}
	}

	success = receiver->ReceiveTexture(fbo->getTextureID(), juce::gl::GL_TEXTURE_2D, invertImage);
	// DBG("Receiver Texture : " << (int)success << " / Get Sender Name [" << sharingName << "] : " << receiver->GetSenderName() << " ( " << (int)receiver->GetSenderWidth() << "x" << (int)receiver->GetSenderHeight() << ")");

	if (success) {
		if (receiver->IsUpdated()) createImageDefinition();
	}

#elif JUCE_MAC
	if (receiver && [receiver hasNewFrame]) {
		SyphonOpenGLImage* syphonImage = [receiver newFrameImage];
		if (syphonImage) {
			GLuint textureName = syphonImage.textureName;
			NSSize textureSize = syphonImage.textureSize;

			// Update our texture size if it has changed
			if (width != textureSize.width || height != textureSize.height) {
				width = textureSize.width;
				height = textureSize.height;
				createImageDefinition();
			}

			fbo->makeCurrentAndClear();

			Init2DViewport(width, height)

				glEnable(GL_TEXTURE_RECTANGLE_ARB);
			glDisable(GL_DEPTH_TEST);
			glColor4f(1, 1, 1, 1);

			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, textureName);

			Draw2DTexRect_Rectangle(0, 0, width, height, width, height);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

			fbo->releaseAsRenderingTarget();

			success = true;

			[syphonImage release] ;
		}
	}
#endif

	setConnected(success);

	if (success && useCPUImage) {
		if (!outImage.isValid() || outImage.getWidth() != width || outImage.getHeight() != height)
			outImage = juce::Image(juce::Image::ARGB, width, height, true);

		juce::Image::BitmapData sourceData = juce::Image::BitmapData(image, 0, 0, image.getWidth(), image.getHeight(), juce::Image::BitmapData::readOnly);
		juce::Image::BitmapData destData(outImage, 0, 0, outImage.getWidth(), outImage.getHeight(), juce::Image::BitmapData::writeOnly);

		jassert(sourceData.pixelStride == destData.pixelStride);

		const auto copyWidth = juce::jmin(sourceData.width, destData.width);
		const auto copyHeight = juce::jmin(sourceData.height, destData.height);
		const auto rowBytes = (size_t) copyWidth * (size_t) destData.pixelStride;

		for (int y = 0; y < copyHeight; ++y)
		{
			auto* dstLine = destData.getLinePointer(y);
			auto* srcLine = sourceData.getLinePointer(copyHeight - 1 - y);
			std::memcpy(dstLine, srcLine, rowBytes);
		}
	}

	listeners.call(&Listener::textureUpdated, this);
}

void SharedTextureReceiver::clearGL() {
	const juce::ScopedLock stateGuard(stateLock);
	juce::Logger::writeToLog("[SharedTexture] Receiver clearGL: \"" + sharingName + "\"");
#if JUCE_MAC
	if (receiver) {
		[receiver stop] ;
		[receiver release] ;
		receiver = nullptr;
	}
#endif
}

SharedTextureManager* SharedTextureManager::getInstance() {
	const juce::ScopedLock lock(sharedTextureManagerLock);
	if (g_SharedTextureManager == nullptr)
		g_SharedTextureManager = new SharedTextureManager();
	return g_SharedTextureManager;
}

SharedTextureManager* SharedTextureManager::getInstanceWithoutCreating() noexcept {
	const juce::ScopedLock lock(sharedTextureManagerLock);
	return g_SharedTextureManager;
}

void SharedTextureManager::deleteInstance() {
	const juce::ScopedLock lock(sharedTextureManagerLock);
	delete g_SharedTextureManager;
	g_SharedTextureManager = nullptr;
}

SharedTextureManager::SharedTextureManager() {
	const juce::ScopedLock lock(sharedTextureManagerLock);
	jassert(g_SharedTextureManager == nullptr || g_SharedTextureManager == this);
	g_SharedTextureManager = this;
#if JUCE_WINDOWS
	senderDetect = GetSpout();
#endif
}

SharedTextureManager::~SharedTextureManager() {
	const juce::ScopedLock managerGuard(sharedTextureManagerLock);
#if JUCE_WINDOWS
	if (senderDetect != nullptr) {
		senderDetect->ReleaseSender();
		senderDetect->Release();
		senderDetect = nullptr;
	}
#endif

	while (senders.size() > 0) removeSender(senders[0], true);
	while (receivers.size() > 0) removeReceiver(receivers[0], true);
	if (g_SharedTextureManager == this)
		g_SharedTextureManager = nullptr;
}

SharedTextureSender* SharedTextureManager::addSender(const juce::String& name, int width, int height, bool enabled) {
	SharedTextureSender* s = new SharedTextureSender(name, width, height, enabled);
	senders.add(s);
	juce::Logger::writeToLog("[SharedTexture] Manager: added sender \"" + name + "\" " + juce::String(width) + "x" + juce::String(height)+(enabled ? " enabled" : " disabled"));
	return s;
}

SharedTextureReceiver* SharedTextureManager::addReceiver(const juce::String& name, const juce::String& appName) {
	SharedTextureReceiver* r = new SharedTextureReceiver(name, appName);
	receivers.add(r);
	juce::Logger::writeToLog("[SharedTexture] Manager: added receiver \"" + name + "\" app: \"" + appName + "\"");
	return r;
}

void SharedTextureManager::removeSender(SharedTextureSender* sender, bool force) {
	if (sender == nullptr) return;

	if (!force && (juce::OpenGLContext::getCurrentContext() == nullptr || !juce::OpenGLContext::getCurrentContext()->isActive())) {
		sendersToRemove.addIfNotAlreadyThere(sender);
		return;
	}

	sendersToRemove.removeAllInstancesOf(sender);
	juce::Logger::writeToLog("[SharedTexture] Manager: removed sender \"" + sender->sharingName + "\"");
	senders.removeObject(sender, false);
	listeners.call(&Listener::senderRemoved, sender);
	delete sender;
}

void SharedTextureManager::removeReceiver(SharedTextureReceiver* receiver, bool force) {
	if (receiver == nullptr) return;
	if (!force && (juce::OpenGLContext::getCurrentContext() == nullptr || !juce::OpenGLContext::getCurrentContext()->isActive())) {
		receiversToRemove.addIfNotAlreadyThere(receiver);
		return;
	}

	receiversToRemove.removeAllInstancesOf(receiver);
	juce::Logger::writeToLog("[SharedTexture] Manager: removed receiver \"" + receiver->sharingName + "\"");
	receivers.removeObject(receiver, false);
	listeners.call(&Listener::receiverRemoved, receiver);
	delete receiver;
}

void SharedTextureManager::initGL() {
	{
		const juce::ScopedLock lock(senders.getLock());
		for (auto& s : senders) s->initGL();
	}
	{
		const juce::ScopedLock lock(receivers.getLock());
		for (auto& r : receivers) r->initGL();
	}

	listeners.call(&Listener::GLInitialized);
}

void SharedTextureManager::renderGL() {
	juce::Array<SharedTextureSender*> pendingSenders;
	{
		const juce::ScopedLock lock(sendersToRemove.getLock());
		pendingSenders.addArray(sendersToRemove);
		sendersToRemove.clear();
	}
	for (auto* sender : pendingSenders) removeSender(sender, true);

	juce::Array<SharedTextureReceiver*> pendingReceivers;
	{
		const juce::ScopedLock lock(receiversToRemove.getLock());
		pendingReceivers.addArray(receiversToRemove);
		receiversToRemove.clear();
	}
	for (auto* receiver : pendingReceivers) removeReceiver(receiver, true);

	{
		const juce::ScopedLock lock(senders.getLock());
		for (auto& s : senders) s->renderGL();
	}
	{
		const juce::ScopedLock lock(receivers.getLock());
		for (auto& r : receivers) r->renderGL();
	}
}

void SharedTextureManager::clearGL() {
	juce::Array<SharedTextureSender*> pendingSenders;
	{
		const juce::ScopedLock lock(sendersToRemove.getLock());
		pendingSenders.addArray(sendersToRemove);
		sendersToRemove.clear();
	}
	for (auto* sender : pendingSenders) removeSender(sender, true);

	juce::Array<SharedTextureReceiver*> pendingReceivers;
	{
		const juce::ScopedLock lock(receiversToRemove.getLock());
		pendingReceivers.addArray(receiversToRemove);
		receiversToRemove.clear();
	}
	for (auto* receiver : pendingReceivers) removeReceiver(receiver, true);

	{
		const juce::ScopedLock lock(senders.getLock());
		for (auto& s : senders) s->clearGL();
	}
	{
		const juce::ScopedLock lock(receivers.getLock());
		for (auto& r : receivers) r->clearGL();
	}

	while (senders.size() > 0) removeSender(senders[0]);
	while (receivers.size() > 0) removeReceiver(receivers[0]);
}

void SharedTextureManager::getAvailableSenderDetails(juce::StringArray& serverNames, juce::StringArray& appNames) {
	serverNames.clear();
	appNames.clear();

#if JUCE_WINDOWS
	int count = senderDetect->GetSenderCount();
	for (int i = 0; i < count; i++) {
		char sName[256];
		bool result = senderDetect->GetSender(i, sName);
		if (result) {
			serverNames.add(juce::String(sName));
			appNames.add(juce::String());
		}
	}
#elif JUCE_MAC
	SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
	NSArray* servers = [directory servers];

	for (NSDictionary* serverDescription in servers) {
		NSString* sn = [serverDescription objectForKey:SyphonServerDescriptionNameKey];
		NSString* an = [serverDescription objectForKey:SyphonServerDescriptionAppNameKey];
		serverNames.add(juce::String::fromUTF8([sn UTF8String]));
		appNames.add(juce::String::fromUTF8([an UTF8String]));
	}
#endif
}

juce::StringArray SharedTextureManager::getAvailableSenders() {
	juce::StringArray serverNames, appNames;
	getAvailableSenderDetails(serverNames, appNames);
	return serverNames;
}
