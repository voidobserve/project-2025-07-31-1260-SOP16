#ifndef _TIME0_H
#define _TIME0_H

#include "include.h"

#include <stdio.h>
#include "adc.h"

extern volatile bit tmr0_flag;
extern volatile bit flag_time_comes_during_power_on ; // ��־λ�������������ڼ䣬����ʱ�䵽��

void tmr0_config(void);
void tmr0_enable(void);  // ������ʱ������ʼ��ʱ
void tmr0_disable(void); // �رն�ʱ������ռ���ֵ


#endif
