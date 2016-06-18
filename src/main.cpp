/******************************************************************************
**
** This file is part of thumbnaild.
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#include "thumbnailservice.h"

#include <QCoreApplication>
#include <QtDBus>
#include <QtDebug>

Q_DECL_EXPORT int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qWarning() << "Starting:" << argv[0];

    qDBusRegisterMetaType<ThumbnailPathList>();

    ThumbnailService service;
    QObject::connect(&service, &ThumbnailService::serviceExpired, qApp, &QCoreApplication::quit, Qt::QueuedConnection);

    int result = app.exec();
    qWarning() << "Finished:" << result;
    return result;
}
