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


#include "aht20.h"      // AHT20数字温湿度传感器驱动接口
#include <stdio.h>      
#include <unistd.h>     
#include <string.h>  
#include "iot_i2c.h"
#include "iot_errno.h"


#define AHT20_I2C_IDX 1
#define AHT20_STARTUP_TIME     20*1000//上电启动时间（20ms）
#define AHT20_CALIBRATION_TIME 40*1000//校准时间（40ms）
#define AHT20_MEASURE_TIME     75*1000//测量时间（75ms）
#define AHT20_DEVICE_ADDR   0x38//AHT20数字温湿度传感器的从机地址
#define AHT20_CMD_CALIBRATION       0xBE//校准命令
#define AHT20_CMD_CALIBRATION_ARG0  0x08
#define AHT20_CMD_CALIBRATION_ARG1  0x00

/**
 * 传感器在采集时需要时间,主机发出测量指令（0xAC）后，延时75毫秒以上再读取转换后的数据并判断返回的状态位是否正常。
 * 若状态比特位[Bit7]为0代表数据可正常读取，为1时传感器为忙状态，主机需要等待数据处理完成。
 **/
#define AHT20_CMD_TRIGGER       0xAC// 触发测量命令
#define AHT20_CMD_TRIGGER_ARG0  0x33
#define AHT20_CMD_TRIGGER_ARG1  0x00

/** 用于在无需关闭和再次打开电源的情况下，重新启动传感器系统，软复位所需时间不超过20 毫秒*/
#define AHT20_CMD_RESET      0xBA //软复位命令
#define AHT20_CMD_STATUS     0x71// 获取状态命令


/**
 * STATUS 命令回复：
 * 1. 初始化后触发测量之前，STATUS 只回复 1B 状态值；
 * 2. 触发测量之后，STATUS 回复6B： 1B 状态值 + 2B 湿度 + 4b湿度 + 4b温度 + 2B 温度
 *      RH = Srh / 2^20 * 100%
 *      T  = St  / 2^20 * 200 - 50
 **/
#define AHT20_STATUS_BUSY_SHIFT 7

#define AHT20_STATUS_BUSY_MASK  (0x1<<AHT20_STATUS_BUSY_SHIFT)

#define AHT20_STATUS_BUSY(status) ((status & AHT20_STATUS_BUSY_MASK) >> AHT20_STATUS_BUSY_SHIFT)

#define AHT20_STATUS_MODE_SHIFT 5

#define AHT20_STATUS_MODE_MASK  (0x3<<AHT20_STATUS_MODE_SHIFT)

#define AHT20_STATUS_MODE(status) ((status & AHT20_STATUS_MODE_MASK) >> AHT20_STATUS_MODE_SHIFT)

#define AHT20_STATUS_CALI_SHIFT 3

#define AHT20_STATUS_CALI_MASK  (0x1<<AHT20_STATUS_CALI_SHIFT)

#define AHT20_STATUS_CALI(status) ((status & AHT20_STATUS_CALI_MASK) >> AHT20_STATUS_CALI_SHIFT)

#define AHT20_STATUS_RESPONSE_MAX 6

#define AHT20_RESLUTION            (1<<20)  // 2^20


#define AHT20_MAX_RETRY 10


// AHT20读函数
static uint32_t AHT20_Read(uint8_t *buffer, uint32_t buffLen)
{
    uint32_t retval = IoTI2cRead(AHT20_I2C_IDX,     
                                 AHT20_DEVICE_ADDR,  
                                 buffer,          
                                 buffLen);         
    if (retval != IOT_SUCCESS)                   
    {
        printf("I2cRead() failed, %0X!\n", retval);
        return retval;
    }
    return IOT_SUCCESS;
}

// AHT20写函数
static uint32_t AHT20_Write(uint8_t *buffer, uint32_t buffLen)
{
    // 向I2C设备中写入数据
    uint32_t retval = IoTI2cWrite(AHT20_I2C_IDX,    
                                  AHT20_DEVICE_ADDR, 
                                  buffer,        
                                  buffLen);         
    if (retval != IOT_SUCCESS)                   
    {
        printf("I2cWrite() failed, %0X!\n", retval);
        return retval;
    }
    return IOT_SUCCESS;
}


// 发送获取状态命令
static uint32_t AHT20_StatusCommand(void)
{
    uint8_t statusCmd[] = { AHT20_CMD_STATUS };
    return AHT20_Write(statusCmd, sizeof(statusCmd));
}

// 发送软复位命令
static uint32_t AHT20_ResetCommand(void)
{

    uint8_t resetCmd[] = {AHT20_CMD_RESET};
    return AHT20_Write(resetCmd, sizeof(resetCmd));
}

// 发送初始化校准命令
static uint32_t AHT20_CalibrateCommand(void)
{
    uint8_t clibrateCmd[] = {AHT20_CMD_CALIBRATION, AHT20_CMD_CALIBRATION_ARG0, AHT20_CMD_CALIBRATION_ARG1};
    return AHT20_Write(clibrateCmd, sizeof(clibrateCmd));
}


/** 读取温湿度值之前， 首先要看状态字的校准使能位Bit[3]是否为 1(通过发送0x71可以获取一个字节的状态字)，
如果不为1，要发送0xBE命令(初始化)，此命令参数有两个字节， 第一个字节为0x08，第二个字节为0x00。**/
uint32_t AHT20_Calibrate(void)
{
    uint32_t retval = 0;
    uint8_t buffer[AHT20_STATUS_RESPONSE_MAX] = { 0 };
    memset(&buffer, 0x0, sizeof(buffer));
    retval = AHT20_StatusCommand();
    if (retval != IOT_SUCCESS) {
        return retval;
    }

    retval = AHT20_Read(buffer, sizeof(buffer));
    if (retval != IOT_SUCCESS) {
        return retval;
    }

    if (AHT20_STATUS_BUSY(buffer[0]) || !AHT20_STATUS_CALI(buffer[0])) {
        retval = AHT20_ResetCommand();
        if (retval != IOT_SUCCESS) {
            return retval;
        }
        usleep(AHT20_STARTUP_TIME);
        retval = AHT20_CalibrateCommand();
        usleep(AHT20_CALIBRATION_TIME);
        return retval;
    }
    return IOT_SUCCESS;
}

//发送触发测量命令，开始测量
uint32_t AHT20_StartMeasure(void)
{
    uint8_t triggerCmd[] = {AHT20_CMD_TRIGGER, AHT20_CMD_TRIGGER_ARG0, AHT20_CMD_TRIGGER_ARG1};
    return AHT20_Write(triggerCmd, sizeof(triggerCmd));
}

// 接收测量结果，拼接转换为标准值
uint32_t AHT20_GetMeasureResult(float* temp, float* humi)
{
    uint32_t retval = 0, i = 0;
    if (temp == NULL || humi == NULL) {
        return IOT_FAILURE;
    }

    uint8_t buffer[AHT20_STATUS_RESPONSE_MAX] = { 0 };

    memset(&buffer, 0x0, sizeof(buffer));

    retval = AHT20_Read(buffer, sizeof(buffer));
    if (retval != IOT_SUCCESS) {
        return retval;
    }

    for (i = 0; AHT20_STATUS_BUSY(buffer[0]) && i < AHT20_MAX_RETRY; i++) {
        // printf("AHT20 device busy, retry %d/%d!\r\n", i, AHT20_MAX_RETRY);
        usleep(AHT20_MEASURE_TIME);
        retval = AHT20_Read(buffer, sizeof(buffer));
        if (retval != IOT_SUCCESS) {
            return retval;
        }
    }

    if (i >= AHT20_MAX_RETRY) {
        printf("AHT20 device always busy!\r\n");
        return IOT_FAILURE;
    }

    // 计算湿度值
    uint32_t humiRaw = buffer[1];                           // 字节2
    humiRaw = (humiRaw << 8) | buffer[2];                   // 字节3
    humiRaw = (humiRaw << 4) | ((buffer[3] & 0xF0) >> 4);   // 字节4的前四位
    *humi = humiRaw / (float)AHT20_RESLUTION * 100;         // 根据公式计算湿度值

    // 计算温度值
    uint32_t tempRaw = buffer[3] & 0x0F;                    // 字节4的后四位
    tempRaw = (tempRaw << 8) | buffer[4];                   // 字节5
    tempRaw = (tempRaw << 8) | buffer[5];                   // 字节6
    *temp = tempRaw / (float)AHT20_RESLUTION * 200 - 50;    // 根据公式计算温度值
    // printf("humi = %05X, %f, temp= %05X, %f\r\n", humiRaw, *humi, tempRaw, *temp);
    return IOT_SUCCESS;
}
