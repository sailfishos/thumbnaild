TEMPLATE = app
TARGET = thumbnaild-pdf

CONFIG += c++11 hide_symbols

CONFIG += link_pkgconfig
PKGCONFIG += poppler-qt5

INCLUDEPATH *= ..
HEADERS = thumbnailutility.h
SOURCES = main.cpp

target.path = /usr/bin

INSTALLS += target

