#ifndef HELIXINTERFACES_H
#define HELIXINTERFACES_H

#include <qwidget.h>
#include <qstring.h>

#include "hxcom.h"
#include "hxtypes.h"
#include "hxwintyp.h"
#include "hxwin.h"
#include "ihxpckts.h"
#include "hxcomm.h"
#include "hxprefs.h"
#include "hxerror.h"
#include "hxengin.h"
#include "hxcore.h"
#include "hxclsnk.h"
#include "hxmon.h"
#include "hxauth.h"
#include "fivemmap.h"

class MyClient;

class MyErrorSink : public IHXErrorSink
{
public:
	MyErrorSink(IUnknown *u);
	virtual ~MyErrorSink();

	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppvObj);
	STDMETHOD_(ULONG32,AddRef) (THIS);
	STDMETHOD_(ULONG32,Release) (THIS);

	// IHXErrorSink
	STDMETHOD(ErrorOccurred) (THIS_ const UINT8 unSeverity, const ULONG32 ulHXCode, const ULONG32 ulUserCode, const char* pUserString, const char* pMoreInfoURL);

private:
	LONG32 refs;
	IHXPlayer *player;
};

class MyClientAdviceSink : public IHXClientAdviseSink
{
public:
	MyClientAdviceSink(IUnknown *u);
	virtual ~MyClientAdviceSink();

	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppvObj);
	STDMETHOD_(ULONG32,AddRef) (THIS);
	STDMETHOD_(ULONG32,Release) (THIS);

	// IHXClientAdviseSink
	STDMETHOD(OnPosLength) (THIS_ UINT32 ulPosition, UINT32 ulLength);
	STDMETHOD(OnPresentationOpened) (THIS);
	STDMETHOD(OnPresentationClosed) (THIS);
	STDMETHOD(OnStatisticsChanged) (THIS);
	STDMETHOD(OnPreSeek) (THIS_ ULONG32 ulOldTime, ULONG32  ulNewTime);
	STDMETHOD(OnPostSeek) (THIS_ ULONG32 ulOldTime, ULONG32 ulNewTime);
	STDMETHOD(OnStop) (THIS);
	STDMETHOD(OnPause) (THIS_ ULONG32 ulTime);
	STDMETHOD(OnBegin) (THIS_ ULONG32 ulTime);
	STDMETHOD(OnBuffering) (THIS_ ULONG32 ulFlags, UINT16 unPercentComplete);
	STDMETHOD(OnContacting) (THIS_ const char *pHostName);

private:
	LONG32 refs;
	IUnknown *player;
	IHXRegistry *registry;
	IHXScheduler *scheduler;
	UINT32 startTime;
	UINT32 stopTime;
	UINT32 currentBandwidth;
	UINT32 averageBandwidth;
	bool onStop;

	//HX_RESULT DumpRegTree(const char *pszTreeName);
	//void GetStatistics(char *pszRegistryKey);
	//void GetAllStatistics (void);
};

class MyAuthManager : public IHXAuthenticationManager
{
public:
	MyAuthManager();
	virtual ~MyAuthManager();

	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppvObj);
	STDMETHOD_(ULONG32,AddRef) (THIS);
	STDMETHOD_(ULONG32,Release) (THIS);

	// IHXAuthenticationManager
	STDMETHOD(HandleAuthenticationRequest) (IHXAuthenticationManagerResponse* pResponse);

private:
	LONG32 refs;
	bool sentPassword;
};

class MySiteSupplier : public IHXSiteSupplier
{
public:
	MySiteSupplier(IUnknown *player);
	virtual ~MySiteSupplier();

	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppvObj);
	STDMETHOD_(ULONG32,AddRef) (THIS);
	STDMETHOD_(ULONG32,Release) (THIS);

	// IHXSiteSupplier
	STDMETHOD(SitesNeeded) (THIS_ UINT32 uRequestID, IHXValues *props);
	STDMETHOD(SitesNotNeeded) (THIS_ UINT32 uRequestID);
	STDMETHOD(BeginChangeLayout) (THIS);
	STDMETHOD(DoneChangeLayout) (THIS);

	MyClient *client;

private:
	LONG32 refs;
	IUnknown *player;
	IHXSiteManager *siteManager;
	IHXCommonClassFactory *classFactory;
	FiveMinuteMap           m_CreatedSites;
};

class MyClient : public QObject, public IHXPreferences
{
	Q_OBJECT
public:
	MyClient();
	virtual ~MyClient();

	void Init(IUnknown *u, IHXPreferences *prefs, const QString &guid);
	void Close();

	// IUnknown
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppvObj);
	STDMETHOD_(ULONG32,AddRef) (THIS);
	STDMETHOD_(ULONG32,Release) (THIS);

	// IHXPreferences
	STDMETHOD(ReadPref) (THIS_ const char *pref_key, IHXBuffer *&buffer);
	STDMETHOD(WritePref) (THIS_ const char *pref_key, IHXBuffer *buffer);

	// To get a site, set the target QWidget here.  siteReady() will be emitted once done, and
	// then you can call site() and display() to get the details.
	void setWidget(QWidget *siteTarget);
	IHXSite *site() const;
#ifdef Q_WS_X11
	Display *display() const;
#endif

signals:
	void siteReady();

private:
	LONG32 refs;
	MyErrorSink *errorSink;
	MyClientAdviceSink *clientSink;
	MyAuthManager *authManager;
	MySiteSupplier *siteSupplier;
	IHXPreferences *defaultPrefs;
	QString guid;

	friend class MySiteSupplier;
	void siteChanged();
};

#endif
