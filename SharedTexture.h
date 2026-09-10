#pragma once

#ifdef __OBJC__
@class SyphonOpenGLClient;
@class SyphonOpenGLServer;
#else
class SyphonOpenGLClient;
class SyphonOpenGLServer;
#endif

class SharedTextureSender
{
public:
	SharedTextureSender(const juce::String &name, int width, int height, bool enabled = true);
	~SharedTextureSender();

#if JUCE_WINDOWS
	SPOUTLIBRARY * sender;
#elif JUCE_MAC
    SyphonOpenGLServer* sender;
#endif

	bool isInit;

	juce::String sharingName;
	bool sharingNameChanged;
	bool enabled;
	juce::CriticalSection stateLock;


    std::function<void()> drawFunction;
	juce::Image image;
	juce::OpenGLFrameBuffer *fbo;
	juce::OpenGLFrameBuffer* externalFBO = nullptr;
	GLuint sharedTextureId = 0; // 0 is an invalid value for a texture id

	int width;
	int height;

	bool canDraw();

	void setSize(int w, int h);

	void setSharedTextureId(GLuint newTextureId);
	void setExternalFBO(juce::OpenGLFrameBuffer* newFBO);
    void setDrawFunction(std::function<void()> newDrawFunction);

	void initGL();
	void renderGL();
	void clearGL();

	void setSharingName(juce::String value);
	void setEnabled(bool value);

	void createImageDefinition();
	void setupNativeSender(bool forceRecreation = false);

	class SharedTextureListener
	{
	public:
		virtual ~SharedTextureListener() {}
		virtual void drawSharedTexture(juce::Graphics &g, juce::Rectangle<int> r) = 0;
	};

	juce::ListenerList<SharedTextureListener, juce::Array<SharedTextureListener*, juce::CriticalSection>> sharedTextureListeners;
	void addSharedTextureListener(SharedTextureListener* newListener) { sharedTextureListeners.add(newListener); }
	void removeSharedTextureListener(SharedTextureListener* listener) { sharedTextureListeners.remove(listener); }
};


#if JUCE_WINDOWS
class SpoutReceiver;
#endif

class SharedTextureReceiver
{
public:
	SharedTextureReceiver(const juce::String &sharingName = juce::String(), const juce::String &sharingAppName = juce::String());
	~SharedTextureReceiver();

#if JUCE_WINDOWS
	SPOUTLIBRARY * receiver;
#elif JUCE_MAC
    SyphonOpenGLClient* receiver;
#endif

	bool enabled;
	juce::String sharingName;
    juce::String sharingAppName; //for syphon
	bool isInit;
	bool isConnected;

	int width;
	int height;
	bool invertImage;
	juce::CriticalSection stateLock;

	juce::Image image;
	juce::OpenGLFrameBuffer * fbo;

	bool useCPUImage; //useful for manipulations like getPixelAt, but not optimized
	juce::Image outImage;
	bool createReceiverFailureLogged = false;

	void setSharingName(const juce::String& name, const juce::String& appName = "");

	void setConnected(bool value);

	void setUseCPUImage(bool value);
	juce::Image & getImage();

	bool canDraw();
	void createReceiver();
	void createImageDefinition();

	void initGL();
	void renderGL();
	void clearGL();


	class  Listener
	{
	public:
		virtual ~Listener() {}
		virtual void textureUpdated(SharedTextureReceiver *) {}
		virtual void connectionChanged(SharedTextureReceiver *) {}
	};

	juce::ListenerList<Listener, juce::Array<Listener*, juce::CriticalSection>> listeners;
	void addListener(Listener* newListener) { listeners.add(newListener); }
	void removeListener(Listener* listener) { listeners.remove(listener); }

};

class SharedTextureManager : public juce::DeletedAtShutdown
{
public:
	SharedTextureManager();
	virtual ~SharedTextureManager();
	static SharedTextureManager* getInstance();
	static SharedTextureManager* getInstanceWithoutCreating() noexcept;
	static void deleteInstance();

	juce::OwnedArray<SharedTextureSender, juce::CriticalSection> senders;
	juce::OwnedArray<SharedTextureReceiver, juce::CriticalSection> receivers;

	juce::Array<SharedTextureSender*, juce::CriticalSection> sendersToRemove;
	juce::Array<SharedTextureReceiver*, juce::CriticalSection> receiversToRemove;

	SharedTextureSender * addSender(const juce::String &name, int width, int height, bool enabled = true);
	SharedTextureReceiver * addReceiver(const juce::String &name = juce::String(), const juce::String &appName = juce::String());



	void removeSender(SharedTextureSender * sender, bool force = false);
	void removeReceiver(SharedTextureReceiver * receiver, bool force = false);

	virtual void initGL();
	virtual void renderGL();
	virtual void clearGL();

#if JUCE_WINDOWS
	SPOUTLIBRARY* senderDetect;
#endif

	void getAvailableSenderDetails(juce::StringArray& serverNames, juce::StringArray& appNames);
	juce::StringArray getAvailableSenders();

	class  Listener
	{
	public:
		virtual ~Listener() {}
		virtual void receiverRemoved(SharedTextureReceiver *) {}
		virtual void senderRemoved(SharedTextureSender *) {}
		virtual void GLInitialized() {}
	};

	juce::ListenerList<Listener, juce::Array<Listener*, juce::CriticalSection>> listeners;
	void addListener(Listener* newListener) { listeners.add(newListener); }
	void removeListener(Listener* listener) { listeners.remove(listener); }

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTextureManager)
};
// End of SharedTexture.h
