#include "mediaclient.h"

#include <qlibrary.h>
#include "mediaplugin.h"

MediaPlugin *plugin = 0;
QLibrary *plugin_lib = 0;

int media_loadPlugin(const QString &fname, const QString &resourcePath)
{
	if(plugin)
		media_unloadPlugin();

	QLibrary *lib = new QLibrary(fname);
	if(!lib->load())
	{
		delete lib;
		return MEDIAPLUGIN_ERRLOAD;
	}

	void *s;

	s = lib->resolve("version");
	if(!s)
	{
		delete lib;
		return MEDIAPLUGIN_ERRRESOLVE;
	}
	int (*version)() = (int (*)())s;
	if(version() != MEDIAPLUGIN_VERSION)
		return MEDIAPLUGIN_ERRVERSION;

	s = lib->resolve("createPlugin");
	if(!s)
	{
		delete lib;
		return MEDIAPLUGIN_ERRRESOLVE;
	}
	MediaPlugin *(*createPlugin)() = (MediaPlugin *(*)())s;
	MediaPlugin *p = createPlugin();
	if(!p)
	{
		delete lib;
		return MEDIAPLUGIN_ERRINIT;
	}
	if(!p->init(resourcePath))
	{
		delete p;
		delete lib;
		return MEDIAPLUGIN_ERRINIT;
	}

	plugin = p;
	plugin_lib = lib;
	return MEDIAPLUGIN_SUCCESS;
}

void media_unloadPlugin()
{
	delete plugin;
	plugin = 0;
	delete plugin_lib;
	plugin_lib = 0;
}

VideoWidget *media_createVideoWidget(QWidget *parent, const char *name)
{
	return plugin->createVideoWidget(parent, name);
}

//----------------------------------------------------------------------------
// VideoParams
//----------------------------------------------------------------------------
VideoParams::VideoParams()
{
	_fps = 0;
}

QString VideoParams::codec() const
{
	return _codec;
}

QSize VideoParams::size() const
{
	return _size;
}

int VideoParams::fps() const
{
	return _fps;
}

void VideoParams::setCodec(const QString &s)
{
	_codec = s;
}

void VideoParams::setSize(const QSize &s)
{
	_size = s;
}

void VideoParams::setFPS(int n)
{
	_fps = n;
}

//----------------------------------------------------------------------------
// AudioParams
//----------------------------------------------------------------------------
AudioParams::AudioParams()
{
	_rate = 0;
	_size = 0;
	_channels = 0;
}

QString AudioParams::codec() const
{
	return _codec;
}

int AudioParams::sampleRate() const
{
	return _rate;
}

int AudioParams::sampleSize() const
{
	return _size;
}

int AudioParams::channels() const
{
	return _channels;
}

void AudioParams::setCodec(const QString &s)
{
	_codec = s;
}

void AudioParams::setSampleRate(int n)
{
	_rate = n;
}

void AudioParams::setSampleSize(int n)
{
	_size = n;
}

void AudioParams::setChannels(int n)
{
	_channels = n;
}

//----------------------------------------------------------------------------
// MediaDevice
//----------------------------------------------------------------------------
MediaDevice::MediaDevice()
{
}

QString MediaDevice::name() const
{
	return QString();
}

//----------------------------------------------------------------------------
// MediaClient
//----------------------------------------------------------------------------
class MediaClient::Private
{
public:
	Private()
	{
		c = plugin->createClient();
	}

	~Private()
	{
		delete c;
	}

	MP_Client *c;
};

MediaClient::MediaClient(QObject *parent, const char *name)
:QObject(parent, name)
{
	d = new Private;
}

MediaClient::~MediaClient()
{
	delete d;
}

void MediaClient::setVideoWidget(VideoWidget *c)
{
	d->c->setVideoWidget(c);
}

void MediaClient::setRemoteAuth(const QString &user, const QString &pass)
{
	d->c->setRemoteAuth(user, pass);
}

void MediaClient::play(const QString &url)
{
	d->c->play(url);
}

void MediaClient::stop()
{
	d->c->stop();
}

void MediaClient::setVolume(int value)
{
	d->c->setVolume(value);
}

bool MediaClient::isPlaying() const
{
	return d->c->isPlaying();
}

QSize MediaClient::videoSize() const
{
	return d->c->videoSize();
}

//----------------------------------------------------------------------------
// MediaServer
//----------------------------------------------------------------------------
MediaServer::MediaServer(QObject *parent, const char *name)
:QObject(parent, name)
{
}

MediaServer::~MediaServer()
{
}

QPtrList<MediaDevice> MediaServer::videoInputDevices() const
{
	return QPtrList<MediaDevice>();
}

QPtrList<MediaDevice> MediaServer::audioInputDevices() const
{
	return QPtrList<MediaDevice>();
}

void MediaServer::setVideoInputDevice(MediaDevice *)
{
}

void MediaServer::setAudioInputDevice(MediaDevice *)
{
}

void MediaServer::setRequiredAuth(const QString &, const QString &)
{
}

bool MediaServer::isActive() const
{
	return false;
}

bool MediaServer::start(int)
{
	return false;
}

void MediaServer::stop()
{
}

QString MediaServer::createResource(const AudioParams &)
{
	return QString();
}

QString MediaServer::createResource(const VideoParams &, const AudioParams &)
{
	return QString();
}

void MediaServer::destroyResource(const QString &)
{
}
