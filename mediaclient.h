#ifndef MEDIACLIENT_H
#define MEDIACLIENT_H

#include <qwidget.h>

#ifdef Q_OS_WIN32
#  ifndef MEDIACLIENT_STATIC
#    ifdef MEDIACLIENT_MAKEDLL
#      define MEDIACLIENT_EXPORT __declspec(dllexport)
#    else
#      define MEDIACLIENT_EXPORT __declspec(dllimport)
#    endif
#  endif
#endif
#ifndef MEDIACLIENT_EXPORT
#define MEDIACLIENT_EXPORT
#endif

#ifdef Q_OS_WIN32
#  ifdef MEDIAPLUGIN_DLL
#    define MEDIAPLUGIN_EXPORT extern "C" __declspec(dllexport)
#  else
#    define MEDIAPLUGIN_EXPORT extern "C" __declspec(dllimport)
#  endif
#endif
#ifndef MEDIAPLUGIN_EXPORT
#define MEDIAPLUGIN_EXPORT extern "C"
#endif

typedef QWidget VideoWidget;

class MediaServer;

#define MEDIAPLUGIN_SUCCESS 0
#define MEDIAPLUGIN_ERRLOAD 1
#define MEDIAPLUGIN_ERRRESOLVE 2
#define MEDIAPLUGIN_ERRVERSION 3
#define MEDIAPLUGIN_ERRINIT 4

int media_loadPlugin(const QString &fname, const QString &resourcePath);
void media_unloadPlugin();
VideoWidget *media_createVideoWidget(QWidget *parent=0, const char *name=0);

// holds stream parameters
class VideoParams
{
public:
	VideoParams();

	QString codec() const;
	QSize size() const;
	int fps() const;

	void setCodec(const QString &s);
	void setSize(const QSize &s);
	void setFPS(int n);

private:
	QString _codec;
	QSize _size;
	int _fps;
};

// holds stream parameters
class AudioParams
{
public:
	AudioParams();

	QString codec() const;
	int sampleRate() const;
	int sampleSize() const;
	int channels() const;

	void setCodec(const QString &s);
	void setSampleRate(int n);
	void setSampleSize(int n);
	void setChannels(int n);

private:
	QString _codec;
	int _rate, _size, _channels;
};

// represents a device
class MediaDevice : public QObject
{
	Q_OBJECT
public:
	enum Type { Video, Audio };

	QString name() const;
	void setVolume(int value);

private:
	friend class MediaServer;
	MediaDevice();

	class Private;
	Private *d;
};

// connect and play a stream on a remote host
class MediaClient : public QObject
{
	Q_OBJECT
public:
	enum Error { ErrSystem, ErrConnect, ErrNeg, ErrPlay };
	MediaClient(QObject *parent=0, const char *name=0);
	~MediaClient();

	void setVideoWidget(VideoWidget *w);
	void setRemoteAuth(const QString &user, const QString &pass);

	void play(const QString &url);
	void stop();
	void setVolume(int value);

	bool isPlaying() const;
	QSize videoSize() const;

signals:
	void connected();
	void starting();
	void finished();
	void error(int);

private:
	class Private;
	Private *d;
};

// stream locally-captured content to a client
class MediaServer : public QObject
{
	Q_OBJECT
public:
	MediaServer(QObject *parent=0, const char *name=0);
	~MediaServer();

	QPtrList<MediaDevice> videoInputDevices() const;
	QPtrList<MediaDevice> audioInputDevices() const;
	void setVideoInputDevice(MediaDevice *dev);
	void setAudioInputDevice(MediaDevice *dev);

	void setRequiredAuth(const QString &user, const QString &pass);
	bool isActive() const;
	bool start(int port);
	void stop();

	QString createResource(const AudioParams &ap);
	QString createResource(const VideoParams &vp, const AudioParams &ap);
	void destroyResource(const QString &url);

signals:
	void starting(const QString &url);
	void finished(const QString &url);
	void error(const QString &url);

private:
	class Private;
	Private *d;
};

#endif

