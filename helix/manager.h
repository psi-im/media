#ifndef MANAGER_H
#define MANAGER_H

#include <qobject.h>
#include "hxwintyp.h"

class IHXClientEngine;

class Manager : public QObject
{
	Q_OBJECT
public:
	Manager(IHXClientEngine *clientEngine);
	~Manager();

	void start();
	void stop();
	HXxWindow *createWindow();
	void freeWindow(HXxWindow *);

	static Manager *Manager::singleton();

	class Private;
private:
	Private *d;
};

#endif
