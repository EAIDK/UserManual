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

#ifndef __IPC_RTSP_HPP__
#define __IPC_RTSP_HPP__

#include "rtspclient.h"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <chrono>
#include "fastcv.hpp"
#include "openai_io.hpp"
#include <rockchip/rockchip_mpp.h>

extern "C" {
#include <rockchip/rockchip_rga.h>
}

using namespace fcv;
using namespace std;


//#define RTSP_DEMO_DEBUG	1

class ipcCamera {

private:
    RockchipRga *rga;
    MppDecoder *dec;

    int recv_count = 0;
    int dec_count = 0;

    __u32 width;
    __u32 height;
    RgaRotate rotate;
    __u32 cropx;
    __u32 cropy;
    __u32 cropw;
    __u32 croph;
    FILE *f_h264 = NULL;
    FILE *f_nv12 = NULL;
    FILE *f_rgb24 = NULL;

public:
    ipcCamera(__u32 w, __u32 h, RgaRotate r, int V, int H, __u32 cx, __u32 cy, __u32 cw, __u32 ch);
    ~ipcCamera();
    int init(DecodeType type);
#ifdef RTSP_DEMO_DEBUG
    void saveToFile(FILE **fp, void *buf, size_t len, int count);
#endif
    unsigned long microTime();
    int enqueue(unsigned char *buf, size_t len);
    DecFrame *dequeue(void);
    void freeFrame(DecFrame *frame);
    void rgaProcess(DecFrame *frame, __u32 dstFormat, Mat* mat);
    void rgaConvertFormat(Mat src, Mat dst, __u32 srcFormat, __u32 dstFormat, __u32 width, __u32 height);
    RgaBuffer *allocBuffer(__u32 v4l2Format, __u32 width, __u32 height);
    void freeBuffer(RgaBuffer *buf);
    void onStreamReceive(unsigned char *buf, size_t len);
};


#endif