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
#include "thumbnaileradaptor.h"

#include <nemothumbnailcache.h>

namespace {

const QString serviceName(QStringLiteral("org.nemomobile.Thumbnailer"));

const unsigned MinSupportedSize = 128;
const unsigned MaxSupportedSize = 512;

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
    } else if (crop) {
        // We don't handle the crop option at this time
        qWarning() << "Invalid Fetch with unsupported crop option";
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
        }
    }

    if (validRequest) {
        // Wake a worker to process the request
        requestAvailable.wakeOne();
    } else {
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
        if (requests.isEmpty()) {
            requestAvailable.wait(&requestMutex);
        }
        if (serviceFinished) {
            requestMutex.unlock();
            return;
        }

        bool outstandingRequests = true;

        // Process the next requested URI
        ThumbnailRequest *request = requests.last();
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
        requestMutex.unlock();
        if (outstandingRequests) {
            requestAvailable.wakeOne();
        }

        // Find or generate a thumbnail for this URI
        path = processUri(uri);
    }
}

QString ThumbnailService::processUri(const QString &uri)
{
    NemoThumbnailCache *cache = NemoThumbnailCache::instance();
    QThread::msleep(100);
    return QString("dummy path");
}

void ThumbnailService::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId()) {
        timer.stop();

        qWarning() << "ThumbnailService expired after linger period";

        {
            QMutexLocker lock(&requestMutex);
            serviceFinished = true;
        }
        requestAvailable.wakeAll();

        emit serviceExpired();
    }
}

#include "thumbnailservice.moc"
