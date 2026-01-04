#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <iot_button.h>
#include <nvs_flash.h>
#include "esp_system.h"
#include "app_priv.h"
#include "app_const.h"
#include "string.h"
#include <stdint.h>

#define BUTTON_ACTIVE_LEVEL  0

//========= ПРОГРАММЫ ОБРАБОТКИ НАЖАТИЯ КНОПКИ ============================
// Эта кнопка используется для управления работой
#define BUTTON_GPIO       INIT
// Этот выход используется для индикации работы 
#define OUTPUT_GPIO    LED_WORK
//при отпускании кнопки вызывается эта функция
static void push_btn_cb(void *arg)
{       
    extern char obey[7];
    extern uint8_t flag_mode;
    if(strcmp(obey,"master")==0)
    {
        flag_mode = 1;
    } else {

    }    
}
//при нажатии кнопки более 3 сек. вызывается данная функция
static void button_press_3sec_cb(void *arg)
{
    //пока не используется   
}
static void configure_push_button(int gpio_num, void (*btn_cb)(void *))
{
    //создаем объект кнопки
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    //регистрация обратных вызовов для кнопки
    if (btn_handle) {
        //только нажать и отпустить
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, btn_cb, "RELEASE");
        //нажать и держать более 3 сек.
        iot_button_add_on_press_cb(btn_handle, 3, button_press_3sec_cb, NULL);
    }
}

void app_driver_init()
{
    configure_push_button(BUTTON_GPIO, push_btn_cb);
}
