#pragma once


#if JUCE_WINDOWS
class SpoutSender;
#endif

class SharedTextureSender
{
public:
	SharedTextureSender(const String &name, int width, int height, bool enabled = true);
	~SharedTextureSender();

#if JUCE_WINDOWS
	std::unique_ptr<SpoutSender> spoutSender;
#elif JUCE_MAC

#endif

	bool isInit;

	String sharingName;
	bool sharingNameChanged;
	bool enabled;


	Image image;
	OpenGLFrameBuffer *fbo;

	int width;
	int height;

	bool canDraw();

	void setSize(int w, int h);

	void initGL();
	void renderGL();
	void clearGL();

	void setSharingName(String value);
	void setEnabled(bool value);

	void createImageDefinition();
	void setupNativeSender(bool forceRecreation = false);

	class SharedTextureListener
	{
	public:
		virtual ~SharedTextureListener() {}
		virtual void drawSharedTexture(Graphics &g, juce::Rectangle<int> r) = 0;
	};

	ListenerList<SharedTextureListener> sharedTextureListeners;
	void addSharedTextureListener(SharedTextureListener* newListener) { sharedTextureListeners.add(newListener); }
	void removeSharedTextureListener(SharedTextureListener* listener) { sharedTextureListeners.remove(listener); }
};


#if JUCE_WINDOWS
class SpoutReceiver;
#endif

class SharedTextureReceiver
{
public:
	SharedTextureReceiver(const String &sharingName = String());
	~SharedTextureReceiver();

#if JUCE_WINDOWS
	std::unique_ptr<SpoutReceiver> receiver;
	char sharingNameArr[256];
#elif JUCE_MAC

#endif

	bool enabled;
	String sharingName;
	bool isInit;
	bool isConnected;

	unsigned int width;
	unsigned int height;
	bool invertImage;

	OpenGLContext context;

	Image image;
	OpenGLFrameBuffer * fbo;

	bool useCPUImage; //useful for manipulations like getPixelAt, but not optimized
	Image outImage;

	void setConnected(bool value);

	void setUseCPUImage(bool value);
	Image & getImage(); 
	
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

	ListenerList<Listener> listeners;
	void addListener(Listener* newListener) { listeners.add(newListener); }
	void removeListener(Listener* listener) { listeners.remove(listener); }

};

class SharedTextureManager
{
public:
	juce_DeclareSingleton(SharedTextureManager,true);

	SharedTextureManager();
	virtual ~SharedTextureManager();

	OwnedArray<SharedTextureSender> senders;
	OwnedArray<SharedTextureReceiver> receivers;

	HashMap<String, SharedTextureSender *> sendersMap;
	HashMap<String, SharedTextureReceiver *> receiversMap;

	SharedTextureSender * addSender(const String &name, int width, int height, bool enabled = true);
	SharedTextureReceiver * addReceiver(const String &name = String());

	void removeSender(SharedTextureSender * sender);
	void removeReceiver(SharedTextureReceiver * receiver);

	virtual void initGL();
	virtual void renderGL();
	virtual void clearGL();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTextureManager)


};