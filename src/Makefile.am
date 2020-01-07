# Build a static library (libggk.a)
noinst_LIBRARIES = libggk.a
libggk_a_CXXFLAGS = -fPIC -Wall -Wextra -std=c++11 $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS)
libggk_a_SOURCES = DBusInterface.cpp \
                   DBusInterface.h \
                   DBusMethod.cpp \
                   DBusMethod.h \
                   DBusObject.cpp \
                   DBusObject.h \
                   DBusObjectPath.h \
                   GattCharacteristic.cpp \
                   ../include/GattCharacteristic.h \
                   GattDescriptor.cpp \
                   ../include/GattDescriptor.h \
                   GattInterface.cpp \
                   ../include/GattInterface.h \
                   GattProperty.cpp \
                   ../include/GattProperty.h \
                   GattService.cpp \
                   ../include/GattService.h \
                   ../include/GattUuid.h \
                   Globals.h \
                   Gobbledegook.cpp \
                   ../include/Gobbledegook.h \
                   HciAdapter.cpp \
                   HciAdapter.h \
                   HciSocket.cpp \
                   HciSocket.h \
                   Init.cpp \
                   Init.h \
                   Logger.cpp \
                   Logger.h \
                   Mgmt.cpp \
                   Mgmt.h \
                   Server.cpp \
                   Server.h \
                   ServerUtils.cpp \
                   ServerUtils.h \
                   standalone.cpp \
                   TickEvent.h \
                   Utils.cpp \
                   Utils.h
# Build our standalone server (linking statically with libggk.a and GLib (though it could possibly be dynamic too)
standalone_CXXFLAGS = -fPIC -Wall -Wextra -std=c++11 $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS)
noinst_PROGRAMS = standalone
standalone_SOURCES = standalone.cpp
standalone_LDADD = libggk.a