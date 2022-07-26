/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http:// www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"
#include "wifiiot_pwm.h"

#include "hi_pwm.h"

#define ATTR.STACK_SIZE 1024
#define TWO 2
#define FORTY 40
#define ONE_THOUSAND 1000

static volatile int g_buttonPressed = 0;
static const uint16_t g_tuneFreqs[] = {
    0, // 40M Hz 对应的分频系数：
    38223, // 1046.5
    34052, // 1174.7
    30338, // 1318.5
    28635, // 1396.9
    25511, // 1568
    22728, // 1760
    20249, // 1975.5
    51021 // 5_ 783.99 // 第一个八度的 5
};

// 曲谱音符
static const uint8_t g_scoreNotes[] = {
    // 《两只老虎》简谱：http://www.jianpu.cn/pu/33/33945.htm
    1, 2, 3, 1,        1, 2, 3, 1,        3, 4, 5,  3, 4, 5,
    5, 6, 5, 4, 3, 1,  5, 6, 5, 4, 3, 1,  1, 8, 1,  1, 8, 1, // 最后两个 5 应该是低八度的，链接图片中的曲谱不对，声音到最后听起来不太对劲
};

// 曲谱时值
static const uint8_t g_scoreDurations[] = {
    4, 4, 4, 4,        4, 4, 4, 4,        4, 4, 8,  4, 4, 8,
    3, 1, 3, 1, 4, 4,  3, 1, 3, 1, 4, 4,  4, 4, 8,  4, 4, 8,
};

// 注释蜂鸣器音乐任务
static void *BeeperMusicTask(const char *arg)
{
    (void)arg;

    printf("BeeperMusicTask start!\r\n");

    hi_pwm_set_clock(PWM_CLK_XTAL); // 设置时钟源为晶体时钟（40MHz，默认时钟源160MHz）

    for (size_t i = 0; i < sizeof(g_scoreNotes)/sizeof(g_scoreNotes[0]); i++) {
        uint32_t tune = g_scoreNotes[i]; // 音符
        uint16_t freqDivisor = g_tuneFreqs[tune];
        uint32_t tuneInterval = g_scoreDurations[i] * (125*1000); // 音符时间
        printf("%d %d %d %d\r\n", tune, (FORTY*ONE_THOUSAND*ONE_THOUSAND) / freqDivisor, freqDivisor, tuneInterval);
        PwmStart(WIFI_IOT_PWM_PORT_PWM0, freqDivisor/TWO, freqDivisor);
        usleep(tuneInterval);
        PwmStop(WIFI_IOT_PWM_PORT_PWM0);
    }

    return NULL;
}

static void StartBeepMusicTask(void)
{
    osThreadAttr_t attr;

    GpioInit();

    // 蜂鸣器引脚 设置为 PWM功能
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9, WIFI_IOT_IO_FUNC_GPIO_9_PWM0_OUT);
    PwmInit(WIFI_IOT_PWM_PORT_PWM0);

    WatchDogDisable();

    attr.name = "BeeperMusicTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = ATTR.STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)BeeperMusicTask, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create BeeperMusicTask!\n");
    }
}

SYS_RUN(StartBeepMusicTask);