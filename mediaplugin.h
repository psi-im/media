#ifndef MEDIAPLUGIN_H
#define MEDIAPLUGIN_H

#include "mediaclient.h"

#define MEDIAPLUGIN_VERSION 1

struct MP_ResourceParams
{
	QString v_codec;
	QSize v_size;
	int v_fps;
	QString a_codec;
	int a_rate, a_size, a_channels;
};

class MP_Client : public QObject
{
	Q_OBJECT
public:
	virtual ~MP_Client() {}

	virtual void setVideoWidget(VideoWidget *w) = 0;
	virtual void setRemoteAuth(const QString &user, const QString &pass) = 0;
	virtual void play(const QString &url) = 0;
	virtual void stop() = 0;
	virtual void setVolume(int value) = 0;
	virtual bool isPlaying() const = 0;
	virtual QSize videoSize() const = 0;

signals:
	void connected();
	void starting();
	void finished();
	void error(int);
};

class MP_Server : public QObject
{
	Q_OBJECT
public:
	virtual ~MP_Server() {}
};

class MediaPlugin
{
public:
	virtual ~MediaPlugin() {}

	virtual bool init(const QString &resourcePath) = 0;
	virtual QWidget *createVideoWidget(QWidget *parent, const char *name) = 0;
	virtual MP_Client *createClient() = 0;
	virtual MP_Server *createServer() = 0;
};

MEDIAPLUGIN_EXPORT int version();
MEDIAPLUGIN_EXPORT MediaPlugin *createPlugin();

#endif
