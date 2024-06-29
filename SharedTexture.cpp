//#include "JuceHeader.h"

SharedTextureSender::SharedTextureSender(const juce::String& name, int width, int height, bool enabled) :
    isInit(false),
    sharingName(name),
    sharingNameChanged(false),
    enabled(enabled),
    fbo(nullptr),
    externalFBO(nullptr),
    width(width),
    height(height)
{
    
#if JUCE_WINDOWS
    sender = GetSpout();
#elif JUCE_MAC
    
#endif
}

SharedTextureSender::~SharedTextureSender()
{
#if JUCE_WINDOWS
    sender->ReleaseSender();
    sender->Release();
    sender = nullptr;
#endif
}

bool SharedTextureSender::canDraw()
{
    return juce::OpenGLContext::getCurrentContext() != nullptr && image.isValid();
}

void SharedTextureSender::setSize(int w, int h)
{
    width = w;
    height = h;
}

void SharedTextureSender::setExternalFBO(juce::OpenGLFrameBuffer* newFBO)
{
    externalFBO = newFBO;
    setSize(externalFBO->getWidth(), externalFBO->getHeight());
}

void SharedTextureSender::createImageDefinition()
{
    if (width == 0 || height == 0) return;
    
    if (externalFBO != nullptr) return;
    
    if (fbo != nullptr) fbo->release();
    
    if (enabled)
    {
        //MessageManager::callAsync([&] {
        
        image = juce::Image(juce::Image::ARGB, width, height, true, juce::OpenGLImageType()); //create the openGL image
        fbo = juce::OpenGLImageType::getFrameBufferFrom(image);
        //});
    }
    
    setupNativeSender();
}

void SharedTextureSender::setupNativeSender(bool forceRecreation)
{
    if (enabled)
    {
#if JUCE_WINDOWS
        if (isInit && !forceRecreation) sender->UpdateSender(sharingName.getCharPointer(), width, height);
        else
        {
            if (isInit) sender->ReleaseSender();
            sender->CreateSender(sharingName.getCharPointer(), width, height);
            isInit = true;
        }
#elif JUCE_MAC
        
#endif
    }
    else
    {
#if JUCE_WINDOWS
        if (isInit) sender->ReleaseSender();
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
    else
    {
        if (!isInit) setupNativeSender();
    }
    
    if (!isInit) return;
    
    if (sharingNameChanged)
    {
        setupNativeSender(true);
    }
    
    
    juce::OpenGLFrameBuffer* targetFBO = fbo;
    
    if (externalFBO != nullptr)
    {
        if (externalFBO->getWidth() != width || externalFBO->getHeight() != height) setSize(externalFBO->getWidth(), externalFBO->getHeight());
        targetFBO = externalFBO;
    }
    else
    {
        if (!isInit || !image.isValid() || image.getWidth() != width || image.getHeight() != height) createImageDefinition();
        if (!image.isValid())
        {
            DBG("Problem creating image");
            return;
        }
        
        juce::Rectangle<int> r = image.getBounds();
        image.clear(r);
        juce::Graphics g(image);
        g.beginTransparencyLayer(1);
        sharedTextureListeners.call(&SharedTextureListener::drawSharedTexture, g, r);
        g.endTransparencyLayer();
    }
    
    if (targetFBO == nullptr) return;
    
#if JUCE_WINDOWS
    sender->SendTexture(targetFBO->getTextureID(), juce::gl::GL_TEXTURE_2D, width, height);
#elif JUCE_MAC
    
#endif
}

void SharedTextureSender::clearGL()
{
    if (fbo != nullptr) fbo->release();
    
#if JUCE_WINDOWS
    sender->ReleaseSender();
#elif JUCE_MAC
#endif
    
    isInit = false;
}

void SharedTextureSender::setSharingName(juce::String value)
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
invertImage(true),
fbo(nullptr),
useCPUImage(SHAREDTEXTURE_USE_CPU_IMAGE)
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
#elif JUCE_MAC
    [receiver stop];
    [receiver release];
#endif
    
    if (!useCPUImage)
    {
        if (fbo != nullptr) fbo->release();
        delete fbo;
    }
}

void SharedTextureReceiver::setSharingName(const juce::String& name, const juce::String& appName)
{
    if (name == sharingName && appName == sharingAppName) return;
    sharingName = name;
    sharingAppName = appName;
    
#if JUCE_WINDOWS
    if (receiver == nullptr) return;
    receiver->SetReceiverName(sharingName.toStdString().c_str());
#elif JUCE_MAC
    if (receiver)
    {
        [receiver setServerName:sharingName.toCFString() appName:sharingAppName.toCFString()];
    }
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
    if (!useCPUImage) outImage = juce::Image();
}

juce::Image& SharedTextureReceiver::getImage()
{
    return image;
}

bool SharedTextureReceiver::canDraw()
{
    return getImage().isValid() && juce::OpenGLContext::getCurrentContext() != nullptr;
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
    if (!isInit)
        @try {
                    SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
                    NSArray* servers = [directory servers];
                    
                    NSLog(@"Available Syphon servers: %@", servers);

                    NSDictionary* targetServer = nil;
                    for (NSDictionary* serverInfo in servers)
                    {
                        NSString* serverName = [serverInfo objectForKey:SyphonServerDescriptionNameKey];
                        NSString* appName = [serverInfo objectForKey:SyphonServerDescriptionAppNameKey];
                        
                        if ([serverName isEqualToString:(NSString*)sharingName.toCFString()] &&
                            [appName isEqualToString:(NSString*)sharingAppName.toCFString()])
                        {
                            targetServer = serverInfo;
                            break;
                        }
                    }

                    if (targetServer == nil)
                    {
                        NSLog(@"Could not find the specified Syphon server");
                        return;
                    }

                    NSLog(@"Target server description: %@", targetServer);

                    receiver = [[SyphonClient alloc] initWithServerDescription:targetServer context:CGLGetCurrentContext() options:nil newFrameHandler:nil];

                    if (receiver == nil)
                    {
                        NSLog(@"Failed to create SyphonClient");
                        return;
                    }

                    NSLog(@"SyphonClient created successfully");
                    
                    createImageDefinition();
                    isInit = true;
                }
                @catch (NSException *exception) {
                    NSLog(@"Exception in createReceiver: %@", exception.reason);
                }
    
#endif
    
    if (!isInit) return;
}


void SharedTextureReceiver::createImageDefinition()
{
    if (juce::OpenGLContext::getCurrentContext() == nullptr) return;
    if (!juce::OpenGLContext::getCurrentContext()->isActive()) return;
    
#if JUCE_WINDOWS
    if (receiver == nullptr) return;
    
    width = jmax<int>(receiver->GetSenderWidth(), 1);
    height = jmax<int>(receiver->GetSenderHeight(), 1);
#elif JUCE_MAC
    if (receiver == nullptr)
    {
        NSLog(@"Receiver is null in createImageDefinition");
        return;
    }
    
//    NSSize textureSize = NSZeroSize;
    
//    @try {
//        if ([receiver isValid])
//        {
//            textureSize = [receiver textureSize];
//            NSLog(@"Received texture size: %@", NSStringFromSize(textureSize));
//        }
//        else
//        {
//            NSLog(@"Receiver is not valid");
//            return;
//        }
//    }
//    @catch (NSException *exception) {
//        NSLog(@"Exception when getting texture size: %@", exception.reason);
//        return;
//    }
//    
//    if (NSEqualSizes(textureSize, NSZeroSize))
//    {
//        NSLog(@"Received zero texture size");
//        return;
//    }
//    
//    width = juce::jmax<int>(textureSize.width, 1);
//    height = juce::jmax<int>(textureSize.height, 1);
#endif
    
    if(width == 0 || height == 0) return;
    
    if (useCPUImage)
    {
        image = juce::Image(juce::Image::ARGB, width, height, true, juce::OpenGLImageType()); //create the openGL image
        outImage = juce::Image(juce::Image::ARGB, width, height, true); //not gl to be able to manipulate
        fbo = juce::OpenGLImageType::getFrameBufferFrom(image);
    }
    else
    {
        if (fbo != nullptr) fbo->release();
        delete fbo;
        
        fbo = new juce::OpenGLFrameBuffer();
        fbo->initialise(*juce::OpenGLContext::getCurrentContext(), width, height);
    }
    
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
    
    
    
    bool success = false;
    
#if JUCE_WINDOWS
    //unsigned int receiveWidth = width, receiveHeight = height;
    
    success = receiver->ReceiveTexture(fbo->getTextureID(), juce::gl::GL_TEXTURE_2D, invertImage);
    //DBG("Receiver Texture : " << (int)success << " / Get Sender Name [" << sharingName << "] : " << receiver->GetSenderName() << " ( " << (int)receiver->GetSenderWidth() << "x" << (int)receiver->GetSenderHeight() << ")");
    
    if (success)
    {
        if (receiver->IsUpdated()) createImageDefinition();
    }
    
#elif JUCE_MAC
    if (receiver && [receiver hasNewFrame])
    {
        [receiver invalidateFrame];
            SyphonOpenGLImage* syphonImage = [receiver newFrameImage];
            if (syphonImage)
            {
                GLuint textureName = syphonImage.textureName;
                NSSize textureSize = syphonImage.textureSize;
                
                // Update our texture size if it has changed
                if (width != textureSize.width || height != textureSize.height)
                {
                    width = textureSize.width;
                    height = textureSize.height;
                    createImageDefinition();
                }
                
                // Bind the received texture to our FBO
                juce::gl::glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, fbo->getFrameBufferID());
                juce::gl::glFramebufferTexture2D(juce::gl::GL_FRAMEBUFFER, juce::gl::GL_COLOR_ATTACHMENT0, juce::gl::GL_TEXTURE_2D, textureName, 0);
                
                success = true;
                
                // If using CPU image, copy the texture data
                if (useCPUImage)
                {
                    //outImage.clear(outImage.getBounds());
                    //                juce::OpenGLFrameBuffer::copyTexture(juce::gl::GL_TEXTURE_2D, textureName,
                    //                                                     juce::gl::GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    //                                                     0, 0, width, height,
                    //                                                     outImage, invertImage);
                    
                   
//                    outImage.clear(outImage.getBounds());
//                    juce::Graphics g(outImage);
//                    g.drawImage(image, outImage.getBounds().toFloat());
                }
                
                // Clean up
//                juce::gl::glFramebufferTexture2D(juce::gl::GL_FRAMEBUFFER, juce::gl::GL_COLOR_ATTACHMENT0, juce::gl::GL_TEXTURE_2D, 0, 0);
                juce::gl::glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, 0);
//                
                if (!outImage.isValid()) outImage = juce::Image(juce::Image::ARGB, width, height, true); //not gl to be able to manipulate
                
                //outImage = image.createCopy();
                
                [syphonImage release];
            }
    }
#endif
    
    setConnected(success);
    
    
    if (success && useCPUImage)
    {
        if (!outImage.isValid()) outImage = juce::Image(juce::Image::ARGB, width, height, true); //not gl to be able to manipulate
        
         outImage = image;
//        outImage.clear(outImage.getBounds());
//        juce::Graphics g(outImage);
//        g.drawImage(image, outImage.getBounds().toFloat());
    }
    
    listeners.call(&Listener::textureUpdated, this);
}

void SharedTextureReceiver::clearGL()
{
    #if JUCE_MAC
    if (receiver)
    {
        [receiver stop];
        [receiver release];
        receiver = nullptr;
    }
    #endif
}


juce_ImplementSingleton(SharedTextureManager)

SharedTextureManager::SharedTextureManager()
{
}

SharedTextureManager::~SharedTextureManager()
{
    while (senders.size() > 0) removeSender(senders[0], true);
    while (receivers.size() > 0) removeReceiver(receivers[0], true);
}

SharedTextureSender* SharedTextureManager::addSender(const juce::String& name, int width, int height, bool enabled)
{
    SharedTextureSender* s = new SharedTextureSender(name, width, height, enabled);
    senders.add(s);
    return s;
}

SharedTextureReceiver* SharedTextureManager::addReceiver(const juce::String& name, const juce::String& appName)
{
    SharedTextureReceiver* r = new SharedTextureReceiver(name, appName);
    receivers.add(r);
    return r;
}

void SharedTextureManager::removeSender(SharedTextureSender* sender, bool force)
{
    if (sender == nullptr) return;
    
    if (!force && (juce::OpenGLContext::getCurrentContext() == nullptr || !juce::OpenGLContext::getCurrentContext()->isActive()))
    {
        sendersToRemove.add(sender);
        return;
    }
    
    senders.removeObject(sender, false);
    listeners.call(&Listener::senderRemoved, sender);
    delete sender;
}

void SharedTextureManager::removeReceiver(SharedTextureReceiver* receiver, bool force)
{
    if (receiver == nullptr) return;
    if (!force && (juce::OpenGLContext::getCurrentContext() == nullptr || !juce::OpenGLContext::getCurrentContext()->isActive()))
    {
        receiversToRemove.add(receiver);
        return;
    }
    
    receivers.removeObject(receiver, false);
    listeners.call(&Listener::receiverRemoved, receiver);
    delete receiver;
}

void SharedTextureManager::initGL()
{
    for (auto& s : senders) s->initGL();
    for (auto& r : receivers) r->initGL();
    
    listeners.call(&Listener::GLInitialized);
}

void SharedTextureManager::renderGL()
{
    for (auto& s : sendersToRemove) removeSender(s);
    sendersToRemove.clear();
    
    for (auto& r : receiversToRemove) removeReceiver(r);
    receiversToRemove.clear();
    
    for (auto& s : senders) s->renderGL();
    for (auto& r : receivers) r->renderGL();
}

void SharedTextureManager::clearGL()
{
    for (auto& s : sendersToRemove) removeSender(s);
    sendersToRemove.clear();
    
    for (auto& r : receiversToRemove) removeReceiver(r);
    receiversToRemove.clear();
    
    
    for (auto& s : senders) s->clearGL();
    for (auto& r : receivers) r->clearGL();
    
    while (senders.size() > 0) removeSender(senders[0]);
    while (receivers.size() > 0) removeReceiver(receivers[0]);
}

juce::StringArray SharedTextureManager::getAvailableSenders()
{
    juce::StringArray serverList;

    #if JUCE_MAC
    SyphonServerDirectory* directory = [SyphonServerDirectory sharedDirectory];
    NSArray* servers = [directory servers];

    for (NSDictionary* serverDescription in servers)
    {
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
