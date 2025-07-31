/**
 ******************************************************************************
 * @file    main.c
 * @author  HUGE-IC Application Team
 * @version V1.0.0
 * @date    02-09-2022
 * @brief   Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2021 HUGE-IC</center></h2>
 *
 * 版权说明后续补上
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "my_config.h"
#include "include.h"
#include <math.h>
#include <stdio.h>

// float step = 70;
// float mi; // 幂
// float rus; // 10的幂次方
// float r_ms = 0;
// #define USER_BAUD (115200UL)
// #define USER_UART_BAUD ((SYSCLK - USER_BAUD) / (USER_BAUD))

volatile float32 time_accumulate = 0.0;
    

#if USE_MY_DEBUG // 打印串口配置

#define UART0_BAUD 115200
#define USER_UART0_BAUD ((SYSCLK - UART0_BAUD) / (UART0_BAUD))
// 重写puchar()函数
char putchar(char c)
{
    while (!(UART0_STA & UART_TX_DONE(0x01)))
        ;
    UART0_DATA = c;
    return c;
}

void my_debug_config(void)
{
    // 作为发送引脚
    P1_MD0 &= (GPIO_P13_MODE_SEL(0x3));
    P1_MD0 |= GPIO_P13_MODE_SEL(0x1);            // 配置为输出模式
    FOUT_S13 |= GPIO_FOUT_UART0_TX;              // 配置为UART0_TX
    UART0_BAUD1 = (USER_UART0_BAUD >> 8) & 0xFF; // 配置波特率高八位
    UART0_BAUD0 = USER_UART0_BAUD & 0xFF;        // 配置波特率低八位
    UART0_CON0 = UART_STOP_BIT(0x0) |
                 UART_EN(0x1); // 8bit数据，1bit停止位
}
#endif // USE_MY_DEBUG // 打印串口配置

// 开机缓启动，调节占空比：
void adjust_pwm_duty_when_power_on(void)
{
#if 0
    if (c_duty < 6000)
    {
        /* step初始值为70 */
        mi = (step - 1) / (253 / 3) - 1;
        step += 0.5;
        /*
            C 库函数 double pow(double x, double y) 返回 x 的 y 次幂
            原本是 c_duty = pow(5, mi) * 60，实际测试开机8s左右
        */
        c_duty = pow(5, mi) * 60;
    }

    if (c_duty >= 6000)
    {
        c_duty = 6000;
    }

    // printf("c_duty %d\n",c_duty);
    printf("%d\n", c_duty);

    // delay_ms(16); // 每16ms调整一次PWM的脉冲宽度 ---- 校验码A488对应的时间
    // delay_ms(11); // 16 * 0.666 约为10.656   ---- 校验码B5E3对应的时间
#endif



    if (c_duty < PWM_DUTY_50_PERCENT)
    {
// 持续时间 40s，每13.3ms变化1
#define STEP_1 ((float32)40000 / PWM_DUTY_50_PERCENT)
        if (time_accumulate >= STEP_1)
        {
            time_accumulate -= STEP_1;
            c_duty++;
        }
    }
    else if (c_duty < PWM_DUTY_100_PERCENT)
    {
// 持续时间20s，每 6.66ms变化1
#define STEP_2 ((float32)20000 / (PWM_DUTY_100_PERCENT - PWM_DUTY_50_PERCENT))
        if (time_accumulate >= STEP_2)
        {
            time_accumulate -= STEP_2;
            c_duty++;
        }
    }
    else
    {
        c_duty = PWM_DUTY_100_PERCENT;
    }

    // printf("%u\n", c_duty);
}

void main(void)
{
    // 看门狗默认打开, 复位时间2s
    WDT_KEY = WDT_KEY_VAL(0xDD); //  关闭看门狗 (如需配置看门狗请查看“WDT\WDT_Reset”示例)

    system_init();

#if USE_MY_DEBUG // 打印串口配置
    // 初始化打印
    my_debug_config();

    // 输出模式：
    // P1_MD0 &= (GPIO_P13_MODE_SEL(0x3));
    // P1_MD0 |= GPIO_P13_MODE_SEL(0x1); // 配置为输出模式
    // FOUT_S13 = GPIO_FOUT_AF_FUNC;     // 选择AF功能输出
#endif // 打印串口配置

    // 过压保护  16脚-----P14
    //		P1_MD1   &= ~GPIO_P14_MODE_SEL(0x03);
    //		P1_MD1   |=  GPIO_P14_MODE_SEL(0x01);
    //		FOUT_S14  =  GPIO_FOUT_AF_FUNC;
    ///////////////////////////////////////////

#if 1
    adc_pin_config(); // 配置使用到adc的引脚
    // adc_sel_pin(ADC_SEL_PIN_GET_TEMP);
    tmr0_config(); // 配置定时器，默认关闭
    pwm_init();    // 配置pwm输出的引脚
    tmr1_config();

    timer2_config();
#endif

    adc_sel_pin(ADC_SEL_PIN_GET_VOL); // 切换到9脚，准备检测9脚的电压

// ===================================================================
#if 1        // 开机缓慢启动（PWM信号变化平缓）
    P14 = 0; // 16脚先输出低电平
    c_duty = 0;
    while (c_duty < 6000)
    {
        /*
            原本计划在开机缓启动时检测发送机供电是否稳定，但是实际测试发现容易误检，导致提前退出
            现在只有框架，哪怕检测到发送机供电不稳定，也不会有任何操作
        */
        adc_update_pin_9_adc_val(); // 采集并更新9脚的ad值

#if USE_MY_DEBUG // 直接打印0，防止在串口+图像上看到错位
                 // printf(",b=0,");  // 防止错位

#endif

        if (flag_is_pwm_sub_time_comes) // pwm递减时间到来
        {
            flag_is_pwm_sub_time_comes = 0;

            if (adc_val_pin_9 >= ADC_VAL_WHEN_UNSTABLE) // 只要有一次跳动，退出开机缓启动
            {
                // if (c_duty >= PWM_DUTY_50_PERCENT)
                if (c_duty >= PWM_DUTY_100_PERCENT)
                {
                    // adjust_duty = c_duty;
                    break;
                }
            }
        }

        // if (flag_time_comes_during_power_on) // 如果调节时间到来 -- 13ms
        if (flag_time_comes_during_power_on) // 如果调节时间到来 -- 1 ms
        {
            flag_time_comes_during_power_on = 0;
            adjust_pwm_duty_when_power_on();
        }

        set_pwm_duty(); // 将 c_duty 写入pwm对应的寄存器

#if USE_MY_DEBUG
        // printf("power_on_duty %u\n", c_duty);
#endif //  USE_MY_DEBUG
    }
#endif
    // ===================================================================

    while (1)
    {
#if 1
        adc_update_pin_9_adc_val(); // 采集并更新9脚的ad值
        temperature_scan();         // 检测热敏电阻一端的电压值

        set_duty();                       // 设定到要调节到的脉宽
        according_pin9_to_adjust_pin16(); // 根据9脚的电压来设定16脚的电平

#if USE_MY_DEBUG
        // printf("adjust_duty %u\n", adjust_duty);
        // printf(",b=%u,", adjust_duty);
#endif //  USE_MY_DEBUG

#endif

        // P13 = ~P13;
    }
}

/**
 * @}
 */

/*************************** (C) COPYRIGHT 2022 HUGE-IC ***** END OF FILE *****/
