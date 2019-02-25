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


const string winname = "ipc-rtsp";
//const string ipcUrl="rtsp://192.168.88.76/cam/realmonitor?channel=1&subtype=0"
const string ipcUrl="rtsp://192.168.88.174/video1";
const string ipcUser="admin";
const string ipcPassword="12345678";



typedef struct _VideoDecoder {
    ipcCamera* ipc;
    Mat* image;
} VideoDecoder;

static void usage(char **argv)
{
    printf(
        "Usage: %s [Options]\n\n"
        "Options:\n"
        "-w, --width                  Destination images's width\n"
        "-h, --height                 Destination images's height\n"
        "-r, --rotate                 Image rotation degree, the value should be 90, 180 or 270\n"
        "-V, --vflip                  Vertical Mirror\n"
        "-H, --hflip                  Horizontal Mirror\n"
        "-c, --crop                   Crop, format: x,y,w,h\n"
        "\n",
        argv[0]);
}

static const char *short_options = "d:w:h:r:VHc:";

static struct option long_options[] = {
    {"decoder", required_argument, NULL, 'd'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"rotate", required_argument, NULL, 'r'},
    {"vflip", no_argument, NULL, 'V'},
    {"hflip", no_argument, NULL, 'H'},
    {"crop", required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
};

void *onVideoDecode(void *data)
{
    VideoDecoder *dec = (VideoDecoder*)data;
    ipcCamera *ipc = dec->ipc;
    Mat *mat = dec->image;
    DecFrame *frame = NULL;

    unsigned long long t1, t2;

    while(1) {
        frame = ipc->dequeue();
        if(frame != NULL) {
            t1 = ipc->microTime();
            ipc->rgaProcess(frame, V4L2_PIX_FMT_RGB24, mat);
            t2 = ipc->microTime();
            usleep(2000);
            printf("rga time: %llums\n", (t2 - t1)/1000);
            ipc->freeFrame(frame);
        }
        usleep(1000);
    }

    return NULL;
}



static void parse_crop_parameters(char *crop, __u32 *cropx, __u32 *cropy, __u32 *cropw, __u32 *croph)
{
    char *p, *buf;
    const char *delims = ".,";
    __u32 v[4] = {0,0,0,0};
    int i = 0;

    buf = strdup(crop);
    p = strtok(buf, delims);
    while(p != NULL) {
        v[i++] = atoi(p);
        p = strtok(NULL, delims);

        if(i >=4)
            break;
    }

    *cropx = v[0];
    *cropy = v[1];
    *cropw = v[2];
    *croph = v[3];
}

int main(int argc, char **argv)
{
    int ret, c, r;
    pthread_t id;

    Mat image;

    __u32 width = 640, height = 360;
    RgaRotate rotate = RGA_ROTATE_NONE;
    __u32 cropx = 0, cropy = 0, cropw = 0, croph = 0;
    int vflip = 0, hflip = 0;
    DecodeType type=DECODE_TYPE_H264;

    VideoDecoder* decoder=(VideoDecoder*)malloc(sizeof(VideoDecoder));

    while((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case 'd':
            if(strncmp(optarg, "h264", 5) == 0)
                type = DECODE_TYPE_H264;
            if(strncmp(optarg, "h265", 5) == 0)
                type = DECODE_TYPE_H265;
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'r':
            r = atoi(optarg);
            switch(r) {
            case 0:
                rotate = RGA_ROTATE_NONE;
                break;
            case 90:
                rotate = RGA_ROTATE_90;
                break;
            case 180:
                rotate = RGA_ROTATE_180;
                break;
            case 270:
                rotate = RGA_ROTATE_270;
                break;
            default:
                printf("roate %d is not supported\n", r);
                return -1;
            }
            break;
        case 'V':
            vflip = 1;
            break;
        case 'H':
            hflip = 1;
            break;
        case 'c':
            parse_crop_parameters(optarg, &cropx, &cropy, &cropw, &croph);
            break;
        default:
            usage(argv);
            return 0;
        }
    }

    printf("decoder=%u, width = %u, height = %u, rotate = %u, vflip = %d, hflip = %d, crop = [%u, %u, %u, %u]\n",
           type, width, height, rotate, vflip, hflip, cropx, cropy, cropw, croph);


    fcv::namedWindow(winname);

    ipcCamera *ipc = new ipcCamera(width, height, rotate, vflip, hflip, cropx, cropy, cropw, croph);

    decoder->ipc = ipc;
    decoder->image = &image;

    RtspClient client(ipcUrl, ipcUser, ipcPassword);
    ret = ipc->init(type);

    image.create(cv::Size(width, height), CV_8UC3);



    if(ret < 0)
        return ret;
    ret = pthread_create(&id, NULL, onVideoDecode, decoder);
    if(ret < 0) {
        printf("pthread_create failed\n");
        return ret;
    }

    client.setDataCallback(std::bind(&ipcCamera::onStreamReceive, ipc, std::placeholders::_1, std::placeholders::_2));
    client.enable();

    while(1) {

        imshow(winname, image, NULL);
        fcv::waitKey(1);
        usleep(20000);

    }
    pthread_join(id, NULL);
	image.release();
	delete ipc;
	free(decoder);
    return 0;
}
