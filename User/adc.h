#ifndef _ADC_H
#define _ADC_H

#include <stdio.h>
#include <include.h>
#include "pwm.h"
#include <string.h>

enum
{
    ADC_SEL_PIN_NONE = 0,
    ADC_SEL_PIN_GET_TEMP = 0x01, // ������������һ��������ADC
    ADC_SEL_PIN_GET_VOL = 0x02,  // ����9��������ADC
};

extern volatile u16 adc_val_pin_9; // ���9�Ųɼ�����adֵ

// ��ȡһ��adc�ɼ�+�˲����ֵ
u16 adc_get_val(void);

void adc_pin_config(void);      // adc��ص��������ã�������ɺ󣬻�δ��ʹ��adc
void adc_sel_pin(const u8 pin); // �л�adc�ɼ������ţ������ú�adc
// void adc_single_getval(void);   // adc���һ��ת��

u32 get_voltage_from_pin(void); // �������ϲɼ��˲���ĵ�ѹֵ

void adc_update_pin_9_adc_val(void);

// void adc_scan(void);
void temperature_scan(void);
void set_duty(void);

#endif