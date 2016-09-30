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

#include "thumbnailutility.h"

#include <QImage>
#include <QScopedPointer>
#include <QtDebug>
#include <qmath.h>

#include <poppler-qt5.h>

#include <iostream>

namespace {

int generateThumbnail(QFile &input, QFile &output, int width, int height, bool crop)
{
    QScopedPointer<Poppler::Document> pdf(Poppler::Document::load(input.fileName()));
    if (!pdf) {
        std::cerr << std::endl << "Unable to load PDF data from: " << QFile::encodeName(input.fileName()).constData();
        return 2;
    }

    QScopedPointer<Poppler::Page> page(pdf->page(0));
    if (!page) {
        std::cerr << std::endl << "Unable to load first page from: " << QFile::encodeName(input.fileName()).constData();
        return 2;
    }

    const QSize requestedSize(width, height);
    const QSizeF inputSize(page->pageSizeF());
    const QSizeF targetSize(inputSize.scaled(requestedSize, crop ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio));

    QImage thumbnail = page->thumbnail();
    if (thumbnail.isNull() || thumbnail.width() < qFloor(targetSize.width()) || thumbnail.height() < qFloor(targetSize.height())) {
        // Render the image instead, scaled to the size we need
        const double scale = 72.0 / std::max(inputSize.width() / targetSize.width(), inputSize.height() / targetSize.height());
        thumbnail = page->renderToImage(scale, scale);
    }
    if (thumbnail.isNull()) {
        std::cerr << std::endl << "Unable to render to thumbnail image: " << QFile::encodeName(output.fileName()).constData();
        return 2;
    }

    if (crop) {
        QRect subrect(QPoint(0, 0), requestedSize);
        QSize size(thumbnail.size());
        subrect.moveCenter(QPoint((size.width() - 1) / 2, (size.height() - 1) / 2));
        thumbnail = thumbnail.copy(subrect);
    } else {
        thumbnail = thumbnail.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (!thumbnail.save(&output, thumbnail.hasAlphaChannel() ? "PNG" : "JPG")) {
        std::cerr << std::endl << "Unable to save thumbnail image to: " << QFile::encodeName(output.fileName()).constData();
        return 2;
    }

    output.flush();
    output.close();
    return 0;
}

}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("0.1"));

    ThumbnailUtility(app, "thumbnaild PDF thumbnail generator", &generateThumbnail);
}

