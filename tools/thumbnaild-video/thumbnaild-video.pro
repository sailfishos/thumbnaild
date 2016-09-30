TEMPLATE = app
TARGET = thumbnaild-video

CONFIG += c++11 hide_symbols

CONFIG += link_pkgconfig
PKGCONFIG += \
        libavcodec \
        libavformat \
        libavutil \
        libswscale

INCLUDEPATH *= ..
HEADERS = thumbnailutility.h
SOURCES = main.cpp libavthumbnailer.cpp

target.path = /usr/bin

INSTALLS += target

