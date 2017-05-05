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
	ScopedPointer<SpoutSender> spoutSender;
#elif JUCE_MAC

#endif

	bool isInit;

	String sharingName;
	bool enabled;

	Image image;
	OpenGLFrameBuffer * fbo;

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
	SharedTextureReceiver(const String &sharingName = "Whatever");
	~SharedTextureReceiver();

#if JUCE_WINDOWS
	ScopedPointer<SpoutReceiver> receiver;
	char sharingNameArr[256];
#elif JUCE_MAC

#endif

	bool enabled;
	String sharingName;
	bool isInit;
	
	bool invertImage;

	Image image;
	Image outImage;
	OpenGLFrameBuffer * fbo;
	
	unsigned int width;
	unsigned int height;

	bool canDraw();

	void createReceiver();
	void createImageDefinition();
	void renderGL();

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
	SharedTextureReceiver * addReceiver(const String &name = "Whatever");

	void renderGL();
};