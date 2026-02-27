//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
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
//таблица ячейки которой содержат код типа функциональности
extern uint16_t tabl_type[64][9];

//Данная функция вызывается для инициализации входов/выходов и
//шаблона сервиса типа в nvs памяти
void init_type_070(uint8_t num)
{
  //инициализация шаблона сервиса
    read_service_type(num);
  //запись типа функциональности
  //tab_param[num][0] = strtol("070", NULL, 16);//код типа
  tab_param[num][0] = 0x070;
  tabl_type[htol(number)][num] = 0x070;//запись типа функциональности
  char data[] = "070";
  //запись типа в память nvs
  change_profile_nvs(htol(number),0x070,'n',1,data,1);
  //запись признака окончания характеристик #
  char name_param[] = "#";
  change_profile_nvs(htol(number),0x070,'t',3,name_param,1);  
}