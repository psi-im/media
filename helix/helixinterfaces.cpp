#include "helixinterfaces.h"

#include "hxbuffer.h"
#include "hxmangle.h"

#include "manager.h"

#include<X11/Xlib.h>
QWidget *siteWidget = 0;
IHXSite* siteHXSite = 0;
HXxWindow *siteWin = 0;
extern double aspect;

//----------------------------------------------------------------------------
// MyErrorSink
//----------------------------------------------------------------------------
MyErrorSink::MyErrorSink(IUnknown* u)
{
	refs = 0;
	IHXClientEngine *engine = 0;
	u->QueryInterface(IID_IHXClientEngine, (void **)&engine);
	if(engine) {
		IUnknown *pTmp = NULL;
		engine->GetPlayer(0, pTmp);
		player = (IHXPlayer *)pTmp;
	}

	HX_RELEASE(engine);
}

MyErrorSink::~MyErrorSink()
{
	HX_RELEASE(player);
}

STDMETHODIMP MyErrorSink::QueryInterface(REFIID riid, void **ppvObj)
{
	if(IsEqualIID(riid, IID_IUnknown)) {
		AddRef();
		*ppvObj = (IUnknown*)(IHXErrorSink*)this;
		return HXR_OK;
	}
	else if (IsEqualIID(riid, IID_IHXErrorSink)) {
		AddRef();
		*ppvObj = (IHXErrorSink*)this;
		return HXR_OK;
	}

	*ppvObj = NULL;
	return HXR_NOINTERFACE;
}

STDMETHODIMP_(ULONG32) MyErrorSink::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG32) MyErrorSink::Release()
{
	if(InterlockedDecrement(&refs) > 0)
		return refs;

	delete this;
	return 0;
}

STDMETHODIMP MyErrorSink::ErrorOccurred(const UINT8 unSeverity, const ULONG32 ulHXCode, const ULONG32 ulUserCode, const char *pUserString, const char *pMoreInfoURL)
{
	printf("Error: s=%d, hxcode=%lu, usercode=%lu, userstr=%s, url=%s\n", unSeverity, ulHXCode, ulUserCode, pUserString, pMoreInfoURL);
	return HXR_OK;
}

//----------------------------------------------------------------------------
// MyClientAdviceSink
//----------------------------------------------------------------------------
MyClientAdviceSink::MyClientAdviceSink(IUnknown* u)
{
	refs = 0;

	player = 0;
	registry = 0;
	scheduler = 0;
	currentBandwidth = 0;
	averageBandwidth = 0;
	onStop = false;

	if(u) {
		player = u;
		player->AddRef();

		if(player->QueryInterface(IID_IHXRegistry, (void **)&registry) != HXR_OK)
			registry = 0;
		if(player->QueryInterface(IID_IHXScheduler, (void **)&scheduler) != HXR_OK)
			scheduler = 0;

		IHXPlayer *pPlayer;
		if(player->QueryInterface(IID_IHXPlayer, (void **)&pPlayer) == HXR_OK) {
			pPlayer->AddAdviseSink(this);
			pPlayer->Release();
		}
	}
}

MyClientAdviceSink::~MyClientAdviceSink()
{
	if(scheduler)
		scheduler->Release();
	if(registry)
		registry->Release();
	if(player)
		player->Release();
}

STDMETHODIMP MyClientAdviceSink::QueryInterface(REFIID riid, void **ppvObj)
{
	if(IsEqualIID(riid, IID_IUnknown)) {
		AddRef();
		*ppvObj = (IUnknown*)(IHXClientAdviseSink*)this;
		return HXR_OK;
	}
	else if(IsEqualIID(riid, IID_IHXClientAdviseSink)) {
		AddRef();
		*ppvObj = (IHXClientAdviseSink*)this;
		return HXR_OK;
	}

	*ppvObj = NULL;
	return HXR_NOINTERFACE;
}

STDMETHODIMP_(ULONG32) MyClientAdviceSink::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG32) MyClientAdviceSink::Release()
{
	if(InterlockedDecrement(&refs) > 0)
		return refs;

	delete this;
	return 0;
}

STDMETHODIMP MyClientAdviceSink::OnPosLength(UINT32 ulPosition, UINT32 ulLength)
{
	printf("Advice: Pos/Length: [%ld, %ld]\n", ulPosition, ulLength);
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnPresentationOpened()
{
	printf("Advice: Presentation Opened\n");
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnPresentationClosed()
{
	printf("Advice: Presentation Closed\n");
	return HXR_OK;
}

#if 0
void ExampleClientAdviceSink::GetStatistics (char* pszRegistryKey)
{
    char    szRegistryValue[MAX_DISPLAY_NAME] = {0}; /* Flawfinder: ignore */
    INT32   lValue = 0;
    INT32   i = 0;
    INT32   lStatistics = 8;
    UINT32 *plValue;

#ifdef __TCS__
    return;	  // DISABLED FOR NOW
#endif

    // collect statistic
    for (i = 0; i < lStatistics; i++)
    {
	plValue = NULL;
	switch (i)
	{
	case 0:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.Normal", pszRegistryKey);
	    break;
	case 1:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.Recovered", pszRegistryKey);
	    break;
	case 2:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.Received", pszRegistryKey);
	    break;
	case 3:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.Lost", pszRegistryKey);
	    break;
	case 4:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.Late", pszRegistryKey);
	    break;
	case 5:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.ClipBandwidth", pszRegistryKey);
	    break;
	case 6:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.AverageBandwidth", pszRegistryKey);
	    plValue = &m_lAverageBandwidth;
	    break;
	case 7:
	    SafeSprintf(szRegistryValue, MAX_DISPLAY_NAME, "%s.CurrentBandwidth", pszRegistryKey);
	    plValue = &m_lCurrentBandwidth;
	    break;
	default:
	    break;
	}

	m_pRegistry->GetIntByName(szRegistryValue, lValue);
	if (plValue)
	{
	    if (m_bOnStop || lValue == 0)
	    {
		lValue = *plValue;
	    }
	    else
	    {
		*plValue = lValue;
	    }
	}
	if (GetGlobal()->bEnableAdviceSink || (GetGlobal()->bEnableVerboseMode && m_bOnStop))
	{
	    STDOUT("%s = %ld\n", szRegistryValue, lValue);
	}
    }
}

void ExampleClientAdviceSink::GetAllStatistics(void)
{
    UINT32  unPlayerIndex = 0;
    UINT32  unSourceIndex = 0;
    UINT32  unStreamIndex = 0;

    char*   pszRegistryPrefix = "Statistics";
    char    szRegistryName[MAX_DISPLAY_NAME] = {0}; /* Flawfinder: ignore */

#ifdef __TCS__
    return;	  // DISABLED FOR NOW
#endif

    // display the content of whole statistic registry
    if (m_pRegistry)
    {
	// ok, let's start from the top (player)
	SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Player%ld", pszRegistryPrefix, m_lClientIndex);
	if (PT_COMPOSITE == m_pRegistry->GetTypeByName(szRegistryName))
	{
	    // display player statistic
	    GetStatistics(szRegistryName);

	    SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Source%ld", szRegistryName, unSourceIndex);
	    while (PT_COMPOSITE == m_pRegistry->GetTypeByName(szRegistryName))
	    {
		// display source statistic
		GetStatistics(szRegistryName);

		SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Stream%ld", szRegistryName, unStreamIndex);
		while (PT_COMPOSITE == m_pRegistry->GetTypeByName(szRegistryName))
		{
		    // display stream statistic
		    GetStatistics(szRegistryName);

		    unStreamIndex++;

		    SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Player%ld.Source%ld.Stream%ld",
			pszRegistryPrefix, unPlayerIndex, unSourceIndex, unStreamIndex);
		}

		unSourceIndex++;

		SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Player%ld.Source%ld",
		    pszRegistryPrefix, unPlayerIndex, unSourceIndex);
	    }

	    unPlayerIndex++;

	    SafeSprintf(szRegistryName, MAX_DISPLAY_NAME, "%s.Player%ld", pszRegistryPrefix, unPlayerIndex);
	}
    }
}
#endif

STDMETHODIMP MyClientAdviceSink::OnStatisticsChanged(void)
{
#if 0
	char        szBuff[1024]; /* Flawfinder: ignore */
	HX_RESULT   res     = HXR_OK;
	UINT16      uPlayer = 0;

	if(GetGlobal()->bEnableAdviceSink)
	{
		STDOUT("OnStatisticsChanged():\n");

		SafeSprintf(szBuff, 1024, "Statistics.Player%u", uPlayer );
		while( HXR_OK == res )
		{
		res = DumpRegTree( szBuff );
		uPlayer++;
		SafeSprintf(szBuff, 1024, "Statistics.Player%u", uPlayer );
		}
	}
#endif
	return HXR_OK;
}

#if 0
HX_RESULT ExampleClientAdviceSink::DumpRegTree(const char* pszTreeName )
{
    const char* pszName = NULL;
    ULONG32     ulRegID   = 0;
    HX_RESULT   res     = HXR_OK;
    INT32       nVal    = 0;
    IHXBuffer* pBuff   = NULL;
    IHXValues* pValues = NULL;

    //See if the name exists in the reg tree.
    res = m_pRegistry->GetPropListByName( pszTreeName, pValues);
    if( HXR_OK!=res || !pValues )
        return HXR_FAIL;

    //make sure this is a PT_COMPOSITE type reg entry.
    if( PT_COMPOSITE != m_pRegistry->GetTypeByName(pszTreeName))
        return HXR_FAIL;

    //Print out the value of each member of this tree.
    res = pValues->GetFirstPropertyULONG32( pszName, ulRegID );
    while( HXR_OK == res )
    {
        //We have at least one entry. See what type it is.
        HXPropType pt = m_pRegistry->GetTypeById(ulRegID);
        switch(pt)
        {
           case PT_COMPOSITE:
               DumpRegTree(pszName);
               break;
           case PT_INTEGER :
               nVal = 0;
               m_pRegistry->GetIntById( ulRegID, nVal );
               STDOUT("%s : %d\n", pszName, nVal );
               break;
           case PT_INTREF :
               nVal = 0;
               m_pRegistry->GetIntById( ulRegID, nVal );
               STDOUT("%s : %d\n", pszName, nVal );
               break;
           case PT_STRING :
               pBuff = NULL;
               m_pRegistry->GetStrById( ulRegID, pBuff );
               STDOUT("%s : \"", pszName );
               if( pBuff )
                   STDOUT("%s", (const char *)(pBuff->GetBuffer()) );
               STDOUT("\"\n" );
               HX_RELEASE(pBuff);
               break;
           case PT_BUFFER :
               STDOUT("%s : BUFFER TYPE NOT SHOWN\n",
                        pszName, nVal );
               break;
           case PT_UNKNOWN:
               STDOUT("%s Unkown registry type entry\n", pszName );
               break;
           default:
               STDOUT("%s Unkown registry type entry\n", pszName );
               break;
        }
        res = pValues->GetNextPropertyULONG32( pszName, ulRegID);
    }

    HX_RELEASE( pValues );

    return HXR_OK;
}
#endif

STDMETHODIMP MyClientAdviceSink::OnPreSeek(ULONG32 ulOldTime, ULONG32 ulNewTime)
{
	printf("Advice: PreSeek old/new: [%ld, %ld]\n", ulOldTime, ulNewTime);
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnPostSeek(ULONG32 ulOldTime, ULONG32 ulNewTime)
{
	printf("Advice: PostSeek old/new: [%ld, %ld]\n", ulOldTime, ulNewTime);
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnStop(void)
{
	printf("Advice: Stopped\n");
	onStop = true;

	//GetAllStatistics();

	// Find out the current time and subtract the beginning time to
	// figure out how many seconds we played
	//HXTimeval now = scheduler->GetCurrentSchedulerTime();
	//stopTime = now.tv_sec;
	//GetGlobal()->g_ulNumSecondsPlayed = m_ulStopTime - m_ulStartTime;
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnPause(ULONG32 ulTime)
{
	printf("Advice: Paused: [%ld]\n", ulTime);
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnBegin(ULONG32 ulTime)
{
	printf("Advice: Starting ... [%ld]\n", ulTime);

	// Record the current time, so we can figure out many seconds we played
	HXTimeval now = scheduler->GetCurrentSchedulerTime();
	startTime = now.tv_sec;
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnBuffering(ULONG32 ulFlags, UINT16 unPercentComplete)
{
	printf("Advice: Buffering: [%ld, %ld]\n", ulFlags, unPercentComplete);
	return HXR_OK;
}

STDMETHODIMP MyClientAdviceSink::OnContacting(const char* pHostName)
{
	printf("Advice: Contacting: [%s]\n", pHostName);
	return HXR_OK;
}

//----------------------------------------------------------------------------
// MyAuthManager
//----------------------------------------------------------------------------
MyAuthManager::MyAuthManager()
{
	refs = 0;
	sentPassword = false;
}

MyAuthManager::~MyAuthManager()
{
}

STDMETHODIMP MyAuthManager::QueryInterface(REFIID riid, void **ppvObj)
{
	if(IsEqualIID(riid, IID_IUnknown)) {
		AddRef();
		*ppvObj = (IUnknown*)(IHXAuthenticationManager*)this;
		return HXR_OK;
	}
	else if(IsEqualIID(riid, IID_IHXAuthenticationManager)) {
		AddRef();
		*ppvObj = (IHXAuthenticationManager*)this;
		return HXR_OK;
	}

	*ppvObj = NULL;
	return HXR_NOINTERFACE;
}

STDMETHODIMP_(ULONG32) MyAuthManager::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG32) MyAuthManager::Release()
{
	if(InterlockedDecrement(&refs) > 0)
		return refs;

	delete this;
	return 0;
}

STDMETHODIMP MyAuthManager::HandleAuthenticationRequest(IHXAuthenticationManagerResponse *pResponse)
{
    char      username[1024] = ""; /* Flawfinder: ignore */
    char      password[1024] = ""; /* Flawfinder: ignore */
    HX_RESULT res = HXR_FAIL;
#if 0
    if( !m_bSentPassword )
    {
        res = HXR_OK;
        if (GetGlobal()->bEnableVerboseMode)
            STDOUT("\nSending Username and Password...\n");

        SafeStrCpy(username,  GetGlobal()->g_pszUsername, 1024);
        SafeStrCpy(password,  GetGlobal()->g_pszPassword, 1024);

        //strip trailing whitespace
        char* c;
        for(c = username + strlen(username) - 1;
            c > username && isspace(*c);
            c--)
            ;
        *(c+1) = 0;

        for(c = password + strlen(password) - 1;
            c > password && isspace(*c);
            c--)
            ;
        *(c+1) = 0;

        m_bSentPassword = TRUE;
    }

    if (GetGlobal()->bEnableVerboseMode && FAILED(res) )
        STDOUT("\nInvalid Username and/or Password.\n");

    pResponse->AuthenticationRequestDone(res, username, password);
#endif
    return res;
}

//----------------------------------------------------------------------------
// MySiteSupplier
//----------------------------------------------------------------------------
MySiteSupplier::MySiteSupplier(IUnknown* _player)
{
	refs = 0;
	player = _player;
	player->QueryInterface(IID_IHXSiteManager, (void **)&siteManager);
	player->QueryInterface(IID_IHXCommonClassFactory, (void **)&classFactory);
	player->AddRef();
}

MySiteSupplier::~MySiteSupplier()
{
	HX_RELEASE(siteManager);
	HX_RELEASE(classFactory);
	HX_RELEASE(player);
}

STDMETHODIMP MySiteSupplier::QueryInterface(REFIID riid, void **ppvObj)
{
	if(IsEqualIID(riid, IID_IUnknown)) {
		AddRef();
		*ppvObj = (IUnknown*)(IHXSiteSupplier*)this;
		return HXR_OK;
	}
	else if(IsEqualIID(riid, IID_IHXSiteSupplier)) {
		AddRef();
		*ppvObj = (IHXSiteSupplier*)this;
		return HXR_OK;
	}

	*ppvObj = NULL;
	return HXR_NOINTERFACE;
}

STDMETHODIMP_(ULONG32) MySiteSupplier::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG32) MySiteSupplier::Release()
{
	if(InterlockedDecrement(&refs) > 0)
		return refs;

	delete this;
	return 0;
}

STDMETHODIMP MySiteSupplier::SitesNeeded(UINT32 uRequestID, IHXValues* props)
{
	// TODO: do we care about this?
	if(!props)
		return HXR_INVALID_PARAMETER;

	IHXSiteWindowed *siteWindowed = 0;
	IHXSite *site = 0;
	IHXValues *siteProps = 0;

	HRESULT hres;
	hres = classFactory->CreateInstance(CLSID_IHXSiteWindowed, (void**)&siteWindowed);
	if(hres != HXR_OK)
		return hres;

	hres = siteWindowed->QueryInterface(IID_IHXSite, (void**)&site);
	if(hres != HXR_OK) {
		HX_RELEASE(siteWindowed);
		return hres;
	}

	hres = siteWindowed->QueryInterface(IID_IHXValues, (void**)&siteProps);
	if(hres != HXR_OK) {
		HX_RELEASE(site);
		HX_RELEASE(siteWindowed);
		return hres;
	}

	IHXBuffer*		pValue		= NULL;
	UINT32		style		= 0;


    /*
     * We need to figure out what type of site we are supposed to
     * to create. We need to "switch" between site user and site
     * properties. So look for the well known site user properties
     * that are mapped onto sites...
     */
    hres = props->GetPropertyCString("playto",pValue);
    if (HXR_OK == hres)
    {
	siteProps->SetPropertyCString("channel",pValue);
	HX_RELEASE(pValue);
    }
    else
    {
	hres = props->GetPropertyCString("name",pValue);
	if (HXR_OK == hres)
	{
	    siteProps->SetPropertyCString("LayoutGroup",pValue);
    	    HX_RELEASE(pValue);
	}
    }

#ifdef _WINDOWS
    style = WS_OVERLAPPED | WS_VISIBLE | WS_CLIPCHILDREN;
#endif

	siteWin = Manager::singleton()->createWindow();
	/*siteWin->x = 20;
	siteWin->y = 40;
	siteWin->width = 320;
	siteWin->height = 240;
	siteWin->clipRect.left = 0;
	siteWin->clipRect.right = 320;
	siteWin->clipRect.top = 0;
	siteWin->clipRect.bottom = 240;*/

	//printf("X Window = %p.\n",  siteWin->window);
	//printf("X Display = %p.\n", siteWin->display);

	siteWin->window = (void *)siteWidget->winId();
	printf("Created Site Window\n");
	printf("  Window ID = %p.\n",  siteWin->window);
#ifdef Q_WS_X11
	printf("  X Display = %p.\n", siteWin->display);
#endif*/

    //hres = pSiteWindowed->Create(siteWin->window /*NULL*/, style);
    hres = siteWindowed->AttachWindow(siteWin);

    if (HXR_OK != hres)
    {
	goto exit;
    }

    /*
     * We need to wait until we have set all the properties before
     * we add the site.
     */
    hres = siteManager->AddSite(site);
    if (HXR_OK != hres)
    {
	goto exit;
    }

#ifdef _WINDOWS
    {
       HXxWindow* pWindow = pSiteWindowed->GetWindow();
       if (pWindow && pWindow->window) ::SetForegroundWindow( (HWND)(pWindow->window) );
    }
#endif
    m_CreatedSites.SetAt((void*)uRequestID, site);
    site->AddRef();
    //HXxPoint pos;
    //pos.x = 32;
    //pos.y = 32;
    siteHXSite = site;
    /*HXxSize size;
    size.cx = 320;
    size.cy = 240;
    //pSite->SetPosition(pos);
    pSite->SetSize(size);
    //printf("current size: %dx%d\n", size.cx, size.cy);*/

exit:

    HX_RELEASE(siteProps);
    HX_RELEASE(siteWindowed);
    HX_RELEASE(site);

    return hres;
}

STDMETHODIMP MySiteSupplier::SitesNotNeeded(UINT32 uRequestID)
{
   printf("sites not needed\n");

    IHXSite*		pSite = NULL;
    IHXSiteWindowed*	pSiteWindowed = NULL;
    void*		pVoid = NULL;

    if (!m_CreatedSites.Lookup((void*)uRequestID,pVoid))
    {
	return HXR_INVALID_PARAMETER;
    }
    pSite = (IHXSite*)pVoid;

    siteManager->RemoveSite(pSite);

    // Need to actually do the work on destroying the window
    // and all that jazz.
    pSite->QueryInterface(IID_IHXSiteWindowed,(void**)&pSiteWindowed);

    pSiteWindowed->Destroy();

    // ref count = 2
    pSiteWindowed->Release();

    // ref count = 1; deleted from this object's view!
    pSite->Release();

    m_CreatedSites.RemoveKey((void*)uRequestID);

    Manager::singleton()->freeWindow(siteWin);
    siteWin = 0;

    return HXR_OK;
}

STDMETHODIMP MySiteSupplier::BeginChangeLayout()
{
	return HXR_OK;
}

STDMETHODIMP MySiteSupplier::DoneChangeLayout()
{
	client->siteChanged();
	return HXR_OK;
}

//----------------------------------------------------------------------------
// MyClient
//----------------------------------------------------------------------------
MyClient::MyClient()
{
	refs = 0;
}

MyClient::~MyClient()
{
	Close();
};

void MyClient::Init(IUnknown *u, IHXPreferences *prefs, const QString &_guid)
{
	errorSink = new MyErrorSink(u);
	errorSink->AddRef();

	clientSink = new MyClientAdviceSink(u);
	clientSink->AddRef();

	authManager = new MyAuthManager();
	authManager->AddRef();

	siteSupplier = new MySiteSupplier(u);
	siteSupplier->AddRef();
	siteSupplier->client = this;

	if(prefs) {
		defaultPrefs = prefs;
		defaultPrefs->AddRef();
	}

	if(!_guid.isEmpty())
		guid = QString::fromLatin1(Cipher(_guid.latin1()));
	else
		guid = QString();
}

void MyClient::Close()
{
	HX_RELEASE(errorSink);
	HX_RELEASE(clientSink);
	HX_RELEASE(authManager);
	HX_RELEASE(siteSupplier);
	HX_RELEASE(defaultPrefs);
}

STDMETHODIMP MyClient::QueryInterface(REFIID riid, void **ppvObj)
{
	if(IsEqualIID(riid, IID_IUnknown)) {
		AddRef();
		*ppvObj = this;
		return HXR_OK;
	}
	else if(IsEqualIID(riid, IID_IHXPreferences)) {
		AddRef();
		*ppvObj = (IHXPreferences*)this;
		return HXR_OK;
	}
	else if(errorSink && errorSink->QueryInterface(riid, ppvObj) == HXR_OK)
		return HXR_OK;
	else if(clientSink && clientSink->QueryInterface(riid, ppvObj) == HXR_OK)
		return HXR_OK;
	else if(authManager && authManager->QueryInterface(riid, ppvObj) == HXR_OK)
		return HXR_OK;
	else if(siteSupplier && siteSupplier->QueryInterface(riid, ppvObj) == HXR_OK)
		return HXR_OK;

	*ppvObj = NULL;
	return HXR_NOINTERFACE;
}

STDMETHODIMP_(ULONG32) MyClient::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG32) MyClient::Release()
{
	if(InterlockedDecrement(&refs) > 0)
		return refs;

	delete this;
	return 0;
}

STDMETHODIMP MyClient::ReadPref(const char *pref_key, IHXBuffer *&buffer)
{
	HX_RESULT hResult = HXR_OK;
	char *pszCipher = NULL;

	if((stricmp(pref_key, CLIENT_GUID_REGNAME) == 0) && !guid.isEmpty()) {
		// Create a Buffer
		buffer = new CHXBuffer();
		buffer->AddRef();

		// Copy the encoded GUID into the buffer
		buffer->Set((UCHAR*)guid.latin1(), strlen(guid.latin1()) + 1);
	}
	else if (defaultPrefs)
		hResult = defaultPrefs->ReadPref(pref_key, buffer);
	else
		hResult = HXR_NOTIMPL;

	return hResult;
}

STDMETHODIMP MyClient::WritePref(const char *pref_key, IHXBuffer *buffer)
{
	if(defaultPrefs)
		return defaultPrefs->WritePref(pref_key, buffer);
	else
		return HXR_OK;
}

void MyClient::setWidget(QWidget *siteTarget)
{
	siteWidget = siteTarget;
}

IHXSite *MyClient::site() const
{
	return siteHXSite;
}

void MyClient::siteChanged()
{
	printf("client: site changed\n");
	siteReady();
}
