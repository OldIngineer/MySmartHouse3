//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include <freertos/semphr.h>
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <iot_button.h>
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"

#define BUTTON_ACTIVE_LEVEL  0
extern uint8_t flag_mode;
extern uint32_t tab_param[8][9];//таблица параметров
extern uint32_t param_change;//дискриптор флага изменения параметра
extern uint8_t tab_type[0x400];//таблица типов функциональности
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];

//при отпускании кнопки вызывается эта функция
    void push_btn_cb1p(void *arg)
    {   
        uint8_t num = tab_type[0x0A0];       
        flag_mode = 10;//изменение контролируемых сигналов на входе
        if(tab_param[num][1]==0) tab_param[num][1] = 1;
        else tab_param[num][1] = 0;
         //два ст.байта - число повторов, средний байт
         // - номер таблиц, мл.байт - № параметра       
        param_change = 1 + (num << 8) + (RETRY << 16);
    //запись новой величины параметра в nvs-память
        //инициализация раздела флеш-памяти "profile"
        nvs_flash_init_partition("profile");      
      //открытие раздела флеш-памяти "profile" с пространством имен  типа "type*"
        nvs_handle_t my_handle;    
        nvs_open_from_partition("profile", namespase_type[num+1],
            NVS_READWRITE, &my_handle);
        char str[8];//преобразование величины параметра в строку
        uint16_t val = tab_param[num][1] & 0xFFFF;
        snprintf(str, sizeof str, "%X", val);
        nvs_set_str(my_handle,"value1p", str);
        nvs_commit(my_handle);//проверка записи в память
        nvs_close(my_handle);//закрытие памяти
    }
    void push_btn_cb2p(void *arg)
    {    
        uint8_t num = tab_type[0x0A0];       
        flag_mode = 10;//изменение контролируемых сигналов на входе
        if(tab_param[num][2]==0) tab_param[num][2] = 1;
        else tab_param[num][2] = 0;        
        param_change = 2 + (num << 8) + (RETRY << 16);
    //запись новой величины параметра в nvs-память
        //инициализация раздела флеш-памяти "profile"
        nvs_flash_init_partition("profile");      
      //открытие раздела флеш-памяти "profile" с пространством имен  типа "type*"
        nvs_handle_t my_handle;    
        nvs_open_from_partition("profile", namespase_type[num+1],
            NVS_READWRITE, &my_handle);
        char str[8];//преобразование величины параметра в строку
        uint16_t val = tab_param[num][2] & 0xFFFF;
        snprintf(str, sizeof str, "%X", val);
        nvs_set_str(my_handle,"value2p", str);
        nvs_commit(my_handle);//проверка записи в память
        nvs_close(my_handle);//закрытие памяти
        //printf("ky-ky 2p\n");
    }
//Данная функция вызывается для инициализации входов/выходов и 
//шаблона сервиса типа в nvs памяти
void init_type_0A0(uint8_t num)
{           
    //инициализация шаблона сервиса
    read_service_type(num);
    //запись типа функциональности
    tab_param[num][0] = strtol("0A0", NULL, 16);//код типа
//запись типа в память nvs
    //инициализация раздела флеш-памяти "profile"
    esp_err_t ret = nvs_flash_init_partition("profile");
   //если не хватает памяти или ошибка при инициализации
    if (ret==ESP_ERR_NVS_NO_FREE_PAGES||ret==ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ret = nvs_flash_erase_partition("profile");
        /* Retry nvs_flash_init */
        ret = nvs_flash_init_partition("profile");
    }
    if (ret != ESP_OK) {
        printf("Failed to init NVS-profile");
        return;
    }    
    //открытие раздела флеш-памяти "profile" с пространством имен  типа "type*"
    nvs_handle_t my_handle; //   "type3"
    ret = nvs_open_from_partition("profile", namespase_type[num+1],
         NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        }
    //запись в nvs-память
    nvs_set_str(my_handle,"type_code", "0A0");
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти
    //инициализация входов
    //1 параметр 
    gpio_reset_pin(SIGN_H);
    gpio_set_direction(SIGN_H, GPIO_MODE_INPUT);
    //2 параметр
    gpio_reset_pin(SIGN_L);
    gpio_set_direction(SIGN_L, GPIO_MODE_INPUT);
    tab_type[0x0A0] = num;//запись номера таблиц обслуживания сервиса
    //==== программа обработки импульсного сигнала (кнопки) по входу =====    
    void configure_signal(int gpio_num, void (*btn_cb)(void *))
    {
    //создаем объект кнопки
    button_handle_t btn_handle = iot_button_create(gpio_num, BUTTON_ACTIVE_LEVEL);
    //регистрация обратных вызовов для кнопки
    if (btn_handle) {
        //только нажать и отпустить
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, btn_cb, "RELEASE");
        }
    }
    void signal_driver_init(gpio_num_t gpio_num)
    {
        if(gpio_num==SIGN_H) {
           configure_signal(gpio_num, push_btn_cb1p); 
        }
        if(gpio_num==SIGN_L) {
           configure_signal(gpio_num, push_btn_cb2p); 
        }
    }
//========================================================
    signal_driver_init(SIGN_H);//инициализация действия по сигналу 1 параметр
    signal_driver_init(SIGN_L);//инициализация действия по сигналу 2 параметр
}