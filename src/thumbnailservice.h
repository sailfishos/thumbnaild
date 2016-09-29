/*
 * Copyright (C) 2016 Jolla Ltd.
 * Contact: Matt Vogt <matthew.vogt@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

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
    QString processUri(const QString &uri, unsigned size, bool unbounded, bool crop);

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
