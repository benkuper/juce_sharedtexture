// #include "JuceHeader.h"

using namespace juce::gl;

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
#if JUCE_WINDOWS
    sender->ReleaseSender();
    // sender->Release();
    sender = nullptr;
#elif JUCE_MAC
    [sender stop];
#endif
}

bool SharedTextureSender::canDraw() {
    return juce::OpenGLContext::getCurrentContext() != nullptr && image.isValid();
}

void SharedTextureSender::setSize(int w, int h) {
    width = w;
    height = h;
}

void SharedTextureSender::setSharedTextureId(const GLuint newTextureId) {
    sharedTextureId = newTextureId;
}

void SharedTextureSender::setDrawFunction(std::function<void()> newDrawFunction) {
    drawFunction = newDrawFunction;
}

void SharedTextureSender::createImageDefinition() {
    if (width == 0 || height == 0) return;

    if (sharedTextureId != 0) return;

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
    if (enabled) {
#if JUCE_WINDOWS
        if (isInit && !forceRecreation) {
            sender->UpdateSender(sharingName.getCharPointer(), width, height);
            juce::Logger::writeToLog("[SharedTexture] Sender updated: \"" + sharingName + "\" " + juce::String(width) + "x" + juce::String(height));
        } else {
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
    } else {
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
    // createImageDefinition();
#if JUCE_MAC
    NSOpenGLContext* nsgl = (NSOpenGLContext*)juce::OpenGLContext::getCurrentContext()->getRawContext();
    sender = [[SyphonOpenGLServer alloc] initWithName:(NSString*)sharingName.toCFString() context:nsgl.CGLContextObj options:nil];
#endif
}

void SharedTextureSender::renderGL() {
    if (!enabled) {
        if (isInit) setupNativeSender();
        return;
    } else {
        if (!isInit) setupNativeSender();
    }

    if (!isInit) return;

    if (sharingNameChanged) {
        setupNativeSender(true);
    }

    if (sharedTextureId == 0) {
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
    }

#if JUCE_WINDOWS
    sender->SendTexture(sharedTextureId, juce::gl::GL_TEXTURE_2D, width, height);
#elif JUCE_MAC
    if (drawFunction) {
        NSRect region = NSMakeRect(0, 0, width, height);

        if ([sender bindToDrawFrameOfSize:region.size inContext:YES]) {
            drawFunction();
            [sender unbindAndPublish];
        }
    } else {
        [sender publishFrameTexture:sharedTextureId
                      textureTarget:juce::gl::GL_TEXTURE_2D
                        imageRegion:NSMakeRect(0, 0, width, height)
                  textureDimensions:NSMakeSize(width, height)
                            flipped:false];
    }
#endif
}

void SharedTextureSender::clearGL() {
    juce::Logger::writeToLog("[SharedTexture] Sender clearGL: \"" + sharingName + "\"");
    if (fbo != nullptr) fbo->release();

#if JUCE_WINDOWS
    sender->ReleaseSender();
#elif JUCE_MAC
#endif

    isInit = false;
}

void SharedTextureSender::setSharingName(juce::String value) {
    if (sharingName == value) return;
    sharingName = value;
    sharingNameChanged = true;
}

void SharedTextureSender::setEnabled(bool value) {
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
#if JUCE_WINDOWS
    if (receiver != nullptr) {
        receiver->ReleaseReceiver();
        // receiver->Release();
    }
    receiver = nullptr;
#elif JUCE_MAC
    [receiver stop];
    [receiver release];
#endif

    if (!useCPUImage) {
        if (fbo != nullptr) fbo->release();
        delete fbo;
    }
}

void SharedTextureReceiver::setSharingName(const juce::String& name, const juce::String& appName) {
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
    if (isConnected == value) return;
    isConnected = value;
    listeners.call(&Listener::connectionChanged, this);
}

void SharedTextureReceiver::setUseCPUImage(bool value) {
    if (useCPUImage == value) return;
    useCPUImage = value;
    if (!useCPUImage) outImage = juce::Image();
}

juce::Image& SharedTextureReceiver::getImage() {
    return useCPUImage ? outImage : image;
}

bool SharedTextureReceiver::canDraw() {
    return getImage().isValid() && juce::OpenGLContext::getCurrentContext() != nullptr;
}

void SharedTextureReceiver::createReceiver() {
#if JUCE_WINDOWS
    if (!isInit) {
        receiver = GetSpout();

        std::vector<char> senderNameBuf(256, 0); // 256 bytes initialized to zero

        // Copy the name into it
        std::string senderNameStr = sharingName.toStdString();
        std::memcpy(senderNameBuf.data(), senderNameStr.c_str(),
            std::min(senderNameStr.size(), size_t(255))); // ensure we don't overflow
        senderNameBuf[255] = '\0'; // Make absolutely sure it's null terminated

        unsigned int tempWidth = static_cast<unsigned int>(width);
        unsigned int tempHeight = static_cast<unsigned int>(height);

        if (!receiver || !receiver->CreateReceiver(senderNameBuf.data(), tempWidth, tempHeight)) {
            if (!createReceiverFailureLogged) {
                juce::Logger::writeToLog("[SharedTexture] Failed to create Spout receiver for \"" + sharingName + "\"");
                createReceiverFailureLogged = true;
            }
            return;
        }

        // Update our variables with values potentially modified by the function
        width = static_cast<int>(tempWidth);
        height = static_cast<int>(tempHeight);

        createImageDefinition();
        isInit = true;
        juce::Logger::writeToLog("[SharedTexture] Spout receiver created: \"" + sharingName + "\" " + juce::String(width) + "x" + juce::String(height));
    }
#elif JUCE_MAC
    if (!isInit)
        @try {
            if (receiver) [receiver release];
            receiver = nullptr;

            SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
            NSArray* servers = [directory servers];

            // NSLog(@"Available Syphon servers: %@", servers);

            NSDictionary* targetServer = nil;
            for (NSDictionary* serverInfo in servers) {
                NSString* serverName = [serverInfo objectForKey:SyphonServerDescriptionNameKey];
                NSString* appName = [serverInfo objectForKey:SyphonServerDescriptionAppNameKey];

                if ([serverName isEqualToString:(NSString*)sharingName.toCFString()] &&
                    [appName isEqualToString:(NSString*)sharingAppName.toCFString()]) {
                    targetServer = serverInfo;
                    break;
                }
            }

            if (targetServer == nil) {
                if (!createReceiverFailureLogged) {
                    juce::Logger::writeToLog("[SharedTexture] Could not find Syphon server: \"" + sharingName + "\" app: \"" + sharingAppName + "\"");
                    createReceiverFailureLogged = true;
                }
                return;
            }

            // NSLog(@"Target server description: %@", targetServer);

            NSOpenGLContext* nsgl = (NSOpenGLContext*)juce::OpenGLContext::getCurrentContext()->getRawContext();
            receiver = [[SyphonOpenGLClient alloc] initWithServerDescription:targetServer context:nsgl.CGLContextObj options:nil newFrameHandler:nil];

            if (receiver == nil) {
                if (!createReceiverFailureLogged) {
                    juce::Logger::writeToLog("[SharedTexture] Failed to create Syphon client for \"" + sharingName + "\"");
                    createReceiverFailureLogged = true;
                }
                return;
            }

            createImageDefinition();
            isInit = true;
            juce::Logger::writeToLog("[SharedTexture] Syphon receiver created: \"" + sharingName + "\" app: \"" + sharingAppName + "\"");
        }
        @catch (NSException* exception) {
            NSLog(@"Exception in createReceiver: %@", exception.reason);
        }

#endif

    if (!isInit) return;
}

void SharedTextureReceiver::createImageDefinition() {
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
    } else {
        if (fbo != nullptr) fbo->release();
        delete fbo;

        fbo = new juce::OpenGLFrameBuffer();
        fbo->initialise(*juce::OpenGLContext::getCurrentContext(), width, height);
    }
}

void SharedTextureReceiver::initGL() {
    createImageDefinition();
}

void SharedTextureReceiver::renderGL() {
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

            [syphonImage release];
        }
    }
#endif

    setConnected(success);

    if (success && useCPUImage) {
        if (!outImage.isValid() || outImage.getWidth() != width || outImage.getHeight() != height)
            outImage = juce::Image(juce::Image::ARGB, width, height, true);

        juce::Image::BitmapData sourceData(image, image.getBounds(), juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData destData(outImage, outImage.getBounds(), juce::Image::BitmapData::writeOnly);

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
    juce::Logger::writeToLog("[SharedTexture] Receiver clearGL: \"" + sharingName + "\"");
#if JUCE_MAC
    if (receiver) {
        [receiver stop];
        [receiver release];
        receiver = nullptr;
    }
#endif
}

SharedTextureManager::SharedTextureManager() {
#if JUCE_WINDOWS
    senderDetect = GetSpout();
#endif
}

SharedTextureManager::~SharedTextureManager() {
#if JUCE_WINDOWS
    senderDetect->ReleaseSender();
    senderDetect = nullptr;
#endif

    while (senders.size() > 0) removeSender(senders[0], true);
    while (receivers.size() > 0) removeReceiver(receivers[0], true);
}

SharedTextureSender* SharedTextureManager::addSender(const juce::String& name, int width, int height, bool enabled) {
    SharedTextureSender* s = new SharedTextureSender(name, width, height, enabled);
    senders.add(s);
    juce::Logger::writeToLog("[SharedTexture] Manager: added sender \"" + name + "\" " + juce::String(width) + "x" + juce::String(height) + (enabled ? " enabled" : " disabled"));
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
        sendersToRemove.add(sender);
        return;
    }

    juce::Logger::writeToLog("[SharedTexture] Manager: removed sender \"" + sender->sharingName + "\"");
    senders.removeObject(sender, false);
    listeners.call(&Listener::senderRemoved, sender);
    delete sender;
}

void SharedTextureManager::removeReceiver(SharedTextureReceiver* receiver, bool force) {
    if (receiver == nullptr) return;
    if (!force && (juce::OpenGLContext::getCurrentContext() == nullptr || !juce::OpenGLContext::getCurrentContext()->isActive())) {
        receiversToRemove.add(receiver);
        return;
    }

    juce::Logger::writeToLog("[SharedTexture] Manager: removed receiver \"" + receiver->sharingName + "\"");
    receivers.removeObject(receiver, false);
    listeners.call(&Listener::receiverRemoved, receiver);
    delete receiver;
}

void SharedTextureManager::initGL() {
    for (auto& s : senders) s->initGL();
    for (auto& r : receivers) r->initGL();

    listeners.call(&Listener::GLInitialized);
}

void SharedTextureManager::renderGL() {
    for (auto& s : sendersToRemove) removeSender(s);
    sendersToRemove.clear();

    for (auto& r : receiversToRemove) removeReceiver(r);
    receiversToRemove.clear();

    for (auto& s : senders) s->renderGL();
    for (auto& r : receivers) r->renderGL();
}

void SharedTextureManager::clearGL() {
    for (auto& s : sendersToRemove) removeSender(s);
    sendersToRemove.clear();

    for (auto& r : receiversToRemove) removeReceiver(r);
    receiversToRemove.clear();

    for (auto& s : senders) s->clearGL();
    for (auto& r : receivers) r->clearGL();

    while (senders.size() > 0) removeSender(senders[0]);
    while (receivers.size() > 0) removeReceiver(receivers[0]);
}

juce::StringArray SharedTextureManager::getAvailableSenders() {
    juce::StringArray serverList;

#if JUCE_WINDOWS
    int count = senderDetect->GetSenderCount();
    for (int i = 0; i < count; i++) {
        char sName[256];
        bool result = senderDetect->GetSender(i, sName);
        if (result) serverList.add(juce::String(sName));
    }

#elif JUCE_MAC
    SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
    NSArray* servers = [directory servers];

    for (NSDictionary* serverDescription in servers) {
        NSString* serverName = [serverDescription objectForKey:SyphonServerDescriptionNameKey];
        NSString* appName = [serverDescription objectForKey:SyphonServerDescriptionAppNameKey];

        juce::String serverString = juce::String::fromUTF8([serverName UTF8String]);
        serverString += " - ";
        serverString += juce::String::fromUTF8([appName UTF8String]);

        serverList.add(serverString);
    }
#endif

    return serverList;
}
