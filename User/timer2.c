#include "timer2.h"
#define TIMER2_PEROID_VAL (SYSCLK / 128 / 10000 - 1) // 周期值=系统时钟/分频/频率 - 1

// volatile u16 tmr2_cnt = 0;

static volatile u16 pwm_duty_add_cnt; // 用于控制pwm增加的时间计数
static volatile u16 pwm_duty_sub_cnt; // 用于控制pwm递减的时间计数

volatile bit flag_is_pwm_add_time_comes = 0; // 标志位，pwm占空比递增时间到来
volatile bit flag_is_pwm_sub_time_comes = 0; // 标志位，pwm占空比递减时间到来

void timer2_config(void)
{
    __EnableIRQ(TMR2_IRQn); // 使能timer2中断
    IE_EA = 1;              // 使能总中断

    // 设置timer2的计数功能，配置一个频率为1kHz的中断
    TMR_ALLCON = TMR2_CNT_CLR(0x1);                               // 清除计数值
    TMR2_PRH = TMR_PERIOD_VAL_H((TIMER2_PEROID_VAL >> 8) & 0xFF); // 周期值
    TMR2_PRL = TMR_PERIOD_VAL_L((TIMER2_PEROID_VAL >> 0) & 0xFF);
    TMR2_CONH = TMR_PRD_PND(0x1) | TMR_PRD_IRQ_EN(0x1);                          // 计数等于周期时允许发生中断
    TMR2_CONL = TMR_SOURCE_SEL(0x7) | TMR_PRESCALE_SEL(0x7) | TMR_MODE_SEL(0x1); // 选择系统时钟，128分频，计数模式
}

// 定时器 中断服务函数
void TIMR2_IRQHandler(void) interrupt TMR2_IRQn
{
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(TMR2_IRQn);

    // ---------------- 用户函数处理 -------------------

    // 周期中断
    if (TMR2_CONH & TMR_PRD_PND(0x1)) // 约100us触发一次中断
    {
        TMR2_CONH |= TMR_PRD_PND(0x1); // 清除pending

        // tmr2_cnt++;
        // P13 = ~P13; // 测试中断触发周期

        pwm_duty_add_cnt++;
        pwm_duty_sub_cnt++;

        if (pwm_duty_sub_cnt >= 13) // 1300us，1.3ms
        // if (pwm_duty_sub_cnt >= 50) 
        {
            pwm_duty_sub_cnt = 0;
            flag_is_pwm_sub_time_comes = 1;
        }

        // if (pwm_duty_add_cnt >= 133) // 13300us, 13.3ms
        if (pwm_duty_add_cnt >= 13) // 
        {
            pwm_duty_add_cnt = 0;
            flag_is_pwm_add_time_comes = 1;
        }
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(TMR2_IRQn);
}