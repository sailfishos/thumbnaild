TEMPLATE = app
TARGET = thumbnaild

QT += dbus
CONFIG += c++11

CONFIG += link_pkgconfig link_prl

packagesExist(qt5-boostable) {
    DEFINES += HAS_BOOSTER
    PKGCONFIG += qt5-boostable
} else {
    warning("qt5-boostable not available; startup times will be slower")
}

system(qdbusxml2cpp -c ThumbnailerAdaptor -a thumbnaileradaptor.h:thumbnaileradaptor.cpp -i thumbnailservice.h ../dbus/org.nemomobile.Thumbnailer.xml)

HEADERS =\
    thumbnaileradaptor.h \
    thumbnailservice.h

SOURCES =\
    thumbnaileradaptor.cpp \
    thumbnailservice.cpp \
    main.cpp

SERVICE_FILE = ../dbus/org.nemomobile.Thumbnailer.service
INTERFACE_FILE = ../dbus/org.nemomobile.Thumbnailer.xml
OTHER_FILES += $$SERVICE_FILE $$INTERFACE_FILE

service.files = $$SERVICE_FILE
service.path  = /usr/share/dbus-1/services/

interface.files = $$INTERFACE_FILE
interface.path  = /usr/share/dbus-1/interfaces/

target.path = /usr/bin

INSTALLS += service interface target
