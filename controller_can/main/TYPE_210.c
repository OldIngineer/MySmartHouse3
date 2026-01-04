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

extern uint32_t tab_param[8][9];//таблица параметров
extern uint64_t tab_event[8][17];//таблица событий
extern uint32_t tab_make[8][17];//таблица исполнения
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
extern uint8_t tab_type[0x400];//таблица где номер ячейки это код типа, а содержимое
    // - первый индекс таблиц для обслуживания сервисов типа

//Данная функция вызывается для инициализации входов/выходов и 
//шаблона сервиса типа в nvs памяти, num от 0 до 8
void init_type_210(uint8_t num)
{       
    //инициализация шаблона сервиса
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
//..............запись типа в память nvs...............
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
    nvs_handle_t my_handle; 
    ret = nvs_open_from_partition("profile", namespase_type[num+1],
         NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        }     
    //запись в nvs-память
    nvs_set_str(my_handle,"type_code", "210");
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти
//.........................................
 tab_type[0x210] = num;//запись номера таблиц обслуживания сервиса

    uint16_t kod_make = (0x210+(1<<11));//код обработки 1 параметра
    tab_param[num][1] = (tab_param[num][1]&0xFFFF) + (kod_make << 16);
    kod_make = (0x210+(2<<11));//код обработки 2 параметра
    tab_param[num][2] = (tab_param[num][2]&0xFFFF) + (kod_make << 16);
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
        change_profile_nvs(255, 0x210, 'p', ordinal, datas, 1);
        return 1;
    }
    return 0;
}