#include <qapplication.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include "mediaclient.h"

class Player : public QWidget
{
	Q_OBJECT
public:
	MediaClient *client;
	MediaServer *server;
	QLineEdit *le_url;
	QPushButton *pb_play, *pb_stop;
	bool playing;

	Player()
	{
		// set up GUI
		setCaption("TestPlay");
		QVBoxLayout *vb = new QVBoxLayout(this);
		VideoWidget *vw = media_createVideoWidget(this);
		vb->addWidget(vw);
		QHBoxLayout *hb = new QHBoxLayout(vb);
		le_url = new QLineEdit(this);
		hb->addWidget(le_url);
		pb_play = new QPushButton("&Play", this);
		connect(pb_play, SIGNAL(clicked()), SLOT(doplay()));
		hb->addWidget(pb_play);
		pb_stop = new QPushButton("&Stop", this);
		connect(pb_stop, SIGNAL(clicked()), SLOT(dostop()));
		hb->addWidget(pb_stop);
		QPushButton *pb = new QPushButton("&Close", this);
		connect(pb, SIGNAL(clicked()), SLOT(close()));
		hb->addWidget(pb);
		QLineEdit *le_serv = new QLineEdit(this);
		vb->addWidget(le_serv);
		le_url->setText("rtsp://media.real.com/showcase/marketing/rv10/mary_tori_amos_350k_320x.rmvb");
		pb_stop->setEnabled(false);
		vw->setMinimumSize(320, 240);
		pb_play->setFocus();
		le_serv->setReadOnly(true);

		// set up client
		client = new MediaClient;
		client->setVideoWidget(vw);
		connect(client, SIGNAL(connected()), SLOT(mc_connected()));
		connect(client, SIGNAL(starting()), SLOT(mc_starting()));
		connect(client, SIGNAL(finished()), SLOT(mc_finished()));
		connect(client, SIGNAL(error(int)), SLOT(mc_error(int)));
		playing = false;

		// server
		server = new MediaServer;
		QPtrList<MediaDevice> list = server->videoInputDevices();
		if(!list.isEmpty())
			server->setVideoInputDevice(list.getFirst());
		list = server->audioInputDevices();
		if(!list.isEmpty())
			server->setAudioInputDevice(list.getFirst());
		server->start(8011);
		QString url = server->createResource(VideoParams(), AudioParams());
		le_serv->setText(url);
		le_serv->setCursorPosition(0);
	}

	~Player()
	{
		delete client;
	}

private slots:
	void doplay()
	{
		le_url->setEnabled(false);
		pb_play->setEnabled(false);
		pb_stop->setEnabled(true);
		playing = true;
		client->play(le_url->text());
	}

	void dostop()
	{
		client->stop();
		le_url->setEnabled(true);
		pb_play->setEnabled(true);
		pb_stop->setEnabled(false);
	}

	void mc_connected()
	{
		printf("Connected!\n");
	}

	void mc_starting()
	{
		printf("Starting...\n");
	}

	void mc_finished()
	{
		playing = false;
		le_url->setEnabled(true);
		pb_play->setEnabled(true);
		pb_stop->setEnabled(false);
	}

	void mc_error(int x)
	{
		printf("Error: %d\n", x);

		playing = false;
		le_url->setEnabled(true);
		pb_play->setEnabled(true);
		pb_stop->setEnabled(false);
	}
};

#include"testplay.moc"

#ifdef Q_WS_X11
#include<X11/Xlib.h>
#endif

int main(int argc, char **argv)
{
#ifdef Q_WS_X11
	XInitThreads();
#endif

	QApplication app(argc, argv);

	QString base = "/home/justin/cvs/neo/helix/cvs/splay3/release";
	/*QString base;
	const char *p = getenv("HELIX_LIBS");
	if(p)
		base = QString::fromUtf8(QCString(p));
	else
		base = ".";*/

	if(media_loadPlugin("helix/libhelixplugin.so", base) != MEDIAPLUGIN_SUCCESS)
	{
		printf("Error loading helix/libhelixplugin.so\n");
		return 0;
	}

	Player *w = new Player;
	w->show();
	app.setMainWidget(w);
	app.exec();
	delete w;

	media_unloadPlugin();

	return 0;
}

