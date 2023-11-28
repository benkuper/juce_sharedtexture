#pragma once


class SharedTextureSender
{
public:
	SharedTextureSender(const String &name, int width, int height, bool enabled = true);
	~SharedTextureSender();

#if JUCE_WINDOWS
	SPOUTLIBRARY * spoutSender;
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
	SPOUTLIBRARY * receiver;
#elif JUCE_MAC

#endif

	bool enabled;
	String sharingName;
	bool isInit;
	bool isConnected;

	int width;
	int height;
	bool invertImage;

	Image image;
	OpenGLFrameBuffer * fbo;

	bool useCPUImage; //useful for manipulations like getPixelAt, but not optimized
	Image outImage;

	void setSharingName(const String& name);

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

	Array<SharedTextureSender *> sendersToRemove;
	Array<SharedTextureReceiver *> receiversToRemove;

	SharedTextureSender * addSender(const String &name, int width, int height, bool enabled = true);
	SharedTextureReceiver * addReceiver(const String &name = String());

	void removeSender(SharedTextureSender * sender, bool force = false);
	void removeReceiver(SharedTextureReceiver * receiver, bool force = false);

	virtual void initGL();
	virtual void renderGL();
	virtual void clearGL();

	class  Listener
	{
	public:
		virtual ~Listener() {}
		virtual void receiverRemoved(SharedTextureReceiver *) {}
		virtual void senderRemoved(SharedTextureSender *) {}
		virtual void GLInitialized() {}
	};

	ListenerList<Listener> listeners;
	void addListener(Listener* newListener) { listeners.add(newListener); }
	void removeListener(Listener* listener) { listeners.remove(listener); }

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTextureManager)


};