#include "pwm.h"
#include "time0.h"

volatile u16 c_duty = 0;         // 当前设置的占空比
volatile u16 adjust_duty = 6000; // 最终要调节成的占空比
// bit jump_flag = 0;
// bit max_flag = 0; // 最大占空比的标志位

extern volatile bit flag_is_pin_9_vol_bounce; // 标志位，9脚电压是否发生了跳动（是否因为发动机功率不稳定导致跳动）

void pwm_init(void)
{
    STMR_CNTCLR |= STMR_0_CNT_CLR(0x1);
#define STMR0_PEROID_VAL (SYSCLK / 8000 - 1)
    STMR0_PSC = STMR_PRESCALE_VAL(0x07);
    STMR0_PRH = STMR_PRD_VAL_H((STMR0_PEROID_VAL >> 8) & 0xFF);
    STMR0_PRL = STMR_PRD_VAL_L((STMR0_PEROID_VAL >> 0) & 0xFF);
    STMR0_CMPAH = STMR_CMPA_VAL_H(((0) >> 8) & 0xFF); // 比较值
    STMR0_CMPAL = STMR_CMPA_VAL_L(((0) >> 0) & 0xFF); // 比较值
    STMR_PWMVALA |= STMR_0_PWMVALA(0x1);

    STMR_CNTMD |= STMR_0_CNT_MODE(0x1); // 连续计数模式
    STMR_LOADEN |= STMR_0_LOAD_EN(0x1); // 自动装载使能
    STMR_CNTCLR |= STMR_0_CNT_CLR(0x1); //
    STMR_CNTEN |= STMR_0_CNT_EN(0x1);   // 使能
    STMR_PWMEN |= STMR_0_PWM_EN(0x1);   // PWM输出使能
    P1_MD1 &= ~GPIO_P16_MODE_SEL(0x03);
    P1_MD1 |= GPIO_P16_MODE_SEL(0x01);
    P1_MD1 &= ~GPIO_P14_MODE_SEL(0x03);
    P1_MD1 |= GPIO_P14_MODE_SEL(0x01);
    FOUT_S14 = GPIO_FOUT_AF_FUNC;      // AF功能输出
    FOUT_S16 = GPIO_FOUT_STMR0_PWMOUT; // stmr0_pwmout
}

// 14脚的PWM调节
void set_pwm_duty(void)
{
    STMR0_CMPAH = STMR_CMPA_VAL_H(((c_duty) >> 8) & 0xFF); // 比较值
    STMR0_CMPAL = STMR_CMPA_VAL_L(((c_duty) >> 0) & 0xFF); // 比较值
    STMR_LOADEN |= STMR_0_LOAD_EN(0x1);                    // 自动装载使能
}

static u16 t_count = 0;
static u16 t_adc_max = 0;
static u16 t_adc_min = 4096;
static u8 over_drive_status = 0;
#define OVER_DRIVE_RESTART_TIME (30)

static volatile u16 filter_buff_2[270] = {0};
static volatile u16 buff_index_2 = 0;

// 电源电压低于170V-AC,启动低压保护，电源电压高于170V-AC，关闭低压保护
// 温度正常，才会进入到这里
void according_pin9_to_adjust_pwm(void)
{
#define ADC_DEAD_ZONE_NEAR_170VAC (30) // 170VAC附近的ad值死区
    static volatile u16 filter_buff[32] = {
        0xFFFF,
    };
    static volatile u8 buff_index = 0;
    static volatile u8 flag_is_sub_power = 0;  // 标志位，是否要连续减功率
    static volatile u8 flag_is_sub_power2 = 0; // 标志位，是否要连续减功率
    static volatile bit flag_is_add_power = 0; // 标志位，是否要连续增功率

    volatile u32 adc_pin_9_avg = 0; // 存放平均值

    if (filter_buff[0] == 0xFFFF) // 如果是第一次检测，让数组内所有元素都变为第一次采集的数据，方便快速作出变化
    {
        u16 i = 0;
        for (; i < ARRAY_SIZE(filter_buff); i++)
        {
            filter_buff[i] = adc_val_pin_9;
        }
        for (i = 0; i < 270; i++)
        {
            filter_buff_2[i] = adc_val_pin_9;
        }
    }
    else
    {
        u16 temp = filter_buff[buff_index];
        temp += adc_val_pin_9;
        temp >>= 1;
        filter_buff[buff_index] = temp;
        buff_index++;
        if (buff_index >= ARRAY_SIZE(filter_buff))
        {
            buff_index = 0;
        }
    }

    { // 取出数组内的数据，计算平均值
        u16 i = 0;
        for (; i < ARRAY_SIZE(filter_buff); i++)
        {
            adc_pin_9_avg += filter_buff[i];
        }

        // adc_pin_9_avg /= ARRAY_SIZE(filter_buff);
        adc_pin_9_avg >>= 5;
    } // 取出数组内的数据，计算平均值

    filter_buff_2[buff_index_2] = adc_pin_9_avg;
    buff_index_2++;
    if (buff_index_2 >= 270)
    {
        buff_index_2 = 0;
    }

#if USE_MY_DEBUG
    // printf(",b=%lu,", adc_pin_9_avg);
#endif

    // if (adc_pin_9_avg > t_adc_max)
    //     t_adc_max = adc_pin_9_avg;
    // if (adc_pin_9_avg < t_adc_min)
    //     t_adc_min = adc_pin_9_avg;
    // if (t_count < 270)
    // {
    //     t_count++;
    //     if (t_count == 270)
    //     {
    //         if ((t_adc_max - t_adc_min) > 80 && t_adc_max > 2228)
    //         { // 电压波动
    //             over_drive_status = OVER_DRIVE_RESTART_TIME;
    //         }
    //         else
    //         {
    //             if (over_drive_status)
    //                 over_drive_status--;
    //         }
    //         t_count = 0;
    //         t_adc_max = 0;
    //         t_adc_min = 4096;
    //     }
    // }
    {
        u16 i = 0;
        t_adc_max = 0;
        t_adc_min = 4096;
        for (; i < 270; i++)
        {
            if (filter_buff_2[i] > t_adc_max)
                t_adc_max = filter_buff_2[i];
            if (filter_buff_2[i] < t_adc_min)
                t_adc_min = filter_buff_2[i];
            if ((t_adc_max - t_adc_min) > 80)
            { // 电压波动
                over_drive_status = OVER_DRIVE_RESTART_TIME;
            }
            else
            {
                if (over_drive_status)
                    over_drive_status--;
            }
        }

        // {
        //     static u8 cnt = 0;
        //     cnt++;
        //     if (cnt >= 100)
        //     {
        //         cnt = 0;
        //         printf("__LINE__ %u\n", __LINE__);
        //     }
        // }
    }

    if (adc_pin_9_avg >= (1645 /*1475*/ + ADC_DEAD_ZONE_NEAR_170VAC) || (flag_is_add_power && adc_pin_9_avg > (1645 /*1475*/ + ADC_DEAD_ZONE_NEAR_170VAC))) // 大于 170VAC
    {
        // 大于170VAC，恢复100%占空比，但是优先级比 "9脚电压检测到发送机功率不稳定，进而降功率" 低
        flag_is_sub_power = 0;
        flag_is_sub_power2 = 0;
        flag_is_add_power = 1;
#if 0
        // 判断是否变化PWM
        if (adc_pin_9_avg > ADC_VAL_WHEN_UNSTABLE) // 9脚电压超过不稳定阈值对应的电压
        {
            if (flag_is_pwm_sub_time_comes) // pwm占空比递减时间到来
            {
                flag_is_pwm_sub_time_comes = 0;
                // 过载 pwm--
                // if (adjust_duty > PWM_DUTY_50_PERCENT)
                if (adjust_duty > PWM_DUTY_30_PERCENT)
                {
                    adjust_duty -= 1;
                }
                else
                {
                    // adjust_duty = PWM_DUTY_50_PERCENT;
                    adjust_duty = PWM_DUTY_30_PERCENT;
                }
            }
        }
        else if (adc_pin_9_avg < (ADC_VAL_WHEN_UNSTABLE - 50))
        {
            // 未满载 pwm++
            if (flag_is_pwm_add_time_comes) // pwm占空比递增时间到来
            {
                flag_is_pwm_add_time_comes = 0;
                if (adjust_duty < 6000)
                {
                    adjust_duty++;
                }
            }
        }
#else
        if (over_drive_status == OVER_DRIVE_RESTART_TIME) // 9脚电压超过不稳定阈值对应的电压
        {
            over_drive_status -= 1;
            if (adjust_duty > PWM_DUTY_50_PERCENT)
                // adjust_duty -= 300;
                adjust_duty -= 1;
            if (adjust_duty < PWM_DUTY_50_PERCENT)
                adjust_duty = PWM_DUTY_50_PERCENT;
        }
        else if (over_drive_status == 0)
        {
            // 未满载 pwm++
            if (flag_is_pwm_add_time_comes) // pwm占空比递增时间到来
            {
                flag_is_pwm_add_time_comes = 0;
                if (adjust_duty < 6000)
                {
                    adjust_duty++;
                }
            }
        }
#endif
    }
    else if (adc_pin_9_avg > (1475) && (adc_pin_9_avg <= (1645 /*1475*/) || flag_is_sub_power == 4)) // 小于 170VAC
    {
        // 锁定最大的占空比为50%，并且给相应标志位置一
        if (flag_is_pwm_sub_time_comes) // pwm占空比递减时间到来
        {
            flag_is_pwm_sub_time_comes = 0;
            // if (flag_is_sub_power == 0)
            //     flag_is_sub_power = 1;
            // else if (flag_is_sub_power == 1)
            //     flag_is_sub_power = 2;
            if (flag_is_sub_power < 4)
                flag_is_sub_power++;

            flag_is_sub_power2 = 0;
            flag_is_add_power = 0;

            if (adjust_duty > PWM_DUTY_50_PERCENT)
            // if (adjust_duty > PWM_DUTY_30_PERCENT)
            {
                adjust_duty -= 2;
            }
            else if (adjust_duty < PWM_DUTY_50_PERCENT)
            {
                adjust_duty++;
            }
            else
            {
                adjust_duty = PWM_DUTY_50_PERCENT;
                // adjust_duty = PWM_DUTY_30_PERCENT;
            }
        }
    }
    else if (adc_pin_9_avg <= (1475) || (flag_is_sub_power2)) // 小于 170VAC
    {
        // 锁定最大的占空比为50%，并且给相应标志位置一
        if (flag_is_pwm_sub_time_comes) // pwm占空比递减时间到来
        {
            flag_is_pwm_sub_time_comes = 0;
            // if (flag_is_sub_power2 < 4)
            //     flag_is_sub_power2++;

            flag_is_sub_power2 = 1;
            flag_is_sub_power = 0;
            flag_is_add_power = 0;

            // if (adjust_duty > PWM_DUTY_50_PERCENT)
            if (adjust_duty > PWM_DUTY_30_PERCENT)
            {
                adjust_duty -= 2;
            }
            else
            {
                // adjust_duty = PWM_DUTY_50_PERCENT;
                adjust_duty = PWM_DUTY_30_PERCENT;
            }
        }
    }
}

// 根据9脚的电压来设定16脚的电平（过压保护）
void according_pin9_to_adjust_pin16(void)
{
    // 当9脚电压高于 3.6 V时，16脚输出1KHz 高电平,用于控制Q2的导通（用于关机）。
    // if (adc_val_pin_9 >= 3511)
    // {
    //     P14 = 1;
    // }
    // else if (adc_val_pin_9 <= 3511 - 40)
    {
        P14 = 0;
    }
}

// 缓慢调节占空比（缓慢提升和缓慢下降）
void Adaptive_Duty(void)
{
#if 0  // 缓慢调节占空比的版本：
    if (c_duty > adjust_duty)
    {
        c_duty--;
    }
    if (c_duty < adjust_duty)
    {
        c_duty++;
    }
    set_pwm_duty(); // 函数内部会将 c_duty 的值代入相关寄存器中

    if (c_duty >= 5800)
    {

        // delay_ms(15); // 时间还需要测试调整一下
        delay_ms(7);
    }
    else
    {
        // delay_ms(5);
        delay_ms(3);
    }
#endif // 缓慢调节占空比的版本

#if 1 // 立即调节占空比的版本：

    c_duty = adjust_duty;
    set_pwm_duty(); // 函数内部会将 c_duty 的值代入相关寄存器中

#if USE_MY_DEBUG
    // printf(",c=%u,", c_duty);
#endif

#endif // 立即调节占空比的版本
}

// void pwm_adjust(void)
// {

// }
