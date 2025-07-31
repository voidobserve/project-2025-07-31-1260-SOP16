#ifndef _ADC_H
#define _ADC_H

#include <stdio.h>
#include <include.h>
#include "pwm.h"
#include <string.h>

enum
{
    ADC_SEL_PIN_NONE = 0,
    ADC_SEL_PIN_GET_TEMP = 0x01, // 根据热敏电阻一端来配置ADC
    ADC_SEL_PIN_GET_VOL = 0x02,  // 根据9脚来配置ADC
};

extern volatile u16 adc_val_pin_9; // 存放9脚采集到的ad值

// 获取一次adc采集+滤波后的值
u16 adc_get_val(void);

void adc_pin_config(void);      // adc相关的引脚配置，调用完成后，还未能使用adc
void adc_sel_pin(const u8 pin); // 切换adc采集的引脚，并配置好adc
// void adc_single_getval(void);   // adc完成一次转换

u32 get_voltage_from_pin(void); // 从引脚上采集滤波后的电压值

void adc_update_pin_9_adc_val(void);

// void adc_scan(void);
void temperature_scan(void);
void set_duty(void);

#endif