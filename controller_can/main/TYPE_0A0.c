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

extern char number[3];//массив в котором хранится локальный номер устройства
extern uint8_t flag_mode;
extern uint32_t tab_param[8][9];//таблица параметров
extern uint32_t param_change;//дискриптор флага изменения параметра
extern uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности
extern const char namespase[64][10];//Пространство имен устройств в nvs-памяти
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
extern uint8_t flag_event;//флаг разрешения формирования событий в CAN от датчиков

//при отпускании кнопки вызывается эта функция
    void push_btn_cb1p(void *arg)
    {   
        uint8_t num;//порядковый номер типа функциональности как он указан в инф.сервисе
        for(num=0; num<=9; num++) {
            if(tabl_type[htol(number)][num] == 0x0A0) break;            
        } 
        if(num==9) return;//нет такого типа выход из п/п 
        //цикл ожидания если запрет формирования событий или не режим ожидания
        while((flag_event==0)||(flag_mode!=0)){
            vTaskDelay(pdMS_TO_TICKS(EVENT_TIMEOUT_MS));
        }
        flag_mode = 10;//изменение контролируемых сигналов на входе
        if(tab_param[num][1]==0) tab_param[num][1] = 1;
        else tab_param[num][1] = 0;
         //два ст.байта - число повторов, средний байт
         // - номер таблиц, мл.байт - № параметра       
        param_change = 1 + (num << 8) + (RETRY << 16);
    //запись новой величины параметра в nvs-память
        char str[8];//преобразование величины параметра в строку
        uint16_t val = tab_param[num][1] & 0xFFFF;
        sprintf(str, "%X", val);
        change_profile_nvs(htol(number),0x0A0,'p',1,str,1);
    }
    void push_btn_cb2p(void *arg)
    {    
        uint8_t num;//порядковый номер типа функциональности как он указан в инф.сервисе
        for(num=0; num<=9; num++) {
            if(tabl_type[htol(number)][num] == 0x0A0) break;            
        }
        if(num==9) return;//нет такого типа выход из п/п 
        //цикл ожидания если запрет формирования событий или не режим ожидания
        while((flag_event==0)||(flag_mode!=0)){
            vTaskDelay(pdMS_TO_TICKS(EVENT_TIMEOUT_MS));
        }       
        flag_mode = 10;//изменение контролируемых сигналов на входе
        if(tab_param[num][2]==0) tab_param[num][2] = 1;
        else tab_param[num][2] = 0;        
        param_change = 2 + (num << 8) + (RETRY << 16);
        //запись новой величины параметра в nvs-память
        char str[8];//преобразование величины параметра в строку
        uint16_t val = tab_param[num][2] & 0xFFFF;
        sprintf(str, "%X", val);
        change_profile_nvs(htol(number),0x0A0,'p',2,str,1);
    }
//Данная функция вызывается для инициализации входов/выходов и 
//шаблона сервиса типа в nvs памяти
void init_type_0A0(uint8_t num)
{           
    //инициализация шаблона сервиса
    //чтение сервиса типа функциональности из nvs-памяти в выделенные таблицы
    read_service_type(num);
    //запись типа функциональности
    //tab_param[num][0] = strtol("0A0", NULL, 16);//код типа
    tab_param[num][0] = 0x0A0;//код типа
    tabl_type[htol(number)][num] = 0x0A0;//запись типа функциональности
    //запись типа в память nvs
    char data[] = "0A0";
    change_profile_nvs(htol(number),0x0A0,'n',1,data,1);
    //запись признака окончания характеристик #
    char name_param[] = "#";
    change_profile_nvs(htol(number),0x0A0,'t',3,name_param,1);
    //инициализация входов
    //1 параметр 
    gpio_reset_pin(SIGN_H);
    gpio_set_direction(SIGN_H, GPIO_MODE_INPUT);
    //2 параметр
    gpio_reset_pin(SIGN_L);
    gpio_set_direction(SIGN_L, GPIO_MODE_INPUT);    
    tabl_type[htol(number)][num] = 0x0A0;//запись типа функциональности
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
