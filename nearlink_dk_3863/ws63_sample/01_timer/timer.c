 /*
 Copyright (C) 2024 HiHope Open Source Organization .
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */
#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

static int times = 0;

void cb_timeout_periodic(void *arg)
{
    (void)arg;
    times++;
}

void timer_periodic(void)
{
    osTimerId_t periodic_tid = osTimerNew(cb_timeout_periodic, osTimerPeriodic, NULL, NULL);
    const uint32_t DelayTimeMs = 100;
    const uint32_t DelayTime = 3;

    if (periodic_tid == NULL) {
        printf("[Timer Test] osTimerNew(periodic timer) failed.\r\n");
        return;
    } else {
        printf("[Timer Test] osTimerNew(periodic timer) success, tid: %p.\r\n", periodic_tid);
    }
    osStatus_t status = osTimerStart(periodic_tid, 100);
    if (status != osOK) {
        printf("[Timer Test] osTimerStart(periodic timer) failed.\r\n");
        return;
    } else {
        printf("[Timer Test] osTimerStart(periodic timer) success, wait a while and stop.\r\n");
    }

    while (times < DelayTime) {
        printf("[Timer Test] times:%d.\r\n", times);
        osDelay(DelayTimeMs);
    }

    status = osTimerStop(periodic_tid);
    printf("[Timer Test] stop periodic timer, status :%d.\r\n", status);
    status = osTimerDelete(periodic_tid);
    printf("[Timer Test] kill periodic timer, status :%d.\r\n", status);
}

static void TimerTestTask(void)
{
    osThreadAttr_t attr;

    attr.name = "timer_periodic";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)timer_periodic, NULL, &attr) == NULL) {
        printf("[TimerTestTask] Falied to create timer_periodic!\n");
    }
}

APP_FEATURE_INIT(TimerTestTask);