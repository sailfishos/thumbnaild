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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QtDebug>

#include <iostream>

extern QImage createVideoThumbnail(const QString &fileName, const QSize &requestedSize, bool crop);

namespace {

int generateThumbnail(QFile &input, QFile &output, int width, int height, bool crop)
{
    QImage thumbnail = ::createVideoThumbnail(QFile::encodeName(input.fileName()), QSize(width, height), crop);
    if (thumbnail.isNull()) {
        std::cerr << std::endl << "Unable to generate thumbnail image from: " << QFile::encodeName(input.fileName()).constData();
        return 2;
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

    QCommandLineOption outputOption({ "o", "output" }, "Path of the output thumbnail.", "output");
    QCommandLineOption widthOption({ "w", "width" }, "Width of the output thumbnail.", "width");
    QCommandLineOption heightOption({ "h", "height" }, "Height of the output thumbnail.", "height");
    QCommandLineOption cropOption({ "c", "crop" }, "Crop the input to preserve the aspect ratio.");

    QCommandLineParser parser;
    parser.setApplicationDescription("thumbnaild video thumbnail generator");
    parser.addVersionOption();
    parser.addPositionalArgument("input", "Path of the input file.");
    parser.addOption(outputOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
    parser.addOption(cropOption);

    parser.process(app);

    const QStringList input(parser.positionalArguments());
    if (input.isEmpty()) {
        std::cerr << std::endl << "Error: Input file path is required." << std::endl << std::endl;
        parser.showHelp(1);
    } else if (input.count() > 1) {
        std::cerr << std::endl << "Error: Only one input file is permitted." << std::endl << std::endl;
        parser.showHelp(1);
    }

    int width = parser.value(widthOption).toInt();
    if (width < 1 || width > 512) {
        std::cerr << std::endl << "Error: Invalid width." << std::endl << std::endl;
        parser.showHelp(1);
    }

    int height = parser.value(heightOption).toInt();
    if (height < 1 || height > 512) {
        std::cerr << std::endl << "Error: Invalid height." << std::endl << std::endl;
        parser.showHelp(1);
    }

    if (!parser.isSet(outputOption)) {
        std::cerr << std::endl << "Error: Output file path is required." << std::endl << std::endl;
        parser.showHelp(1);
    }

    QFile inputFile(input.front());
    if (!inputFile.open(QIODevice::ReadOnly)) {
        std::cerr << std::endl << "Error: Input file path cannot be read: " << QFile::encodeName(inputFile.fileName()).constData() << std::endl << std::endl;
        return 1;
    }

    QFile outputFile(parser.value(outputOption));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << std::endl << "Error: Output file path cannot be written: " << QFile::encodeName(outputFile.fileName()).constData() << std::endl << std::endl;
        return 1;
    }

    return generateThumbnail(inputFile, outputFile, width, height, parser.isSet(cropOption));
}

