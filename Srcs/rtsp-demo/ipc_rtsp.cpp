/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2018, Open AI Lab
 * Author: minglu@openailab.com
 *
 */

#include "ipc_rtsp.hpp"

ipcCamera::ipcCamera(__u32 w, __u32 h, RgaRotate r, int V, int H, __u32 cx, __u32 cy, __u32 cw, __u32 ch)
{
    recv_count = 0;
    dec_count = 0;

    width = w;
    height = h;
    if(V == 1)
        rotate = RGA_ROTATE_VFLIP;
    else if(H == 1)
        rotate = RGA_ROTATE_HFLIP;
    else
        rotate = r;
    cropx = cx;
    cropy = cy;
    cropw = cw;
    croph = ch;
}


ipcCamera::~ipcCamera()
{
    if(dec != NULL)
        MppDecoderDestroy(dec);
    if(rga != NULL)
        RgaDestroy(rga);
}

int ipcCamera::init(DecodeType type)
{
    rga = RgaCreate();
    if(rga == NULL)
        return -1;
    dec = MppDecoderCreate(type);
    if(dec == NULL) {
        RgaDestroy(rga);
        return -1;
    }

#ifdef RTSP_DEMO_DEBUG
    f_h264 = fopen("./test.264", "w+");
    f_nv12 = fopen("./test.nv12", "w+");
    f_rgb24 = fopen("./test.rgb24", "w+");
#endif

    return 0;
}


#ifdef RTSP_DEMO_DEBUG
void ipcCamera::saveToFile(FILE **fp, void *buf, size_t len, int count)
{

    if(*fp != NULL) {
        if(count < 5)
            fwrite(buf, len, 1, *fp);
        else {
            fclose(*fp);
            *fp = NULL;
        }
    }
}
#endif

unsigned long ipcCamera::microTime()
{
    struct timeval t;

    gettimeofday(&t, NULL);

    return (unsigned long)(t.tv_sec) * 1000000 + t.tv_usec;
}



int ipcCamera::enqueue(unsigned char *buf, size_t len)
{
    int ret;
    ret = dec->ops->enqueue(dec, buf, len);

#ifdef RTSP_DEMO_DEBUG
    saveToFile(&f_h264, buf, len, recv_count);
#endif

    recv_count++;
    return ret;
}

DecFrame* ipcCamera::dequeue(void)
{
    DecFrame *frame = dec->ops->dequeue(dec);

    if(frame != NULL) {
        dec_count++;
    }
    return frame;
}

void ipcCamera::freeFrame(DecFrame *frame)
{
    dec->ops->freeFrame(frame);
}

void ipcCamera::rgaProcess(DecFrame *frame, __u32 dstFormat, Mat* mat)
{
#ifdef RTSP_DEMO_DEBUG
    saveToFile(&f_nv12, frame->data, frame->size, dec_count);
#endif
    rga->ops->initCtx(rga);
    rga->ops->setSrcBufferPtr(rga, frame->data);
    rga->ops->setDstBufferPtr(rga, mat->data);

    rga->ops->setSrcFormat(rga, frame->v4l2Format, frame->coded_width, frame->coded_height);
    rga->ops->setDstFormat(rga, dstFormat, width, height);
    rga->ops->setRotate(rga, rotate);
    if(cropw > 0 && croph > 0) {
        rga->ops->setSrcCrop(rga, cropx, cropy, cropw, croph);
        rga->ops->setDstCrop(rga, 0, 0, width, height);
    }

    rga->ops->go(rga);


#ifdef RTSP_DEMO_DEBUG
    saveToFile(&f_rgb24, mat.data, width * height * 3, dec_count);
#endif

}


void ipcCamera::rgaConvertFormat(Mat src, Mat dst, __u32 srcFormat, __u32 dstFormat, __u32 width, __u32 height)
{
    rga->ops->initCtx(rga);
    rga->ops->setSrcBufferPtr(rga, src.data);
    rga->ops->setDstBufferPtr(rga, dst.data);

    rga->ops->setSrcFormat(rga, srcFormat, width, height);
    rga->ops->setDstFormat(rga, dstFormat, width, height);

    rga->ops->go(rga);
}

RgaBuffer* ipcCamera::allocBuffer(__u32 v4l2Format, __u32 width, __u32 height)
{
    return rga->ops->allocDmaBuffer(rga, v4l2Format, width, height);
}

void ipcCamera::freeBuffer(RgaBuffer *buf)
{
    rga->ops->freeDmaBuffer(rga, buf);
}

void ipcCamera::onStreamReceive(unsigned char *buf, size_t len)
{
    while(enqueue(buf, len) != 0)
        usleep(1000);
}







