#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "esp32/clk.h"

static TaskHandle_t xTask_CB = NULL;

static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t high_task_awoken = pdFALSE;
    // 因为是中断服务函数，所以不能使用 printf 作为输出，这里可以采用其他方式进行任务通讯。
    xTaskNotifyFromISR(xTask_CB, 0x01, eSetBits, &high_task_awoken); // 使用直达任务通知方式发送一个信号
    return high_task_awoken == pdTRUE;
}

/**
 * 回调信号处理任务
 */
static void task_cb_signal(void *params)
{
    while (1)
    {
        uint64_t counter = 0;
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &counter);
        uint32_t ulNotifiedValue;
        if (xTaskNotifyWait(UINT32_MAX, UINT32_MAX, &ulNotifiedValue, portMAX_DELAY))
        {
            printf("定时器中断回调发生：%d , %lld\n", xTaskGetTickCount(), counter);
        }
    }
}

void app_main(void)
{
    uint32_t apb_freq = esp_clk_apb_freq();
    printf("CPU 主频： %d\n", CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ);
    printf("APB frequency is %d MHz\n", apb_freq / 1000000);
    // 设置定时器参数
    timer_config_t timer_cfg = {
        .divider = 16,                     // 采用16分频
        .counter_dir = TIMER_COUNT_UP,     // 采用累加计数
        .counter_en = TIMER_PAUSE,         // 初始计数器状态，暂停状态
        .alarm_en = TIMER_ALARM_EN,        // 使能中断
        .auto_reload = TIMER_AUTORELOAD_EN // 自动重载
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg); // 初始化定时器

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0); // 设置定时器的初值
    /* 16分频后，每秒钟定时器递增5000000
     * 所以 1ms = 5000个时钟周期，需要等待多少毫秒，直接×5000即可，
     */
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 5000000 * 2); // 两秒后报警
    timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_EN);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);                                         // 使能定时器中断
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_group_isr_callback, NULL, 0); // 添加中断回调函数

    xTaskCreate(task_cb_signal, "Monitor", 1024 * 4, NULL, 1, &xTask_CB); // 启动监听任务
    TickType_t curr = xTaskGetTickCount();
    timer_start(TIMER_GROUP_0, TIMER_0); // 启动定时器

    while (1)
    {
        uint64_t counter = 0;
        double time;
        timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &time);
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &counter);
        printf("当前计数： %lld, %f\n", counter, time);
        vTaskDelayUntil(&curr, pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}
