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
 *
 *
 */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <mtd/mtd-user.h>
#include <errno.h>

#define GPIO_BANK   32
#define RK_GPIO(chip, offset)       (chip * GPIO_BANK + offset)

#define GPIO_SYS_VALUE_PATH(GPIO)  "/sys/class/gpio/gpio" #GPIO "/value"

static int key_gpios[] = {
    RK_GPIO(2, 12),		//GPIO2_B4
    RK_GPIO(4, 24),		//GPIO4_D0
};

#define ARR_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define GPIO_NUM		ARR_SIZE(key_gpios)

typedef unsigned char  u8;
struct mtd_info_user gMtdInfo;

#ifdef ANDROID_MTD
#define	MTD_DEV_PATH	"/dev/mtd/mtd0"
#else
#define	MTD_DEV_PATH	"/dev/mtd0"
#endif

static int non_region_erase(int Fd, int start, int count, int unlock)
{
    mtd_info_t meminfo;

    if (ioctl(Fd,MEMGETINFO,&meminfo) == 0)
    {
        erase_info_t erase;

        erase.start = start;

        erase.length = meminfo.erasesize;

        for (; count > 0; count--) {
            //printf("\rPerforming Flash Erase of length %u at offset 0x%x",
            //        erase.length, erase.start);
            fflush(stdout);

            if(unlock != 0)
            {
                //Unlock the sector first.
                //printf("\rPerforming Flash unlock at offset 0x%x",erase.start);
                if(ioctl(Fd, MEMUNLOCK, &erase) != 0)
                {
                    perror("\nMTD Unlock failure");
                    close(Fd);
                    return -EBUSY;
                }
            }

            if (ioctl(Fd,MEMERASE,&erase) != 0)
            {
                perror("\nMTD Erase failure");
                close(Fd);
                return -EIO;
            }
            erase.start += meminfo.erasesize;
        }
        //printf(" done\n");
    }
    return 0;
}

int flash_data_comparison_test(const char *mtd_path)
{
    int i;
    int fd;
    struct mtd_info_user *info = &gMtdInfo;
    u8 *compared_data;
    u8 *read_buf;
    int regcount;
    int ret;

    // Open the device
    if ((fd = open(mtd_path, O_RDWR)) < 0)
    {
        fprintf(stderr,"** File open %s error\n", mtd_path);
        return -EINVAL;
    }
    else
    {
        ioctl(fd, MEMGETINFO, info);
        //printf("info.size=%d\ninfo.erasesize=%d\ninfo.writesize=%d\ninfo.oobsize=%d\n",info.size,info.erasesize,info.writesize,info.oobsize);
    }
    compared_data = (u8 *)malloc(info->erasesize);
    read_buf = (u8 *)malloc(info->erasesize);

    if (ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0)
    {
        //printf("regcount=%d\n",regcount);
    }

    memset(compared_data, 0xab, info->erasesize);  // structure compared data

    ret = non_region_erase(fd, 0, 1, 0);  //not all pages, just one page
    if (ret < 0) {
        printf("** Erase fail.\n");
        goto fail;
    }

    ret = write(fd, compared_data, info->erasesize);
    if (ret != (int)info->erasesize) {
        printf("** Write fail.\n");
        goto fail;
    }

    ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) {
        printf("** lseek fail.\n");
        goto fail;

    }
    ret = read(fd, read_buf, info->erasesize);
    if (ret != (int)info->erasesize) {
        printf("** read fail.\n");
        goto fail;
    }

    if (memcmp(read_buf, compared_data, info->erasesize)) {
        printf("** Flash Data Comparison is Fail!!\n");
        return -EINVAL;
    }

    printf("## Flash Data Comparison is OK!!\n");

    return 0;
fail:
    return -EIO;
}

#define REF_VOLT_5V0	5000
#define REF_VOLT_3V3	3300
#define REF_VOLT_12V0	12000
#define TEST_PATH_5V0	"/sys/devices/platform/ff120000.i2c/i2c-2/2-0041/hwmon/hwmon0/in1_input"
#define TEST_PATH_3V3	"/sys/devices/platform/ff150000.i2c/i2c-6/6-0047/hwmon/hwmon1/in1_input"
#define TEST_PATH_12V0	"/sys/devices/platform/ff160000.i2c/i2c-7/7-0040/hwmon/hwmon2/in1_input"
#define DRIFT_VALUE		320

int voltage_detect(void)
{
    int ref_voltage[] = {REF_VOLT_5V0, REF_VOLT_3V3, REF_VOLT_12V0};
    const char *test_path[] = {TEST_PATH_5V0, TEST_PATH_3V3, TEST_PATH_12V0};
    unsigned long i;
    int fd[ARR_SIZE(ref_voltage)];
    int error = -1;
    char buf[16];
    int val;

    for (i = 0; i < ARR_SIZE(ref_voltage); i++) {
        fd[i] = open(test_path[i], O_RDONLY);
        if (fd[i] < 0) {
            printf("** OPEN %s fail\n", test_path[i]);
            error = -ENODEV;
            goto fail0;
        }

        error = read(fd[i], buf, 16);
        if(error < 0) {
            printf("** read error\n");
            error = -EIO;
            goto fail1;
        }

        val = atoi(buf);
        if (val > (ref_voltage[i] - DRIFT_VALUE) && val < (ref_voltage[i] + DRIFT_VALUE)) {
            printf("## Reference %dmv, Read %dmv is valid.\n", ref_voltage[i], val);
            error = 0;
        } else {
            printf("## Reference %dmv, Read %dmv is invalid.\n", ref_voltage[i], val);
            error = -EINVAL;
            goto fail1;
        }

        close(fd[i]);
    }

    printf("## Voltage Detect is OK!!\n");
    return error;

fail1:
    for (i = 0; i < ARR_SIZE(ref_voltage); i++) {
        if (fd[i] < 0)
            break;
        close(fd[i]);
    }

fail0:
    return error;
}

#define DIGIT_TUBE_PATH		"/sys/devices/platform/extbrd/digit_tube"
#define NO_TEST						(0)
#define FLASH_TEST_SUCCESS			(1)
#define VOLT_DETECT_SUCCESS			(2)
#define STR(x)		#x

static int digit_tube_display(const char *ch)
{
    int fd;
    int cnt;

    fd = open(DIGIT_TUBE_PATH, O_WRONLY);
    if (fd < 0) {
        printf("** open %s fail\n", DIGIT_TUBE_PATH);
        return -EEXIST;
    }

    cnt = write(fd, ch, 1);
    if (cnt != 1) {
        printf("** write %s fail\n", DIGIT_TUBE_PATH);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int gpio_fd[GPIO_NUM];
    int ret = 0;
    int i, n;
    struct pollfd fds[GPIO_NUM];
    char *buff[GPIO_NUM];
    int edge_fd[GPIO_NUM];
    ssize_t cnt;
    int result = NO_TEST;

    for (i = 0, n = 0; i < (int)GPIO_NUM; i++, n++) {
        buff[n] = (char *)malloc(64);
        if (buff[n] == NULL) {
            printf("** malloc fail.\n");
            ret = -ENOMEM;
            goto fail0;
        }

        snprintf(buff[n], 64, "/sys/class/gpio/gpio%d/edge", key_gpios[i]);
        edge_fd[i] = open(buff[n], O_RDWR);
        if (edge_fd[i] < 0) {
            printf("** open %s error\n", buff[n]);
            ret = -EEXIST;
            goto fail1;
        }

        cnt = write(edge_fd[i], "falling", sizeof("falling"));
        if (cnt != sizeof("falling")) {
            printf("** write %s fail.\n", buff[n]);
            ret = -EIO;
            goto fail1;
        }

        snprintf(buff[n], 64, "/sys/class/gpio/gpio%d/value", key_gpios[i]);
        gpio_fd[i] = open(buff[n], O_RDONLY);
        if(gpio_fd[i] < 0) {
            printf("** open %s error\n", buff[n]);
            ret = -EEXIST;
            goto fail2;
        }

        fds[i].fd = gpio_fd[i];
        fds[i].events  = POLLPRI;
        ret = read(gpio_fd[i], buff[n], 10);
        if(ret < 0) {
            printf("** read gpio error\n");
            ret = -EIO;
            goto fail2;
        }
    }

    printf("## Please press GPIO_KEY1 or GPIO_KEY2 within 10 seconds!\n");
    while(1) {
        ret = poll(fds, 2, 10000); //timeout : 10s
        if(ret < 0) {
            printf("** poll error\n");
            ret = -EIO;
            goto fail2;
        } else if (ret == 0) {
            printf("** Wait 10s, No Key Press, Exit!\n");
            ret = -ETIME;
            goto fail2;
        }

        for (i = 0, n = 0; i < (int)GPIO_NUM; i++, n++) {
            if(fds[i].revents & POLLPRI) {
                ret = lseek(gpio_fd[i], 0, SEEK_SET);
                if(ret < 0) {
                    printf("** lseek key%d error\n", i + 1);
                    ret = -EIO;
                    goto fail2;
                }
                ret = read(gpio_fd[i], buff[n], 10);
                if(ret < 0) {
                    printf("** read key%d error\n", i + 1);
                    ret = -EIO;
                    goto fail2;
                }

                if (i == 0) {
                    //printf("catch GPIO_KEY%d interrupt, do flash test.\n", i+1);
                    printf("------------- Start Flash Test -------------\n");
                    if (flash_data_comparison_test(MTD_DEV_PATH) < 0) {
                        digit_tube_display(STR(1));
                        ret = -EINVAL;
                        printf("------------ Flash Test is Fail ------------\n");
                        goto fail2;
                    } else {
                        digit_tube_display(STR(o));
                        result |= FLASH_TEST_SUCCESS;
                        printf("--------- Flash Test is Successful ---------\n");
                    }
                }

                if (i == 1) {
                    //printf("catch GPIO_KEY%d interrupt, do voltage detect.\n", i+1);
                    printf("------------ Start Voltage Test ------------\n");
                    if (voltage_detect() < 0) {
                        digit_tube_display(STR(2));
                        ret = -EINVAL;
                        printf("----------- Voltage Test is Fail -----------\n");
                        goto fail2;
                    } else {
                        digit_tube_display(STR(o));
                        result |= VOLT_DETECT_SUCCESS;
                        printf("-------- Voltage Test is Successful --------\n");
                    }
                }
                if (result == (FLASH_TEST_SUCCESS | VOLT_DETECT_SUCCESS)) {
                    digit_tube_display(STR(0));
                    ret = 0;
                    printf("----------- ExtIO Test is Passed -----------\n");
                    goto success;
                }
            }
        }
    }

success:
    for (i = 0; i < (int)GPIO_NUM; i++) {
        close(gpio_fd[i]);
        close(edge_fd[i]);
        free(buff[i]);
    }

    return 0;

fail2:
    for (i = 0; i < (int)GPIO_NUM; i++) {
        if (gpio_fd[i] < 0)
            break;
        close(gpio_fd[i]);
    }

fail1:
    for (i = 0; i < (int)GPIO_NUM; i++) {
        if (edge_fd[i] < 0)
            break;
        close(edge_fd[i]);
    }

fail0:
    for (n = 0; n < (int)GPIO_NUM; n++) {
        if (buff[n] == NULL)
            break;
        free(buff[n]);
    }
    printf("--------- ExtIO Test is Not Passed ---------\n");

    return ret;
}
