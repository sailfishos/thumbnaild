/*
 * Copyright (C) 2012 Jolla Ltd
 * Contact: Andrew den Exter <andrew.den.exter@jollamobile.com>
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


#include <QtDebug>

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/display.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#include <QImage>

namespace {
struct Thumbnailer
{
    Thumbnailer()
        : format(0)
        , codecContext(0)
        , scale(0)
        , frame(0)
    {
    }

    ~Thumbnailer()
    {
        if (scale) {
            sws_freeContext(scale);
        }

        if (codecContext) {
            avcodec_free_context(&codecContext);
        }

        if (format) {
            avformat_free_context(format);
        }

        if (frame) {
            av_frame_free(&frame);
        }
    }

    AVFormatContext *format;
    AVCodecContext *codecContext;
    SwsContext *scale;
    AVFrame *frame;
};
}

bool getVideoPacket(Thumbnailer &thumbnailer, int streamIndex, AVPacket *packet)
{
    bool frameFound = false;

    // Find first frame belonging to the requested stream
    while (!frameFound) {
        if (av_read_frame(thumbnailer.format, packet) < 0) {
            break;
        } else if (packet->stream_index == streamIndex) {
            frameFound = true;
        } else {
            av_packet_unref(packet);
        }
    }

    return frameFound;
}

QImage createVideoThumbnail(const QString &fileName, const QSize &requestedSize, bool crop)
{
    QImage image;

    Thumbnailer thumbnailer;
    AVCodec *codec;

    int streamIndex = 0;
    int rotation_angle = 0;

    if (avformat_open_input(&thumbnailer.format, fileName.toUtf8(), 0, 0)) {
        return image;
    } else if (avformat_find_stream_info(thumbnailer.format, NULL) < 0) {
        return image;
    } else if ((streamIndex = av_find_best_stream(
                thumbnailer.format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) < 0) {
        return image;
    }

    const AVStream *stream = thumbnailer.format->streams[streamIndex];

    const uint8_t *data = av_stream_get_side_data(const_cast<AVStream *>(stream),
                                                  AV_PKT_DATA_DISPLAYMATRIX, NULL);
    if (data) {
        rotation_angle = av_display_rotation_get((int32_t *)data);
    }

    thumbnailer.codecContext = avcodec_alloc_context3(codec);
    if (!thumbnailer.codecContext) {
        return image;
    }

    avcodec_parameters_to_context(thumbnailer.codecContext, stream->codecpar);

    if (avcodec_open2(thumbnailer.codecContext, codec, 0) < 0) {
        return image;
    }

    AVPacket packet;
    thumbnailer.frame = av_frame_alloc();

    // Find first frame before seeking
    while (getVideoPacket(thumbnailer, streamIndex, &packet)) {
        if (avcodec_send_packet(thumbnailer.codecContext, &packet) < 0) {
            av_packet_unref(&packet);
            return image;
        }

        int ret = avcodec_receive_frame(thumbnailer.codecContext, thumbnailer.frame);
        av_packet_unref(&packet);
        if (ret >= 0) {
            av_frame_unref(thumbnailer.frame);
            break;
        } else if (ret != AVERROR(EAGAIN)) {
            return image;
        }
    }

    int64_t seekTo = thumbnailer.format->duration / 10;

    if (av_seek_frame(thumbnailer.format, -1, seekTo, 0) < 0) {
        // If seeking failed go back to beginning of the stream
        if (av_seek_frame(thumbnailer.format, -1, 0, AVSEEK_FLAG_BACKWARD)) {
            return image;
        }
    }
    avcodec_flush_buffers(thumbnailer.codecContext);

    bool foundKeyFrame = false;
    for (bool atEnd = false; !foundKeyFrame ; ) {
        if (!getVideoPacket(thumbnailer, streamIndex, &packet)) {
            // The seek went to the end of the stream or the stream can't be read for some reason.
            // If it's the former then after seeking to the start of the video it should be possible
            // to read a valid packet, otherwise it's the latter so give up the second time around.
            if (atEnd) {
                return image;
            } else {
                atEnd = true;
                av_seek_frame(thumbnailer.format, streamIndex, -1, AVSEEK_FLAG_BACKWARD);
                continue;
            }
        }

        if (avcodec_send_packet(thumbnailer.codecContext, &packet) < 0) {
            av_packet_unref(&packet);
            return image;
        }

        while (1) {
            int ret = avcodec_receive_frame(thumbnailer.codecContext, thumbnailer.frame);

            if (ret >= 0) {
                if (thumbnailer.frame->key_frame) {
                    foundKeyFrame = true;
                    break;
                } else {
                    av_frame_unref(thumbnailer.frame);
                }
            } else if (ret != AVERROR(EAGAIN)) {
                av_packet_unref(&packet);
                return image;
            }
        }

        av_packet_unref(&packet);
    }

    QSize sourceSize(thumbnailer.codecContext->width, thumbnailer.codecContext->height);
    if (rotation_angle % 180 != 0) {
        sourceSize.transpose();
    }

    QSize size = sourceSize.scaled(
                requestedSize,
                crop ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);

    if (rotation_angle % 180 != 0) {
        size.transpose();
    }

    if (SwsContext *scale = sws_getCachedContext(
                0,
                thumbnailer.codecContext->width,
                thumbnailer.codecContext->height,
                thumbnailer.codecContext->pix_fmt,
                size.width(),
                size.height(),
                AV_PIX_FMT_BGRA,
                SWS_BICUBIC,
                0,
                0,
                0)) {

        image = QImage(size.width(), size.height(), QImage::Format_RGB32);

        AVFrame* result = av_frame_alloc();
        result->width = size.width();
        result->height = size.height();
        result->format = AV_PIX_FMT_BGRA;

        if (av_frame_get_buffer(result, 32) < 0) {
            av_frame_free(&result);
            return image;
        }

        sws_scale(scale,
                  thumbnailer.frame->data,
                  thumbnailer.frame->linesize,
                  0,
                  thumbnailer.codecContext->height,
                  result->data,
                  result->linesize);

        for (int x = 0; x < image.height(); x++) {
            memcpy(image.scanLine(x), &result->data[0][x * result->linesize[0]], 4 * size.width());
        }

        av_frame_free(&result);

        if (crop) {
            QRect subrect(QPoint(0, 0), requestedSize);
            subrect.moveCenter(QPoint((size.width() - 1) / 2, (size.height() - 1) / 2));
            image = image.copy(subrect);
        }

        // It looks like libav assumes negative values are clockwise rotations
        // while Qt assumes negative values to be counter clockwise. Just fix up the mess.
        rotation_angle *= -1;

        if (rotation_angle < 0) {
            rotation_angle += 360;
        }

        if (rotation_angle != 0) {
            QTransform transform;
            transform.rotate(rotation_angle);
            image = image.transformed(transform);
        }
    }

    return image;
}
