CONFIG  += x11
DEFINES += QT_CLEAN_NAMESPACE

INCLUDEPATH += \
	$$HELIX_BASE/common/include \
	$$HELIX_BASE/common/runtime/pub \
	$$HELIX_BASE/client/include \
	$$HELIX_BASE/common/container/pub \
	$$HELIX_BASE/datatype/rm/include \
	$$HELIX_BASE/common/system/pub \
	$$HELIX_BASE/common/dbgtool/pub \
	$$HELIX_BASE/common/util/pub \
#	$$HELIX_BASE2/producersdk/include \
#	$$HELIX_BASE2/producersdk/common/include \
#	$$HELIX_BASE2/common/log/logobserverfile/pub

LIBS += \
	$$HELIX_BASE/common/runtime/rel/runtlib.a \
	$$HELIX_BASE/common/system/rel/syslib.a \
	$$HELIX_BASE/common/container/rel/contlib.a \
	$$HELIX_BASE/common/util/rel/utillib.a \

unix:{
	QMAKE_CXXFLAGS += -march=pentium
	QMAKE_CXXFLAGS += -include $$HELIX_BASE/clientapps/simpleplayer/rel/clientapps_simpleplayer_ribodefs.h
}

#QMAKE_CXXFLAGS += -include ../cvs/prod/producersdk/sdksamples/encoder/rel/producersdk_sdksamples_encoder_ribodefs.h

