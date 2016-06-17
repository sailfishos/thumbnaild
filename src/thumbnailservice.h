/******************************************************************************
**
** This file is part of thumbnaild.
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@nokia.com>
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

#ifndef THUMBNAILSERVICE_H
#define THUMBNAILSERVICE_H

#include <QBasicTimer>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QStringList>
#include <QWaitCondition>


typedef QMap<QString, QString> ThumbnailPathList;

class ThumbnailerAdaptor;

struct ThumbnailRequest;
struct ThumbnailResponse;

class ThumbnailService : public QObject
{
    Q_OBJECT
public:
    ThumbnailService();

signals:
    void requestCompleted(unsigned id);
    void serviceExpired();

public slots:
    unsigned Fetch(const QStringList &uris, unsigned size, bool unbounded, bool crop);

private slots:
    void requestFinished(unsigned id);

private:
    friend class WorkerThread;

    void processRequests();
    QString processUri(const QString &uri);

    void timerEvent(QTimerEvent *event) override;

    ThumbnailerAdaptor *adaptor;
    QMutex requestMutex;
    QWaitCondition requestAvailable;
    unsigned requestId;
    bool serviceFinished;
    QList<ThumbnailRequest *> requests;
    QMap<unsigned, ThumbnailResponse *> responses;
    QBasicTimer timer;
};

Q_DECLARE_METATYPE(ThumbnailPathList)

#endif
