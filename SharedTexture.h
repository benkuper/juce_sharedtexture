#pragma once


#if JUCE_WINDOWS
class SpoutSender;
#endif

class SharedTextureSender
{
public:
	SharedTextureSender(const String &name);
	~SharedTextureSender();

#if JUCE_WINDOWS
	std::unique_ptr<SpoutSender> spoutSender;
#elif JUCE_MAC

#endif

	bool isInit;

	String sharingName;
	bool enabled;

	Image image;
	OpenGLFrameBuffer *fbo;

	int width;
	int height;

	bool canDraw();

	void setSize(int w, int h);
	void renderGL();

	void createImageDefinition();

	class  Listener
	{
	public:
		virtual ~Listener() {}
		virtual void drawSharedTexture(Graphics &g, juce::Rectangle<int> r) = 0;
	};

	ListenerList<Listener> listeners;
	void addListener(Listener* newListener) { listeners.add(newListener); }
	void removeListener(Listener* listener) { listeners.remove(listener); }
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
	SpoutReceiver * receiver;
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
	void renderGL();


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
	~SharedTextureManager();

	OwnedArray<SharedTextureSender> senders;
	OwnedArray<SharedTextureReceiver> receivers;

	HashMap<String, SharedTextureSender *> sendersMap;
	HashMap<String, SharedTextureReceiver *> receiversMap;

	SharedTextureSender * addSender(const String &name);
	SharedTextureReceiver * addReceiver(const String &name = String());

	void removeSender(SharedTextureSender * sender);
	void removeReceiver(SharedTextureReceiver * receiver);

	virtual void renderGL();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTextureManager)


};