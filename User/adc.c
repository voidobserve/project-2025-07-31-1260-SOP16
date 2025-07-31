#include "adc.h"
#include "my_config.h"

// 存放温度状态的变量
volatile u8 temp_status = TEMP_NORMAL;

volatile bit flag_is_pin_9_vol_bounce = 0; // 标志位，9脚电压是否发生了跳动

// adc相关的引脚配置
void adc_pin_config(void)
{
    // P30--8脚配置为模拟输入模式
    P3_MD0 |= GPIO_P30_MODE_SEL(0x3);

    // P27--9脚配置为模拟输入模式
    P2_MD1 |= GPIO_P27_MODE_SEL(0x3);
}

// 切换adc采集的引脚，配置好adc
// 参数可以选择：
// ADC_SEL_PIN_GET_TEMP
// ADC_SEL_PIN_GET_VOL
void adc_sel_pin(const u8 adc_sel)
{
    // 切换采集引脚时，把之前采集到的ad值清空
    // adc0_val = 0;
    static u8 last_adc_sel = 0;
    if (last_adc_sel == adc_sel)
    {
        // 如果当前采集adc的引脚就是要配置的adc引脚，不用再继续配置，直接退出
        return;
    }

    last_adc_sel = adc_sel;

    switch (adc_sel)
    {
    case ADC_SEL_PIN_GET_TEMP: // 采集热敏电阻对应的电压的引脚（8脚）

        // ADC配置
        ADC_ACON1 &= ~(ADC_VREF_SEL(0x7) | ADC_EXREF_SEL(0) | ADC_INREF_SEL(0)); // 关闭外部参考电压
        ADC_ACON1 |= ADC_VREF_SEL(0x6) |                                         // 选择内部参考电压VCCA
                     ADC_TEN_SEL(0x3);                                           // 关闭测试信号
        ADC_ACON0 = ADC_CMP_EN(0x1) |                                            // 打开ADC中的CMP使能信号
                    ADC_BIAS_EN(0x1) |                                           // 打开ADC偏置电流能使信号
                    ADC_BIAS_SEL(0x1);                                           // 偏置电流：1x

        ADC_CHS0 = ADC_ANALOG_CHAN(0x18) | // 选则引脚对应的通道（0x18--P30）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        ADC_CFG0 |= ADC_CHAN0_EN(0x1) |    // 使能通道0转换
                    ADC_EN(0x1);           // 使能A/D转换
        break;

    case ADC_SEL_PIN_GET_VOL: // 检测回路电压的引脚（9脚）

        // ADC配置
        ADC_ACON1 &= ~(ADC_VREF_SEL(0x7) | ADC_EXREF_SEL(0x01)); // 关闭外部参考电压
        // ADC_ACON1 |= ADC_VREF_SEL(0x6) |                                         // 选择内部参考电压VCCA
        //              ADC_TEN_SEL(0x3);
        ADC_ACON1 |= ADC_VREF_SEL(0x5) |   // 选择内部参考电压 4.2V (用户手册说未校准)
                     ADC_TEN_SEL(0x3);     /* 关闭测试信号 */
        ADC_ACON0 = ADC_CMP_EN(0x1) |      // 打开ADC中的CMP使能信号
                    ADC_BIAS_EN(0x1) |     // 打开ADC偏置电流能使信号
                    ADC_BIAS_SEL(0x1);     // 偏置电流：1x
        ADC_CHS0 = ADC_ANALOG_CHAN(0x17) | // 选则引脚对应的通道（0x17--P27）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        ADC_CFG0 |= ADC_CHAN0_EN(0x1) |    // 使能通道0转换
                    ADC_EN(0x1);           // 使能A/D转换

        break;
    }

    delay_ms(1); // 等待ADC稳定
}

// adc完成一次转换
// 转换好的值放入全局变量 adc0_val 中
// 需要注意，这款芯片的adc不能频繁采集，需要延时一下再采集一次
// void adc_single_getval(void)
// {
//     ADC_CFG0 |= ADC_CHAN0_TRG(0x1); // 触发ADC0转换
//     while (!(ADC_STA & ADC_CHAN0_DONE(0x1)))
//         ;                                             // 等待转换完成
//     adc0_val = (ADC_DATAH0 << 4) | (ADC_DATAL0 >> 4); // 读取channel0的值
//     ADC_STA = ADC_CHAN0_DONE(0x1);                    // 清除ADC0转换完成标志位
// }

// 获取一次adc采集+滤波后的值
u16 adc_get_val(void)
{
    u8 i = 0; // adc采集次数的计数
    volatile u16 g_temp_value = 0;
    volatile u32 g_tmpbuff = 0;
    volatile u16 g_adcmax = 0;
    volatile u16 g_adcmin = 0xFFFF;

    // 采集20次，去掉前两次采样，再去掉一个最大值和一个最小值，再取平均值
    for (i = 0; i < 20; i++)
    {
        ADC_CFG0 |= ADC_CHAN0_TRG(0x1); // 触发ADC0转换
        while (!(ADC_STA & ADC_CHAN0_DONE(0x1)))
            ;                                                 // 等待转换完成
        g_temp_value = (ADC_DATAH0 << 4) | (ADC_DATAL0 >> 4); // 读取 channel0 的值
        ADC_STA = ADC_CHAN0_DONE(0x1);                        // 清除ADC0转换完成标志位

        if (i < 2)
            continue; // 丢弃前两次采样的
        if (g_temp_value > g_adcmax)
            g_adcmax = g_temp_value; // 最大
        if (g_temp_value < g_adcmin)
            g_adcmin = g_temp_value; // 最小

        g_tmpbuff += g_temp_value;
    }

    g_tmpbuff -= g_adcmax;           // 去掉一个最大
    g_tmpbuff -= g_adcmin;           // 去掉一个最小
    g_temp_value = (g_tmpbuff >> 4); // 除以16，取平均值

    return g_temp_value;
}

//
// volatile u8 flag_timer_filter = 0;
// volatile u32 flag_adc_filter = 0;
// // volatile u8 full_power_flag = 0;
// void fun(void)
// {
//     flag_adc_filter <<= 1;
//     if (flag_timer_filter)
//     {
//         flag_adc_filter |= 1;
//     }

//     if ((flag_adc_filter & 0xFFF) == 0xFFF)
//     {
//         // 降功率
//         flag_is_pin_9_vol_bounce = 1;
//     }
//     else
//     {
//         // flag_is_pin_9_vol_bounce = 0;
//     }

//     flag_timer_filter = 0;
// }

// 从引脚上采集滤波后的电压值,函数内部会将采集到的ad转换成对应的电压值
u32 get_voltage_from_pin(void)
{
    volatile u32 adc_aver_val = 0; // 存放adc滤波后的值
    // 采集热敏电阻的电压
    adc_aver_val = adc_get_val();

    // 4095（adc转换后，可能出现的最大的值） * 0.0012 == 4.914，约等于5V（VCC）
    return adc_aver_val * 12 / 10; // 假设是4095，4095 * 12/10 == 4915mV
}

// void adc_scan(void)
// 温度检测功能
void temperature_scan(void)
{
    volatile u32 voltage = 0; // 存放adc采集到的电压，单位：mV

    // 如果已经超过75摄氏度且超过30min，不用再检测8脚的电压，等待用户排查原因，再重启（重新上电）
    // if (TEMP_75_30MIN == temp_status)
    // 如果已经超过75摄氏度且超过5min，不用再检测8脚的电压，等待用户排查原因，再重启（重新上电）
    if (TEMP_75_5_MIN == temp_status)
    {
        return;
    }

    // adc0_val = 0;
    adc_sel_pin(ADC_SEL_PIN_GET_TEMP); // 先切换成热敏电阻对应的引脚的adc配置
    voltage = get_voltage_from_pin();  // 采集热敏电阻上的电压

#if USE_MY_DEBUG
    // printf("PIN-8 voltage: %lu mV\n", voltage);
#endif // USE_MY_DEBUG

    // 如果之前的温度为正常，检测是否超过75摄氏度（±5摄氏度）
    if (TEMP_NORMAL == temp_status && voltage < VOLTAGE_TEMP_75)
    {
        // 如果检测到温度大于75摄氏度（测得的电压值要小于75摄氏度对应的电压值）

        {
            // 检测10次，如果10次都小于这个电压值，才说明温度真的大于75摄氏度
            u8 i = 0;
            for (i = 0; i < 10; i++)
            {
                voltage = get_voltage_from_pin(); // 采集热敏电阻上的电压
                if (voltage > VOLTAGE_TEMP_75)
                {
                    // 只要有一次温度小于75摄氏度，就认为温度没有大于75摄氏度
                    temp_status = TEMP_NORMAL;
                    return;
                }
            }

            // 如果运行到这里，说明温度确实大于75摄氏度
#if USE_MY_DEBUG
// printf("温度超过了75摄氏度\n");
// printf("此时采集到的电压值：%lu mV", voltage);
#endif
            temp_status = TEMP_75; // 状态标志设置为超过75摄氏度
            return;                // 函数返回，让调节占空比的函数先进行调节
        }

        // static u8 flag_adc_filter = 0;
        // flag_adc_filter <<= 1;
        // if (voltage > VOLTAGE_TEMP_75) // 电压值大于75度对应的电压，说明温度小于75度
        // {
        //     flag_adc_filter = 0;
        // }
        // else
        // {
        //     flag_adc_filter |= 1;
        // }

        // if (flag_adc_filter == 0xFF)
        // {
        // }
    }
    else if (temp_status == TEMP_75)
    {
        // 如果之前的温度超过75摄氏度
        static bit tmr1_is_open = 0;

        if (0 == tmr1_is_open)
        {
            tmr1_is_open = 1;
            tmr1_cnt = 0;
            tmr1_enable(); // 打开定时器，开始记录是否大于75摄氏度且超过30min
        }

        // while (1) // 这个while循环会影响到9脚调节16脚电压的功能
        // {
#if 0 // 这里的代码在客户那边反而出现问题，超过90摄氏度且1个小时都没有将PWM降到25%，
      // 可能是用户那边的电压有跳变，导致这里清空了定时器计数
            if (voltage > VOLTAGE_TEMP_75)
            {
                // 只要有一次温度小于75摄氏度，就认为温度没有大于75摄氏度
                temp_status = TEMP_75; // 温度标记为超过75摄氏度，但是没有累计30min
                tmr1_disable();        // 关闭定时器
                tmr1_cnt = 0;          // 清空时间计数值
#if USE_MY_DEBUG
                // printf("在温度超过了75摄氏度时，检测到有一次温度没有超过75摄氏度\n");
                // printf("此时采集到的电压值：%lu mV\n", voltage);
#endif
                return;
            }
#endif
        // 如果超过75摄氏度并且过了30min，再检测温度是否超过75摄氏度
        // if (tmr1_cnt >= (u32)TMR1_CNT_30_MINUTES)
        // 如果超过75摄氏度并且过了5min，再检测温度是否超过75摄氏度
        if (tmr1_cnt >= (u32)TMR1_CNT_5_MINUTES)
        {
            u8 i = 0;
#if USE_MY_DEBUG
            // printf("温度超过了75摄氏度且超过了30min\n");
            // printf("此时采集到的电压值：%lu mV\n", voltage);
#endif

            for (i = 0; i < 10; i++)
            {
                voltage = get_voltage_from_pin(); // 采集热敏电阻上的电压
                if (voltage > VOLTAGE_TEMP_75)
                {
                    // 只要有一次温度小于75摄氏度，就认为温度没有大于75摄氏度
                    temp_status = TEMP_75;
                    return;
                }
            }

            // 如果运行到这里，说明上面连续、多次检测到的温度都大于75摄氏度
            // temp_status = TEMP_75_30MIN;
            temp_status = TEMP_75_5_MIN;
            tmr1_disable(); // 关闭定时器
            tmr1_cnt = 0;   // 清空时间计数值
            tmr1_is_open = 0;
            return;
        }
        // }  // while(1)
    }
}

// 根据温度（电压值扫描）或9脚的状态来设定占空比
void set_duty(void)
{
    // static bit tmr0_is_open = 0;

    // 如果温度正常，根据9脚的状态来调节PWM占空比
    if (TEMP_NORMAL == temp_status)
    {
        // if (tmr0_is_open == 0)
        // {
        //     tmr0_is_open = 1;
        //     tmr0_enable(); // 打开定时器0，开始根据9脚的状态来调节PWM脉宽
        // }

        // if (tmr0_flag == 1) // 每25ms进入一次
        // {
        //     tmr0_flag = 0;
        //     // adc0_val = 0;                     // 清除之前采集到的电压值
        //     // adc_sel_pin(ADC_SEL_PIN_GET_VOL); // 切换到9脚对应的adc配置
        //     // adc_scan_according_pin9();
        //     according_pin9_to_adjust_pwm();
        //     // 设定占空比
        //     while (c_duty != adjust_duty)
        //     {
        //         Adaptive_Duty(); // 调节占空比
        //     }
        // }

        according_pin9_to_adjust_pwm();
        Adaptive_Duty(); // 调节占空比
#if USE_MY_DEBUG
        // printf("cur duty: %d\n", c_duty);
#endif
    }
    else if (TEMP_75 == temp_status)
    {
        // 如果温度超过了75摄氏度且累计10min
        // tmr0_disable(); // 关闭定时器0，不以9脚的电压来调节PWM
        // tmr0_is_open = 0;
        // 设定占空比
        adjust_duty = PWM_DUTY_50_PERCENT;
        while (c_duty != adjust_duty)
        {
            Adaptive_Duty(); // 调节占空比
        }
    }
    // else if (TEMP_75_30MIN == temp_status)
    else if (TEMP_75_5_MIN == temp_status)
    {
        // tmr0_disable(); // 关闭定时器0，不以9脚的电压来调节PWM
        // tmr0_is_open = 0;
        // 设定占空比
        adjust_duty = PWM_DUTY_25_PERCENT;
        while (c_duty != adjust_duty)
        {
            Adaptive_Duty(); // 调节占空比
        }
    }
}

volatile u16 adc_val_pin_9 = 0; // 存放9脚采集到的ad值
volatile u16 adc_val_pin_9_filter_count = 0;
// u16 adc_val_pin_9_temp;
// 更新9脚采集的ad值
void adc_update_pin_9_adc_val(void)
{
    adc_sel_pin(ADC_SEL_PIN_GET_VOL);
    // adc_val_pin_9_temp = adc_get_val();
    adc_val_pin_9 = adc_get_val();

    // if (adc_val_pin_9 < 200 && adc_val_pin_9_filter_count < 5)
    // {
    //     adc_val_pin_9_filter_count++;
    // }
    // else
    // {    
    //     adc_val_pin_9_filter_count = 0;
    //     adc_val_pin_9 = adc_val_pin_9_temp;
    // }

#if USE_MY_DEBUG // 打印从9脚采集到的ad值
    // printf("adc_val_pin_9 %u\n", adc_val_pin_9);
    // printf(",a=%u,", adc_val_pin_9);
#endif // USE_MY_DEBUG // 打印从9脚采集到的ad值
}
