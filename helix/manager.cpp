#include "manager.h"

#include <qtimer.h>
#include <qptrlist.h>

#include "hxcom.h"
#include "hxcore.h"

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#endif

Manager *manager = 0;

//----------------------------------------------------------------------------
// Manager
//----------------------------------------------------------------------------
class Manager::Private : public QObject
{
	Q_OBJECT
public:
	QTimer helixTimer;
	IHXClientEngine *clientEngine;
	QPtrList<HXxWindow> windowList;

#ifdef Q_WS_X11
	QTimer xTimer;
	bool xshm_present;
	int xshm_event_base;
	Display *disp;
#endif

	Private()
	{
		connect(&helixTimer, SIGNAL(timeout()), SLOT(t_helix()));

#ifdef Q_WS_X11
		disp = 0;
		xshm_present = false;
		connect(&xTimer, SIGNAL(timeout()), SLOT(t_x()));
#endif
	}

private slots:
	void t_helix()
	{
		clientEngine->EventOccurred(NULL);
	}

	void t_x()
	{
#ifdef Q_WS_X11
		XEvent xevent;
		int x_event_available;

		memset(&xevent, 0, sizeof(xevent));

		do
		{
			XLockDisplay(disp);
			x_event_available = XPending(disp);

			if(x_event_available)
			{
				XNextEvent(disp, &xevent);
			}
			XUnlockDisplay(disp);

			if (x_event_available)
			{
				XEvent* pXEvent = &xevent;
				HXxEvent event;
				HXxEvent* pEvent = &event;
				if(pXEvent)
				{
					memset(&event, 0, sizeof(event));
					event.event = pXEvent->type;
					event.window = (void*)pXEvent->xany.window;
					event.param1 = pXEvent->xany.display;
					event.param2 = pXEvent;
				}
				else
				{
					pEvent = NULL;
				}
				clientEngine->EventOccurred(pEvent);

				/* RGG: A helpful hint: If you're getting messages here
				that aren't in the range of the core X events, an
				X extension may be sending you events. You can look
				for this extension using the command
				"xdpyinfo -ext all" and by looking at the reported
				"base event" number. */

				/* Check the extensions */
				if(xshm_present && (ShmCompletion + xshm_event_base) == xevent.type)
				{
					/* Ignore xshm completion */
				}
				else
				{
					//printf("Unhandled event type %d\n", xevent.type);
				}
			}
		} while (x_event_available);
#endif
	}
};

Manager::Manager(IHXClientEngine *clientEngine)
{
	manager = this;
	d = new Private;
	d->clientEngine = clientEngine;
}

Manager::~Manager()
{
	if(!d->windowList.isEmpty())
		printf("*** ~Manager : %d site windows were not freed\n", d->windowList.count());

	if(d->disp)
		XCloseDisplay(d->disp);

	manager = 0;
	delete d;
}

void Manager::start()
{
	if(!d->helixTimer.isActive())
		d->helixTimer.start(20);
}

void Manager::stop()
{
	d->helixTimer.stop();
}

HXxWindow *Manager::createWindow()
{
	HXxWindow *siteWin = new HXxWindow;
	memset(siteWin, 0, sizeof(HXxWindow));

#ifdef Q_WS_X11
	// if we don't have a separate display connection, make one
	if(!d->disp)
	{
		d->disp = XOpenDisplay(NULL);

		// initialize the X handling
		if(d->disp)
		{
			int ignore;
			d->xshm_present = XQueryExtension(d->disp, "MIT-SHM", &ignore, &d->xshm_event_base, &ignore);
			d->xTimer.start(10);
		}
	}

	// use separate display if possible, else share with Qt
	if(d->disp)
		siteWin->display = d->disp;
	else
		siteWin->display = qt_xdisplay();
#endif

	d->windowList.append(siteWin);
	return siteWin;
}

void Manager::freeWindow(HXxWindow *siteWin)
{
	d->windowList.removeRef(siteWin);
	delete siteWin;

	if(d->windowList.isEmpty() && d->disp)
	{
		d->xTimer.stop();
		d->xshm_present = false;
		XCloseDisplay(d->disp);
		d->disp = 0;
	}
}

Manager *Manager::singleton()
{
	return manager;
}

#include "manager.moc"
