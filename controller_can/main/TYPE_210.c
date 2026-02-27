//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_system.h"
#include "driver/gpio.h"
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"

extern char number[3];//массив в котором хранится локальный номер устройства
extern uint32_t tab_param[8][9];//таблица параметров
extern uint64_t tab_event[8][17];//таблица событий
extern uint32_t tab_make[8][17];//таблица исполнения
extern const char namespase[64][10];//Пространство имен устройств в nvs-памяти
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
extern uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности

//Данная функция вызывается для инициализации входов/выходов и 
//шаблона сервиса типа в nvs памяти, num от 0 до 8
void init_type_210(uint8_t num)
{       
    //инициализация шаблона сервиса
    //чтения сервиса типа функциональности из nvs-памяти в выделенные таблицы
    read_service_type(num);
    //инициализация выходов 
    //параметр 1   
    //gpio_reset_pin(RELAY_1);
    gpio_set_direction(RELAY_1, GPIO_MODE_OUTPUT);    
    gpio_set_level(RELAY_1, tab_param[num][1]);    
    //параметр 2
    //gpio_reset_pin(RELAY_2);
    gpio_set_direction(RELAY_2, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_2, tab_param[num][2]);
    //запись типа функциональности
    tab_param[num][0] = strtol("210", NULL, 16);//код типа
    tabl_type[htol(number)][num] = 0x210;//запись типа функциональности
    uint16_t kod_make = (0x210+(1<<11));//код обработки 1 параметра
    tab_param[num][1] = (tab_param[num][1]&0xFFFF) + (kod_make << 16);
    kod_make = (0x210+(2<<11));//код обработки 2 параметра
    tab_param[num][2] = (tab_param[num][2]&0xFFFF) + (kod_make << 16);
    char data[] = "210";
    //запись типа в память nvs
    change_profile_nvs(htol(number),0x210,'n',1,data,1);
    //запись признака окончания характеристик #
    char name_param[] = "#";
    change_profile_nvs(htol(number),0x210,'t',3,name_param,1);
/*
//..............запись типа в память nvs...............
    //инициализация раздела флеш-памяти "device[number]"
    esp_err_t ret = nvs_flash_init_partition(namespase[htol(number)]);
    //открытие раздела флеш-памяти "device[number]" с пространством имен  типа "type*"
    nvs_handle_t my_handle; 
    ret = nvs_open_from_partition(namespase[htol(number)], namespase_type[num],
         NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 10, error code: %i\n",ret);
            return;
        }     
    //запись в nvs-память
    nvs_set_str(my_handle,"name1", "210");
    //запись признака окончания характеристик #
    nvs_set_str(my_handle,"name3p","#");
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти
//.........................................
*/ 
}
//Данная функция обрабатывает команду полученную извне для данного устройства
int type_210_make(char theme, uint32_t ordinal, char *datas, uint8_t num)
{    
    if((theme == 'p')&&(ordinal < 3)) {
        uint8_t dat = 0;
        if(htol(datas) > 0) dat = 1;
        if(ordinal == 1) {//управление 1 реле
          gpio_set_level(RELAY_1, dat);          
        }
        if(ordinal == 2) {//управление 2 реле
          gpio_set_level(RELAY_2, dat);
        }
        //запись в nvs-память параметров измененной величины
        change_profile_nvs(htol(number), 0x210, 'p', ordinal, datas, 1);
        return 1;
    }
    return 0;
}
