TEMPLATE = lib
CONFIG  += qt thread release plugin
TARGET   = helixplugin

INCLUDEPATH += ..

HEADERS = \
	../mediaplugin.h \
	fivemmap.h \
	helixinterfaces.h \
	helixplugin.h

SOURCES = \
	fivemmap.cpp \
	iids.cpp \
	helixinterfaces.cpp \
	helixplugin.cpp

#SOURCES += main.cpp

CS_BASE = /home/justin/cvs/neo/cutestuff
HELIX_BASE = /home/justin/cvs/neo/helix/cvs/splay2

include(helix.pri)

