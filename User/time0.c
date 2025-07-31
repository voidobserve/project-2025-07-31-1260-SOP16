#include "time0.h"

u8 ms_cnt = 0;
volatile bit tmr0_flag = 0;


static volatile u8 cnt_during_power_on = 0; // 开机缓启动，调节pwm占空比时，使用到的计数值
volatile bit flag_time_comes_during_power_on = 0; // 标志位，开机缓启动期间，调节时间到来

/**
 * @brief 配置定时器TMR0，定时器默认关闭
 */
void tmr0_config(void)
{
    __EnableIRQ(TMR0_IRQn); // 使能timer0中断
    IE_EA = 1;              // 使能总中断

#define PEROID_VAL (SYSCLK / 128 / 1000 - 1) // 周期值=系统时钟/分频/频率 - 1
    // 设置timer0的计数功能，配置一个频率为1kHz的中断
    TMR_ALLCON = TMR0_CNT_CLR(0x1);                        // 清除计数值
    TMR0_PRH = TMR_PERIOD_VAL_H((PEROID_VAL >> 8) & 0xFF); // 周期值
    TMR0_PRL = TMR_PERIOD_VAL_L((PEROID_VAL >> 0) & 0xFF);
    TMR0_CONH = TMR_PRD_PND(0x1) | TMR_PRD_IRQ_EN(0x1);                          // 计数等于周期时允许发生中断
    TMR0_CONL = TMR_SOURCE_SEL(0x7) | TMR_PRESCALE_SEL(0x7) | TMR_MODE_SEL(0x1); // 选择系统时钟，128分频，计数模式
}

/**
 * @brief 开启定时器TMR0，开始计时
 */
void tmr0_enable(void)
{
    // 重新给TMR0配置时钟
    TMR0_CONL &= ~(TMR_SOURCE_SEL(0x07)); // 清除定时器的时钟源配置寄存器
    TMR0_CONL |= TMR_SOURCE_SEL(0x06);    // 配置定时器的时钟源，使用系统时钟（约21MHz）

    __EnableIRQ(TMR0_IRQn); // 使能中断
    IE_EA = 1;              // 打开总中断
}

/**
 * @brief 关闭定时器0，清空计数值
 */
void tmr0_disable(void)
{
    // 不给定时器提供时钟，让它停止计数
    TMR0_CONL &= ~(TMR_SOURCE_SEL(0x07)); // 清除定时器的时钟源配置寄存器
    TMR0_CONL |= TMR_SOURCE_SEL(0x05);    // 配置定时器的时钟源，不用任何时钟

    // 清除定时器的计数值
    TMR0_CNTL = 0;
    TMR0_CNTH = 0;

    __DisableIRQ(TMR0_IRQn); // 关闭中断（不使能中断）
}

// extern void fun(void);
// 定时器TMR0中断服务函数
void TIMR0_IRQHandler(void) interrupt TMR0_IRQn
{
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(TMR0_IRQn);

    // ---------------- 用户函数处理 -------------------

    // 周期中断
    if (TMR0_CONH & TMR_PRD_PND(0x1))
    {
        TMR0_CONH |= TMR_PRD_PND(0x1); // 清除pending

        ms_cnt++;
        cnt_during_power_on++;

        if (ms_cnt >= 25)
        {
            ms_cnt = 0;
            tmr0_flag = 1;
        }

        // if (cnt_during_power_on >= 13) // 13ms
        if (cnt_during_power_on >= 1) // 1 ms
        {
            cnt_during_power_on = 0;
            
            time_accumulate += 1.0; // 每次进入，表示经过了1ms
            flag_time_comes_during_power_on = 1;
        }
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(TMR0_IRQn);
}
