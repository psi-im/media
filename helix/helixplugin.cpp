#include "helixplugin.h"

#include <qstring.h>
#include <qcstring.h>
#include <qmap.h>
#include <qpainter.h>
#include <qtimer.h>
#include <qlibrary.h>
#include <qfile.h>

#include "hxengin.h"
#include "hxcore.h"
#include "hxtypes.h"
#include "hxwintyp.h"
#include "hxwin.h"

#include "dllacces.h"
#include "dllpath.h"

#include "hxbuffer.h"
#include "hxmangle.h"
#include "hxclsnk.h"
#include "hxerror.h"
#include "hxprefs.h"
#include "hxstrutl.h"

#include "manager.h"
#include "helixinterfaces.h"

extern IHXSite* siteHXSite;
extern QWidget *siteWidget;

#define GUID_LEN 64

typedef HX_RESULT (HXEXPORT_PTR FPRMSETDLLACCESSPATH) (const char*);

static QByteArray convertMap(const QMap<QString,QCString> &m)
{
	// Create special path buffer.  Format is var=val, null delimited, double-null terminated.
	QByteArray buf(1);
	for(QMap<QString,QCString>::ConstIterator it = m.begin(); it != m.end(); ++it) {
		QCString cs = QCString(it.key().latin1()) + '=' + it.data();
		int len = cs.length() + 1;
		int oldsize = buf.size() - 1;
		buf.resize(oldsize + len + 1);
		memcpy(buf.data() + oldsize, cs.data(), len);
	}
	buf[buf.size()-1] = 0;
	return buf;
}

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------
class HelixManager;

class HelixPlugin : public MediaPlugin
{
public:
	HelixPlugin();
	~HelixPlugin();

	bool init(const QString &resourcePath);
	QWidget *createVideoWidget(QWidget *parent, const char *name);
	MP_Client *createClient();
	MP_Server *createServer();

	Manager *manager;
	QLibrary *clientLib;
	FPRMCREATEENGINE     CreateEngine;
	FPRMCLOSEENGINE      CloseEngine;
	FPRMSETDLLACCESSPATH SetDLLAccessPath;
	IHXClientEngine *clientEngine;
};

int version()
{
	return MEDIAPLUGIN_VERSION;
}

MediaPlugin *createPlugin()
{
	return new HelixPlugin;
}

// used to show video output
class HelixWidget : public QWidget
{
	Q_OBJECT
public:
	HelixWidget(QWidget *parent=0, const char *name=0);

	void adjustSiteSize();

	QWidget *site;

protected:
	// reimplemented
	void paintEvent(QPaintEvent *pe);
	void resizeEvent(QResizeEvent *re);

private:
	class Private;
	Private *d;
};

double vidAspect;
int vidWidth = 320;
int vidHeight = 220;

//----------------------------------------------------------------------------
// HelixWidget
//----------------------------------------------------------------------------
HelixWidget::HelixWidget(QWidget *parent, const char *name)
:QWidget(parent, name)
{
	vidAspect = (double)vidWidth / (double)vidHeight;

	//site = new QWidget(this);
	//site->hide();
}

void HelixWidget::paintEvent(QPaintEvent *pe)
{
	QPainter p(this);
	p.fillRect(pe->rect(), Qt::black);
}

void HelixWidget::resizeEvent(QResizeEvent *)
{
	adjustSiteSize();
}

void HelixWidget::adjustSiteSize()
{
	//site->show();
	if(siteHXSite)
	{
		int w = width();
		int h = height();
		double aspect = (double)w / (double)h;

		int ow, oh;
		if(aspect > vidAspect)
		{
			ow = h * vidAspect;
			oh = h;
		}
		else
		{
			ow = w;
			oh = w / vidAspect;
		}

		HXxSize size;
		size.cx = ow;
		size.cy = oh;
		siteHXSite->SetSize(size);
	}
}


class HelixClient : public MP_Client
{
	Q_OBJECT
public:
	HelixPlugin *plugin;
	IHXPlayer *player;
	MyClient *client;
	int lateError;

	HelixClient(HelixPlugin *p)
	{
		plugin = p;
		player = 0;
		client = 0;

		// create player
		if(plugin->clientEngine->CreatePlayer(player) != HXR_OK)
		{
			player = 0;
			return;
		}

		// create client context
		client = new MyClient;
		client->AddRef();
		QCString guid;
		guid.resize(GUID_LEN + 1);
		IHXPreferences *pPreferences = NULL;
		player->QueryInterface(IID_IHXPreferences, (void **)&pPreferences);
		client->Init(player, pPreferences, guid.data());
		player->SetClientContext(client);
		HX_RELEASE(pPreferences);

		// hook up the error sink
		IHXErrorSink *pErrorSink = NULL;
		IHXErrorSinkControl *pErrorSinkControl = NULL;
		player->QueryInterface(IID_IHXErrorSinkControl, (void **)&pErrorSinkControl);
		if(pErrorSinkControl) {
			client->QueryInterface(IID_IHXErrorSink, (void **)&pErrorSink);
			if(pErrorSink)
				pErrorSinkControl->AddErrorSink(pErrorSink, HXLOG_EMERG, HXLOG_INFO);
			HX_RELEASE(pErrorSink);
		}
		HX_RELEASE(pErrorSinkControl);

		connect(client, SIGNAL(siteReady()), SLOT(client_siteReady()));
		printf("created HelixClient successfully\n");
	}

	virtual ~HelixClient()
	{
		if(player)
		{
			player->Stop();

			printf("releasing client\n");
			client->Release();
			// FIXME: splay frees the client context before the player.  what's the right way??

			printf("closing player\n");
			plugin->clientEngine->ClosePlayer(player);
			printf("releasing player\n");
			player->Release();
		}
	}

	virtual void setVideoWidget(VideoWidget *w)
	{
		client->setWidget(w);
	}

	virtual void setRemoteAuth(const QString &user, const QString &pass)
	{
	}

	virtual void play(const QString &url)
	{
		if(player->OpenURL(url.latin1()) != HXR_OK)
		{
			lateError = MediaClient::ErrSystem;
			QTimer::singleShot(0, this, SLOT(doLateError()));
			return;
		}

		printf("opened url: [%s]\n", url.latin1());

		if(player->Begin() != HXR_OK)
		{
			lateError = MediaClient::ErrSystem;
			QTimer::singleShot(0, this, SLOT(doLateError()));
			return;
		}

		plugin->manager->start();
		printf("playing\n");
	}

	virtual void stop()
	{
		player->Stop();
		printf("stopping\n");
	}

	virtual void setVolume(int value)
	{
	}

	virtual bool isPlaying() const
	{
	}

	virtual QSize videoSize() const
	{
	}

private slots:
	void client_siteReady()
	{
		printf("Site Ready\n");
		((HelixWidget *)siteWidget)->adjustSiteSize();
	}

	void doLateError()
	{
		error(lateError);
	}
};

//----------------------------------------------------------------------------
// HelixPlugin
//----------------------------------------------------------------------------
HelixPlugin::HelixPlugin()
{
	clientLib = 0;
	clientEngine = 0;
	manager = 0;

	printf("loaded\n");
}

HelixPlugin::~HelixPlugin()
{
	delete manager;

	if(clientLib) {
		printf("closing engine\n");
		CloseEngine(clientEngine);
		printf("closing lib\n");
		delete clientLib;
	}

	printf("unloaded\n");
}

bool HelixPlugin::init(const QString &base)
{
	printf("resourcePath: [%s]\n", base.latin1());

	QString path = base + '/' + "clntcore.so";
	clientLib = new QLibrary(path);
	printf("Opening [%s]\n", path.latin1());
	if(!clientLib->load()) {
		printf("Unable to load client core.\n");
		delete clientLib;
		clientLib = 0;
		return false;
	}

	CreateEngine = (FPRMCREATEENGINE)clientLib->resolve("CreateEngine");
	CloseEngine = (FPRMCLOSEENGINE)clientLib->resolve("CloseEngine");
	SetDLLAccessPath = (FPRMSETDLLACCESSPATH)clientLib->resolve("SetDLLAccessPath");

	if(!CreateEngine || !CloseEngine || !SetDLLAccessPath)
		return false;

	QMap<QString,QCString> paths;
	QCString eh = QFile::encodeName(base);
	paths["DT_Common"] = eh;
	paths["DT_Plugins"] = eh;
	paths["DT_Codecs"] = eh;

	// set the paths
	printf("Common DLL path %s\n", paths["DT_Common"].data());
	printf("Plugin path %s\n", paths["DT_Plugins"].data());
	printf("Codec path %s\n", paths["DT_Codecs"].data());
	SetDLLAccessPath((char *)convertMap(paths).data());

	// create client engine
	if(CreateEngine((IHXClientEngine**)&clientEngine) != HXR_OK)
	{
		clientEngine = 0;
		return false;
	}

	printf("created client engine\n");
	manager = new Manager(clientEngine);
	return true;
}

QWidget *HelixPlugin::createVideoWidget(QWidget *parent, const char *name)
{
	return new HelixWidget(parent, name);
}

MP_Client *HelixPlugin::createClient()
{
	return new HelixClient(this);
}

MP_Server *HelixPlugin::createServer()
{
	return 0;
}

#include "helixplugin.moc"

#if 0
#include<qapplication.h>
#include<qwidget.h>
#include<qpushbutton.h>
#include<qlibrary.h>
#include<qfile.h>
#include<qtimer.h>
#include<qmap.h>
#include<qurl.h>
#include<stdlib.h>

#include "rtspproxy.h"

#include "ihxtprofile.h"

#include"hxengin.h"
#include"hxcore.h"
#include"dllacces.h"
#include"dllpath.h"
#include<unistd.h>

#include "hxcom.h"
#include "hxtypes.h"
#include "hxwintyp.h"
#include "hxwin.h"
#include "ihxpckts.h"
#include "hxcomm.h"

#include "hxbuffer.h"
#include "hxmangle.h"

#include "hxclsnk.h"
#include "hxerror.h"
#include "hxprefs.h"
#include "hxstrutl.h"

#include"hxengin.h"
#include"hxcore.h"

#include "ihxtencodingjob.h"

#include "helixinterfaces.h"

QWidget *siteWidget = 0;
extern IHXSite *siteHXSite;

typedef HX_RESULT (HXEXPORT_PTR FPRMSETDLLACCESSPATH) (const char*);

IHXClientEngine* g_engine;
IHXPlayer *g_player;

class HelixManager::Private
{
public:
	QLibrary *lib, *lib2;
	FPRMCREATEENGINE     CreateEngine;
	FPRMCLOSEENGINE      CloseEngine;
	FPRMSETDLLACCESSPATH SetDLLAccessPath;

	FPCREATEJOBFACTORY CreateClassFactory;
	FPRMBUILDSETDLLACCESSPATH SetDLLAccessPath2;

	IHXTClassFactory *factory;
	IHXTEncodingJob *job;

	IHXClientEngine *engine;
	IHXPlayer *player;
	MyClient *client;
};

HelixManager::HelixManager()
{
	d = new Private;
	d->lib = 0;
	d->lib2 = 0;
}

HelixManager::~HelixManager()
{
	if(d->lib) {
		d->player->Stop();

		printf("releasing client\n");
		d->client->Release();
		// FIXME: splay frees the client context before the player.  what's the right way??

		printf("closing player\n");
		d->engine->ClosePlayer(d->player);
		printf("releasing player\n");
		d->player->Release();

		printf("closing engine\n");
		d->CloseEngine(d->engine);
		printf("closing lib\n");
		delete d->lib;
		printf("unloaded\n");
	}

	if(d->lib2)
		delete d->lib2;

	delete d;
}

const int GUID_LEN           = 64;

QString murl;

bool HelixManager::load()
{
	QString base;
	const char *p = getenv("HELIX_LIBS");
	if(p)
		base = QString::fromUtf8(QCString(p));
	else
		base = ".";

	QString path = base + '/' + "clntcore.so";
	d->lib = new QLibrary(path);
	printf("Opening [%s]\n", path.latin1());
	if(!d->lib->load()) {
		printf("Unable to load client core.  Be sure to set HELIX_LIBS.\n");
		delete d->lib;
		d->lib = 0;
		return false;
	}

	d->CreateEngine = (FPRMCREATEENGINE)d->lib->resolve("CreateEngine");
	d->CloseEngine = (FPRMCLOSEENGINE)d->lib->resolve("CloseEngine");
	d->SetDLLAccessPath = (FPRMSETDLLACCESSPATH)d->lib->resolve("SetDLLAccessPath");

	if(!d->CreateEngine || !d->CloseEngine || !d->SetDLLAccessPath)
		return false;

	QMap<QString,QCString> paths;
	QCString eh = QFile::encodeName(base);
	paths["DT_Common"] = eh;
	paths["DT_Plugins"] = eh;
	paths["DT_Codecs"] = eh;

	// set the paths
	printf("Common DLL path %s\n", paths["DT_Common"].data());
	printf("Plugin path %s\n", paths["DT_Plugins"].data());
	printf("Codec path %s\n", paths["DT_Codecs"].data());
	d->SetDLLAccessPath((char *)convertMap(paths).data());

	// create client engine
	if(d->CreateEngine((IHXClientEngine**)&d->engine) != HXR_OK)
		return false;

	printf("created client engine\n");

	// create player
        if(d->engine->CreatePlayer(d->player) != HXR_OK)
		return false;

	printf("created player\n");

	// create client context
	d->client = new MyClient;
	d->client->AddRef();
	char pszGUID[GUID_LEN + 1]; /* Flawfinder: ignore */ // add 1 for terminator
	pszGUID[0] = 0;
	IHXPreferences *pPreferences = NULL;
	d->player->QueryInterface(IID_IHXPreferences, (void **)&pPreferences);
	d->client->Init(d->player, pPreferences, pszGUID);
	d->player->SetClientContext(d->client);
	HX_RELEASE(pPreferences);

	// hook up the error sink
	IHXErrorSink *pErrorSink = NULL;
	IHXErrorSinkControl *pErrorSinkControl = NULL;
	d->player->QueryInterface(IID_IHXErrorSinkControl, (void **)&pErrorSinkControl);
	if(pErrorSinkControl) {
		d->client->QueryInterface(IID_IHXErrorSink, (void **)&pErrorSink);
		if(pErrorSink)
			pErrorSinkControl->AddErrorSink(pErrorSink, HXLOG_EMERG, HXLOG_INFO);
		HX_RELEASE(pErrorSink);
	}
	HX_RELEASE(pErrorSinkControl);

	// RTSP port is 554

	//QString url = "rtsp://media.real.com/showcase/marketing/rv10/mary_tori_amos_350k_320x.rmvb";
	QString url = murl; //"rtsp://localhost:16000/showcase/marketing/rv10/mary_tori_amos_350k_320x.rmvb";

        /*if(d->player->OpenURL(url.latin1()) != HXR_OK)
		return false;

	printf("opened url: [%s]\n", url.latin1());

	if(d->player->Begin() != HXR_OK)
		return false;

	printf("starting playback\n");*/

	g_engine = d->engine;
	g_player = d->player;


	base = "/home/justin/cvs/neo/helix/cvs/prod/release";
	path = base + '/' + "encsession.so";
	d->lib2 = new QLibrary(path);
	printf("Opening [%s]\n", path.latin1());
	if(!d->lib2->load()) {
		printf("Unable to load client core.  Be sure to set HELIX_LIBS.\n");
		delete d->lib2;
		d->lib2 = 0;
		return false;
	}

	d->CreateClassFactory = (FPCREATEJOBFACTORY)d->lib2->resolve("HXTCreateJobFactory");
	d->SetDLLAccessPath2 = (FPRMBUILDSETDLLACCESSPATH)d->lib2->resolve("SetDLLAccessPath");

	if(!d->CreateClassFactory || !d->SetDLLAccessPath2)
		return false;

	paths.clear();
	eh = QFile::encodeName(base);
	paths["DT_Common"] = eh;
	paths["DT_Plugins"] = eh;
	paths["DT_Codecs"] = eh;
	paths["DT_EncSDK"] = eh;

	// set the paths
	printf("Common DLL path %s\n", paths["DT_Common"].data());
	printf("Plugin path %s\n", paths["DT_Plugins"].data());
	printf("Codec path %s\n", paths["DT_Codecs"].data());
	printf("EncSDK path %s\n", paths["DT_EncSDK"].data());
	d->SetDLLAccessPath2((char *)convertMap(paths).data());

	if(d->CreateClassFactory((IHXTClassFactory**)&d->factory) != HXR_OK)
		return false;

	printf("created class factory for encoder\n");

	// --- create job
	HX_RESULT res = HXR_OK;
	// Use the class factory to create the encoding job object
	if (SUCCEEDED(res))
		res = d->factory->CreateInstance(IID_IHXTEncodingJob, (IUnknown**)&d->job);
	// Register event sink
	if (SUCCEEDED(res))
	{
		printf("created job object\n");
		// Create event sink helper class
		/*m_pEventSink = new CSampleEventSink;
		m_pEventSink->AddRef();

		// Get the event manager
		IHXTEventManager* pEventMgr = NULL;
		res = m_pJob->GetEventManager(&pEventMgr);

		// Subscribe for events
		if (SUCCEEDED(res))
		res = pEventMgr->Subscribe(m_pEventSink);

		HX_RELEASE(pEventMgr);*/
	}

	// --- set up input
	{
    HX_RESULT res = HXR_OK;

    // Get input pathname
    //cout << "  Enter input pathname: ";
    //char szInputPathname[1024] = "in.dat";
    //cin >> szInputPathname;

    // Create the property bag used to initialize the input
    IHXTPropertyBag* pInitParams = NULL;
    if (SUCCEEDED(res))
        res = d->factory->CreateInstance(IID_IHXTPropertyBag, (IUnknown**)&pInitParams);

    // Set the plugin type
    // Note that kPropPluginName is not set -- this allows the Producer SDK to pick the optimal
    // input reader plugin to use from the set of all input reader plugins
    if (SUCCEEDED(res))
	res = pInitParams->SetString(kPropPluginType, kValuePluginTypeInputCapture);
    else
    	printf("failed to create property instance\n");

	pInitParams->SetString(kPropPluginName, kValuePluginNameCaptureAV);
    //pInitParams->SetInt(kPropDuration, kValueTimeInfiniteInt);
    //pInitParams->setString(kPropAudioDeviceID, kValueTimeInfiniteInt);

            // Create time object
            IUnknown* pUnk = NULL;
            res = d->factory->CreateInstance(IID_IHXTTime, &pUnk);

            IHXTTime* pTime = NULL;
            if (SUCCEEDED(res))
                res = pUnk->QueryInterface(IID_IHXTTime, (void**)&pTime);

            if (SUCCEEDED(res))
                res = pTime->SetTime(30000);

            // Add duration to the init bag
            if (SUCCEEDED(res))
                res = pInitParams->SetUnknown(kPropDuration, pUnk);

            HX_RELEASE(pTime);
            HX_RELEASE(pUnk);

{
	const char* szCaptureType = kValueCaptureMediaTypeAudioCapture;
	const char* szDeviceIDType = kPropAudioDeviceID;

    HX_RESULT res = HXR_OK;

    // Get access to the capture device enumerator
    IHXTPluginInfoManager* pCaptureDeviceManager = NULL;
    res = d->factory->CreateInstance(CLSID_IHXTCaptureDeviceInfoManager, (IUnknown**)&pCaptureDeviceManager);

    // Create property bag -- used to specify enumeration category
    IHXTPropertyBag* pQueryPropertyBag = NULL;
    if (SUCCEEDED(res)) {
    	printf("created capture input info manager\n");
	res = d->factory->CreateInstance(IID_IHXTPropertyBag, (IUnknown**)&pQueryPropertyBag);
    }

    // Search for all capture devices of type szCaptureType
    if (SUCCEEDED(res)) {
    	printf("created capture type bag\n");
	res = pQueryPropertyBag->SetString(kPropCaptureMediaType, szCaptureType);
    }

    IHXTPluginInfoEnum* pPluginInfoEnum = NULL;
    if (SUCCEEDED(res)) {
    	printf("set a string in the bag\n");
	res = pCaptureDeviceManager->GetPluginInfoEnum(pQueryPropertyBag, &pPluginInfoEnum);
    }

    // Display all capture devices that were found
    if (SUCCEEDED(res))
    {
    	printf("[%s] devices: %d\n", szCaptureType, (int)pPluginInfoEnum->GetCount());
	for (int i = 0; i< (int)pPluginInfoEnum->GetCount(); i++)
	{
	    // Get info about the current device
	    IHXTPropertyBag* pCurrentPluginInfo = NULL;
	    HX_RESULT resEnum = pPluginInfoEnum->GetPluginInfoAt(i, &pCurrentPluginInfo);

	    // Get the device name
	    const char* szDeviceID = NULL;
	    if (SUCCEEDED(resEnum))
		resEnum = pCurrentPluginInfo->GetString(szDeviceIDType, &szDeviceID);

	    if (SUCCEEDED(resEnum))
		printf("  %d:[%s]\n", i, szDeviceID);

	    HX_RELEASE(pCurrentPluginInfo);
	}
    }

    HX_RELEASE(pCaptureDeviceManager);
    HX_RELEASE(pQueryPropertyBag);
    HX_RELEASE(pPluginInfoEnum);
}

    pInitParams->SetString(kPropAudioDeviceID, "0");

    // Set the pathname
    //if (SUCCEEDED(res))
	//res = pInitParams->SetString(kPropInputPathname, szInputPathname);

    // Create the input
    IHXTInput* pInput = NULL;
    if (SUCCEEDED(res))
	res = d->factory->BuildInstance(IID_IHXTInput, pInitParams, (IUnknown**)&pInput);
    else
    	printf("failed to set string in properties\n");

    // Set the input on the encoding job
    if (SUCCEEDED(res))
	res = d->job->SetInput(pInput);
    else
    	printf("failed to create input instance\n");

    // Print name of input plugin
    if (SUCCEEDED(res))
    {
	const char* szPluginName = "";
	res = pInput->GetString(kPropPluginName, &szPluginName);

	if (SUCCEEDED(res))
	    printf("Using input plugin: %s\n", szPluginName);
    }

    HX_RELEASE(pInput);
    HX_RELEASE(pInitParams);
    if(res != HXR_OK)
	return false;
	}

	// --- set up output
	{
    HX_RESULT res = HXR_OK;

    // Create the output profile
    IHXTOutputProfile* pOutputProfile = NULL;
    if (SUCCEEDED(res))
	res = d->factory->BuildInstance(IID_IHXTOutputProfile, NULL, (IUnknown**)&pOutputProfile);

    if (SUCCEEDED(res))
	res = d->job->AddOutputProfile(pOutputProfile);

    // Get the destination pathname
    char szOutputPathname[1024] = "out.dat";

    // Create the property bag used to initialize the destination
    IHXTPropertyBag* pInitParams = NULL;
    if (SUCCEEDED(res))
        res = d->factory->CreateInstance(IID_IHXTPropertyBag, (IUnknown**)&pInitParams);

    // Set the plugin type
    if (SUCCEEDED(res))
	res = pInitParams->SetString(kPropPluginType, kValuePluginTypeDestinationFile);

    // Set the plugin name
    if (SUCCEEDED(res))
	res = pInitParams->SetString(kPropPluginName, kValuePluginNameFileDestRealMedia);

    // Set the pathname
    if (SUCCEEDED(res))
	res = pInitParams->SetString(kPropOutputPathname, szOutputPathname);

    // Create the destination
    IHXTDestination* pDest = NULL;
    if (SUCCEEDED(res))
	res = d->factory->BuildInstance(IID_IHXTDestination, pInitParams, (IUnknown**)&pDest);

    // Set the destination on the output profile
    if (SUCCEEDED(res))
	res = pOutputProfile->AddDestination(pDest);

    HX_RELEASE(pOutputProfile);
    HX_RELEASE(pDest);
    HX_RELEASE(pInitParams);

    if(res != HXR_OK)
    	return false;
    //return res;
	}

	// --- media profile
	{
    HX_RESULT res = HXR_OK;

    // Get the output profile
    IHXTOutputProfile* pOutputProfile = NULL;
    res = d->job->GetOutputProfile(0, &pOutputProfile);

    // Create the media profile
    IHXTMediaProfile* pMediaProfile = NULL;
    if (SUCCEEDED(res))
        res = d->factory->BuildInstance(IID_IHXTMediaProfile, NULL, (IUnknown**)&pMediaProfile);

    // Set the media profile
    if (SUCCEEDED(res))
	res = pOutputProfile->SetMediaProfile(pMediaProfile);

    // Get the current video mode
    if (SUCCEEDED(res))
    {
	const char* szVideoMode = "";
	res = pMediaProfile->GetString(kPropVideoMode, &szVideoMode);

	//if (SUCCEEDED(res))
	  //  cout << "  Current video mode: " << szVideoMode << endl;
    }

    // Set the video encoding mode
    UINT32 ulVideo = 0;
    /*cout << "  Please enter the video mode you would like for encoding.\n    " \
	"0) Normal\n    1) Smoothest Motion\n    2) Sharpest Image\n    3) Slideshow\n? ";
    cin >> ulVideo;*/

    if (SUCCEEDED(res))
    {
	if (ulVideo == 0)
	    res = pMediaProfile->SetString(kPropVideoMode, kValueVideoModeNormal);
	else if (ulVideo == 1)
	    res = pMediaProfile->SetString(kPropVideoMode, kValueVideoModeSmooth);
	else if (ulVideo == 2)
	    res = pMediaProfile->SetString(kPropVideoMode, kValueVideoModeSharp);
	else if (ulVideo == 3)
	    res = pMediaProfile->SetString(kPropVideoMode, kValueVideoModeSharp);
	else
	    res = HXR_FAIL;
    }

    // Get the current audio mode
    if (SUCCEEDED(res))
    {
	const char* szAudioMode = "";
	res = pMediaProfile->GetString(kPropAudioMode, &szAudioMode);

	//if (SUCCEEDED(res))
	  //  cout << "  Current audio mode: " << szAudioMode << endl;
    }

    // Set the audio encoding mode
    UINT32 ulAudio = 0;
    /*cout << "  Please enter the audio mode you would like for encoding.\n" \
	"    0) Music\n    1) Voice\n? ";
    cin >> ulAudio;*/

    if (SUCCEEDED(res))
    {
	if (ulAudio == 0)
	    res = pMediaProfile->SetString(kPropAudioMode, kValueAudioModeMusic);
	else if (ulAudio == 1)
	    res = pMediaProfile->SetString(kPropAudioMode, kValueAudioModeVoice);
	else
	    res = HXR_FAIL;
    }


    HX_RELEASE(pOutputProfile);
    HX_RELEASE(pMediaProfile);

    if(res != HXR_OK)
    	return false;
    //return res;
	}

	// --- audiences
	{
    HX_RESULT res = HXR_OK;

    // Get the output profile
    IHXTOutputProfile* pOutputProfile = NULL;
    res = d->job->GetOutputProfile(0, &pOutputProfile);

    // Get the media profile
    IHXTMediaProfile* pMediaProfile = NULL;
    if (SUCCEEDED(res))
	res = pOutputProfile->GetMediaProfile(&pMediaProfile);

    // Create the audience enumerator
    IHXTAudienceEnumerator* pAudienceEnum = NULL;
    if (SUCCEEDED(res))
	res = d->factory->CreateInstance(IID_IHXTAudienceEnumerator, (IUnknown**)&pAudienceEnum);

    if (SUCCEEDED(res))
    {
	res = pAudienceEnum->SetProfileDirectory("audiences");
    }

    // Get the number of audiences in the profile directory
    UINT32 ulNumAudiences = 0;
    if (SUCCEEDED(res))
	ulNumAudiences = pAudienceEnum->GetAudienceCount();

    // Print all of the available audiences
    //cout << "  Available audiences:" << endl;
    for (UINT32 i=0; i<ulNumAudiences && SUCCEEDED(res); i++)
    {
	IHXTAudience* pAudience = NULL;
	res = pAudienceEnum->GetAudience(i, &pAudience, NULL);

	if (SUCCEEDED(res))
	{
            const char* szName = "";
            res = pAudience->GetString(kPropObjectName, &szName);

	    //cout << "    " << i << ") " << szName << endl;
	}

	HX_RELEASE(pAudience);
    }

    // Select audiences to add to the media profile
    if (SUCCEEDED(res))
    {
	//cout << "  Select an audience to add to your encode, -1 to stop adding audiences." << endl;
	//cout << "  Select multiple audiences to create a SureStream encode." << endl;

	INT32 nAudience = -1;
	do
	{
	    //cin >> nAudience;
	    if (nAudience != -1)
	    {
		// Get the audience from the enumerator
		IHXTAudience* pAudience = NULL;
		res = pAudienceEnum->GetAudience((UINT32)nAudience, &pAudience, NULL);

		// Add the audience to the media profile
		if (SUCCEEDED(res))
		    res = pMediaProfile->AddAudience(pAudience);

		HX_RELEASE(pAudience);
	    }

	} while (nAudience != -1 && SUCCEEDED(res));
    }

    // Set some a/v codec properties
    if (SUCCEEDED(res))
    {
	// Get max startup latency
	UINT32 ulMaxStartupLatency = 4;
	//cout << "  Enter max video startup latency in seconds (typical value is 4): ";
	//cin >> ulMaxStartupLatency;

	// Get max time between keyframes
	UINT32 ulMaxTimeBetweenKeyframes = 10;
	//cout << "  Enter max time between video keyframes in seconds (typical value is 10): ";
	//cin >> ulMaxTimeBetweenKeyframes;

	// Get encoding complexity
	UINT32 ulEncodeComplexity = 0;
	/*cout << "  Please enter the encoding complexity (speed vs. quality tradeoff):\n    " \
	    "0) Low\n    1) Medium\n    2) High\n? ";
	cin >> ulEncodeComplexity;*/

	// Set the values on the appropriate streams within each audience
	UINT32 ulNumAudiences = pMediaProfile->GetAudienceCount();
	for (UINT32 i=0; i<ulNumAudiences && SUCCEEDED(res); i++)
	{
	    // Get the audience
	    IHXTAudience* pAudience = 0;
	    res = pMediaProfile->GetAudience(i, &pAudience);

	    if (SUCCEEDED(res))
	    {
		// Iterate through all streams within the audience
		UINT32 ulNumStreams = pAudience->GetStreamConfigCount();
		for (UINT32 j=0; j<ulNumStreams && SUCCEEDED(res); j++)
		{
		    // Get the stream
		    IHXTStreamConfig* pStream = NULL;
		    res = pAudience->GetStreamConfig(j, &pStream);

		    // Check if the current stream is a video stream
		    if (SUCCEEDED(res))
		    {
			const char* szStreamType = "";
			res = pStream->GetString(kPropPluginType, &szStreamType);

			// Set max startup latency and max time between keyframes on all video streams
			if (SUCCEEDED(res) && strcmp(szStreamType, kValuePluginTypeVideoStream) == 0)
			{
			    res = pStream->SetDouble(kPropMaxStartupLatency, ulMaxStartupLatency);

			    if (SUCCEEDED(res))
				res = pStream->SetDouble(kPropMaxTimeBetweenKeyFrames, ulMaxTimeBetweenKeyframes);
			}
		    }

		    // Set encoding complexity on audio/video streams
		    if (SUCCEEDED(res))
		    {
			switch (ulEncodeComplexity)
			{
			case 0:
			    res = pStream->SetString(kPropEncodingComplexity, kValueEncodingComplexityLow);
			    break;
			case 1:
			    res = pStream->SetString(kPropEncodingComplexity, kValueEncodingComplexityMedium);
			    break;
			default:
			case 2:
			    res = pStream->SetString(kPropEncodingComplexity, kValueEncodingComplexityHigh);
			    break;
			}
		    }

		    HX_RELEASE(pStream);
		}
	    }

	    HX_RELEASE(pAudience);
	}
    }

    HX_RELEASE(pOutputProfile);
    HX_RELEASE(pMediaProfile);
    HX_RELEASE(pAudienceEnum);

    if(res != HXR_OK)
    	return false;
    //return res;
	}

	// --- setup job
	{
    HX_RESULT res = HXR_OK;

    char szInput[1024] = "";

    // Check if the encode should be two-pass
    //cout << "  Perform two-pass encode (y/n)?: ";
    //cin >> szInput;

    // Set two-pass encode state
    if (szInput[0] == 'y' || szInput[0] == 'Y')
    {
	res = d->job->SetBool(kPropEnableTwoPass, TRUE);
    }

    // Get metadata property bag
    IHXTPropertyBag* pMetadata = NULL;
    if (SUCCEEDED(res))
	res = d->job->GetMetadata(&pMetadata);

    // Get the title
    if (SUCCEEDED(res))
    {
	//cout << "  Enter clip title: ";
	//cin >> szInput;
	strcpy(szInput, "foo title");
    }

    // Set the title
    if (SUCCEEDED(res))
	res = pMetadata->SetString(kPropTitle, szInput);

    // Get the author
    if (SUCCEEDED(res))
    {
	//cout << "  Enter clip author: ";
	//cin >> szInput;
	strcpy(szInput, "foo author");
    }

    // Set the author
    if (SUCCEEDED(res))
	res = pMetadata->SetString(kPropAuthor, szInput);


    HX_RELEASE(pMetadata);

    if(res != HXR_OK)
    	return false;
    //return res;
	}

	// --- start encoding
	{
    HX_RESULT res = HXR_OK;

    printf("setup complete.  ready to encode\n");
    // Start the encode -- this call will block until encoding has completed
    /*if (SUCCEEDED(res))
	res = d->job->StartEncoding();*/

    // Unsubscribe the event sink -- not needed anymore.  Preserve the StartEncoding() return code.
    /*if (m_pEventSink)
    {
	// Get the event manager
	IHXTEventManager* pEventMgr = NULL;
	HX_RESULT resEvent = m_pJob->GetEventManager(&pEventMgr);

	// Unsubscribe event sink
	if (SUCCEEDED(resEvent))
	    resEvent = pEventMgr->Unsubscribe(m_pEventSink);

	HX_RELEASE(pEventMgr);
	HX_RELEASE(m_pEventSink);
    }*/

    if(res != HXR_OK)
    	return false;
    //return res;
	}

/*HX_RESULT CEncoderApp::Shutdown()
{
    cout << "Step 12: Shutting down sdk" << endl;

    if (m_pFileLogObserver)
    {
	m_pFileLogObserver->Shutdown();
	HX_RELEASE(m_pFileLogObserver);
    }

    HX_RELEASE(m_pEventSink);
    HX_RELEASE(m_pJob);

    // Note that the log system must be shutdown/released prior to releasing the class factory (because the
    // class factory is holding the log system DLL in memory)
    if (m_pLogSystem)
    {
	m_pLogSystem->Shutdown();
	HX_RELEASE(m_pLogSystem);
    }

    HX_RELEASE(m_pFactory);


    if (m_RmsessionDLL)
    {
#ifdef _WINDOWS
	::FreeLibrary(m_RmsessionDLL);
#elif defined _LINUX
	dlclose(m_RmsessionDLL);
#elif defined _MAC_UNIX
	CFBundleUnloadExecutable(m_RmsessionDLL);
	CFRelease(m_RmsessionDLL);
#endif
    }

    return HXR_OK;
}*/
	return true;
}

#include"mediamanager.moc"

#ifdef Q_WS_X11
#include<X11/Xlib.h>
#endif

#include "rtspbase.h"

int main(int argc, char **argv)
{
#ifdef Q_WS_X11
	XInitThreads();
#endif
	QApplication app(argc, argv);

	/*{
		QString str = "x-real-rdt/mcast;client_port=8000;mode=play,x-real-rdt/udp;client_port=8000;mode=play,x-pn-tng/udp;client_port=8000;mode=play,RTP/AVP;unicast;client_port=8000-8001;mode=play,x-pn-tng/tcp;mode=play,x-real-rdt/tcp;mode=play,RTP/AVP/TCP;unicast;mode=\"play\"";
		printf("orig: [%s]\n", str.latin1());
		RTSP::TransportList list;
		list.fromString(str);
		for(RTSP::TransportList::ConstIterator it = list.begin(); it != list.end(); ++it)
		{
			printf("Name: [%s]\n", (*it).name().latin1());
			RTSP::TransportArgs args = (*it).arguments();
			for(RTSP::TransportArgs::ConstIterator it2 = args.begin(); it2 != args.end(); ++it2)
			{
				if(!(*it2).value.isNull())
					printf("  Arg: [%s], [%s]\n", (*it2).name.latin1(), (*it2).value.latin1());
				else
					printf("  Arg: [%s]\n", (*it2).name.latin1());
			}
		}
		printf("as string: [%s]\n", list.toString().latin1());
	}*/

	QWidget *top = new QWidget;
	top->resize(800,600);
	QPushButton *pb = new QPushButton("Press me for nothing", top);
	pb->move(20, 20);
	pb->resize(pb->sizeHint());
	QWidget *view = new QWidget(top);
	view->move(80, 60);
	view->resize(640, 480);
	top->show();
	siteWidget = view;

	App *a = new App;
	HelixManager *h = new HelixManager;
	if(!h->load()) {
		printf("Error loading\n");
	}
	else {
		QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
		a->start();
		app.exec();
		printf("done playing\n");
	}
	delete a;

	delete h;
	delete top;

	return 0;
}
#endif
