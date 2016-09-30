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

#include "thumbnailservice.h"
#include "thumbnaileradaptor.h"

#include <nemothumbnailcache.h>

namespace {

const QString serviceName(QStringLiteral("org.nemomobile.Thumbnailer"));

const unsigned MinSupportedSize = NemoThumbnailCache::Small;
const unsigned MaxSupportedSize = NemoThumbnailCache::Large;

const int LingerTimeMs = 2 * 60 * 1000;

}

struct ThumbnailRequest
{
    unsigned id;
    unsigned size;
    bool unbounded;
    bool crop;
    QStringList uris;
};

struct ThumbnailResponse
{
    unsigned count;
    ThumbnailPathList paths;
    QStringList failedUris;
};

class WorkerThread : public QThread
{
    Q_OBJECT

    ThumbnailService *service;

public:
    WorkerThread(ThumbnailService *service) : QThread(service) , service(service) {}

    void run() { service->processRequests(); }
};

ThumbnailService::ThumbnailService()
    : QObject(0)
    , adaptor(new ThumbnailerAdaptor(this))
    , requestId(0)
    , serviceFinished(false)
{
    if (!QDBusConnection::sessionBus().registerService(serviceName)) {
        qFatal("Unable to register service: %s", qPrintable(serviceName));
    }

    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/"), this)) {
        qFatal("Unable to register server object");
    }

    connect(this, &ThumbnailService::requestCompleted, this, &ThumbnailService::requestFinished, Qt::QueuedConnection);

    // Create worker threads to do the actual thumbnail processing
    for (int i = 0, n = QThread::idealThreadCount() * 2; i < n; ++i) {
        WorkerThread *thread = new WorkerThread(this);
        thread->start();
    }
}

unsigned ThumbnailService::Fetch(const QStringList &uris, unsigned size, bool unbounded, bool crop)
{
    bool validRequest = true;
    if (uris.isEmpty()) {
        qWarning() << "Invalid Fetch with empty URIS";
        validRequest = false;
    } else if (size > MaxSupportedSize || (size < MinSupportedSize && !unbounded)) {
        qWarning() << "Invalid Fetch with unsupported size:" << size << "unbounded:" << unbounded;
        validRequest = false;
    }

    ThumbnailResponse *response = new ThumbnailResponse;
    ThumbnailRequest *request = 0;

    if (validRequest) {
        response->count = uris.size();
        request = new ThumbnailRequest;
    } else {
        response->failedUris = uris;
    }

    unsigned id = 0;

    {
        QMutexLocker lock(&requestMutex);

        id = ++requestId;
        responses.insert(id, response);

        if (validRequest) {
            // Enqueue the request for processing
            request->id = id;
            request->size = size;
            request->unbounded = unbounded;
            request->crop = crop;
            request->uris = uris;
            requests.prepend(request);

            // Wake a worker to process the request
            requestAvailable.wakeOne();
        }
    }

    if (!validRequest) {
        // Report failure asynchronously
        emit requestCompleted(id);
    }

    timer.stop();

    return id;
}

void ThumbnailService::requestFinished(unsigned id)
{
    ThumbnailResponse *response = 0;
    bool pendingResponses = true;

    {
        QMutexLocker lock(&requestMutex);

        QMap<unsigned, ThumbnailResponse *>::iterator it = responses.find(id);
        if (it != responses.end()) {
            response = *it;
            responses.erase(it);
            pendingResponses = !responses.isEmpty();
        }
    }

    if (response) {
        if (!response->failedUris.isEmpty()) {
            emit adaptor->Failed(id, response->failedUris);
        }
        if (!response->paths.isEmpty()) {
            emit adaptor->Ready(id, response->paths);
        }
        emit adaptor->Finished(id);

        delete response;
    } else {
        qWarning() << Q_FUNC_INFO << "Invalid id:" << id;
    }

    if (!pendingResponses) {
        // Schedule termination if no new requests are received
        timer.start(LingerTimeMs, this);
    }
}

void ThumbnailService::processRequests()
{
    unsigned id = 0;
    QString uri;
    QString path;

    while (true) {
        requestMutex.lock();

        if (!uri.isEmpty()) {
            // Record our previous result
            QMap<unsigned, ThumbnailResponse *>::iterator it = responses.find(id);
            if (it == responses.end()) {
                qWarning() << Q_FUNC_INFO << "Invalid id:" << id << "for URI:" << uri;
            } else {
                ThumbnailResponse *response(*it);
                if (path.isEmpty()) {
                    response->failedUris.append(uri);
                } else {
                    response->paths.insert(uri, path);
                }
                if (--response->count == 0) {
                    emit requestCompleted(id);
                }
            }
        }

        // Take the next request
        while (!serviceFinished && requests.isEmpty()) {
            requestAvailable.wait(&requestMutex);
        }
        if (serviceFinished) {
            requestMutex.unlock();
            return;
        }

        bool outstandingRequests = true;

        // Process the next requested URI
        ThumbnailRequest *request = requests.last();
        const unsigned imageSize(request->size);
        const bool unbounded(request->unbounded);
        const bool crop(request->crop);
        id = request->id;
        uri = request->uris.takeLast();
        if (request->uris.isEmpty()) {
            // This request has been fully processed
            requests.removeLast();
            delete request;

            if (requests.isEmpty()) {
                outstandingRequests = false;
            }
        }

        // Start another handler if there are waiting requests
        if (outstandingRequests) {
            requestAvailable.wakeOne();
        }
        requestMutex.unlock();

        // Find or generate a thumbnail for this URI
        path = processUri(uri, imageSize, unbounded, crop);
    }
}

QString ThumbnailService::processUri(const QString &uri, unsigned size, bool unbounded, bool crop)
{
    NemoThumbnailCache::ThumbnailData data = NemoThumbnailCache::instance()->requestThumbnail(uri, QSize(size, size), crop, unbounded);
    return data.validPath() ? data.path() : QString();
}

void ThumbnailService::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId()) {
        timer.stop();

        qWarning() << "ThumbnailService expired after linger period";

        {
            QMutexLocker lock(&requestMutex);
            serviceFinished = true;
            requestAvailable.wakeAll();
        }

        QThread::msleep(1000);
        emit serviceExpired();
    }
}

#include "thumbnailservice.moc"
