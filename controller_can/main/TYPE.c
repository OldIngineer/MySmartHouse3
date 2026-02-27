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

// Шаблон чтения сервиса типа функциональности из nvs-памяти в выделенную таблицу
void read_service_type(uint8_t num_tab)
{
  extern char number[3];//массив в котором хранится локальный номер устройства  
  extern const char namespase_type[9][6];//Пространство имен
  extern uint32_t tab_param[8][9];//таблица параметров
  extern uint64_t tab_event[8][17];//таблица событий
  extern uint32_t tab_make[8][17];//таблица исполнения
  //Пространство имен устройств в nvs-памяти
  extern const char namespase[64][10];
  //Пространство имен для записи сценариев
  extern const char namespace_script[17][9];

    // По умолчанию при первом чтении сервиса читается следующее:
    char name1[8];  strcpy(name1, "no code");//код типа функциональности
    char name2[15];  strcpy(name2, "no name");//наименование типа функциональности
    char name3[15];  strcpy(name3, "no name");//присвоенное пользователем имя
    char name1p[8];    strcpy(name1p, "no name");
    char value1p[8];  strcpy(value1p, "0");
    char name2p[8];    strcpy(name2p, "no name");
    char value2p[8];  strcpy(value2p, "0");
    char name3p[8];    strcpy(name3p, "no name");
    char value3p[8];  strcpy(value3p, "0");
    char name4p[8];    strcpy(name4p, "no name");
    char value4p[8];  strcpy(value4p, "0");
    char name5p[8];    strcpy(name5p, "no name");
    char value5p[8];  strcpy(value5p, "0");
    char name6p[8];    strcpy(name6p, "no name");
    char value6p[8];  strcpy(value6p, "0");
    char name7p[8];    strcpy(name7p, "no name");
    char value7p[8];  strcpy(value7p, "0");
    char name8p[8];    strcpy(name8p, "no name");
    char value8p[8];  strcpy(value8p, "0"); 
    //инициализация раздела флеш-памяти "device[number]"
    esp_err_t ret = nvs_flash_init_partition(namespase[htol(number)]);
    //открытие раздела флеш-памяти "device[number]" с пространством имен  "service_types"
    nvs_handle_t my_handle;
    ret = nvs_open_from_partition(namespase[htol(number)], namespase_type[num_tab],
         NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 11, error code: %i\n",ret);
            return;
    }
    size_t required_size = 8;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name1",name1,&required_size);
    tab_param[num_tab][0] = strtol(name1, NULL, 0);
    //чтение и обратная запись в память для исключения неопределенности первого раза
    required_size = 15;
    ret = nvs_get_str(my_handle,"name2",name2,&required_size);
    ret = nvs_set_str(my_handle,"name2",name2);    
    ret = nvs_get_str(my_handle,"name3",name3,&required_size);
    ret = nvs_set_str(my_handle,"name3",name3);    
    //...получение из памяти наименования и значения величины параметров......     
    //преобразование числа представленного в шестнадчатиричном коде 
    //в символьном отображении в виде числа uint16 и запись в таблицу
    //и обратная запись в память (исключение неопределенности первого раза)
    required_size = 8;//размер получаемых данных фиксирован    
    ret = nvs_get_str(my_handle,"value1p",value1p,&required_size);
    ret = nvs_set_str(my_handle,"value1p",value1p);    
    tab_param[num_tab][1] = strtol(value1p, NULL, 0);    
    ret = nvs_get_str(my_handle,"value2p",value2p,&required_size);
    ret = nvs_set_str(my_handle,"value2p",value2p);   
    tab_param[num_tab][2] = strtol(value2p, NULL, 0);    
    ret = nvs_get_str(my_handle,"value3p",value3p,&required_size);
    ret = nvs_set_str(my_handle,"value3p",value3p);    
    tab_param[num_tab][3] = strtol(value3p, NULL, 0);
    ret = nvs_get_str(my_handle,"value4p",value4p,&required_size);
    ret = nvs_set_str(my_handle,"value4p",value4p);
    tab_param[num_tab][4] = strtol(value4p, NULL, 0);
    nvs_get_str(my_handle,"value5p",value5p,&required_size);
    ret = nvs_set_str(my_handle,"value5p",value5p);
    tab_param[num_tab][5] = strtol(value5p, NULL, 0);
    nvs_get_str(my_handle,"value6p",value6p,&required_size);
    ret = nvs_set_str(my_handle,"value6p",value6p);
    tab_param[num_tab][6] = strtol(value6p, NULL, 0);
    nvs_get_str(my_handle,"value7p",value7p,&required_size);
    ret = nvs_set_str(my_handle,"value7p",value7p);
    tab_param[num_tab][7] = strtol(value7p, NULL, 0);
    nvs_get_str(my_handle,"value8p",value8p,&required_size);
    ret = nvs_set_str(my_handle,"value8p",value8p);
    tab_param[num_tab][8] = strtol(value8p, NULL, 0);
    ret = nvs_get_str(my_handle,"name1p",name1p,&required_size);
    ret = nvs_set_str(my_handle,"name1p",name1p);
    ret = nvs_get_str(my_handle,"name2p",name2p,&required_size);
    ret = nvs_set_str(my_handle,"name2p",name2p);
    ret = nvs_get_str(my_handle,"name3p",name3p,&required_size);
    ret = nvs_set_str(my_handle,"name3p",name3p);
    ret = nvs_get_str(my_handle,"name4p",name4p,&required_size);
    ret = nvs_set_str(my_handle,"name4p",name4p);
    ret = nvs_get_str(my_handle,"name5p",name5p,&required_size);
    ret = nvs_set_str(my_handle,"name5p",name5p);
    ret = nvs_get_str(my_handle,"name6p",name6p,&required_size);
    ret = nvs_set_str(my_handle,"name6p",name6p);
    ret = nvs_get_str(my_handle,"name7p",name7p,&required_size);
    ret = nvs_set_str(my_handle,"name7p",name7p);
    ret = nvs_get_str(my_handle,"name8p",name8p,&required_size);
    ret = nvs_set_str(my_handle,"name8p",name8p);
    ret = nvs_commit(my_handle);//проверка записи в память
    //printf("Code types: %s\n",name1);
    //printf("value1p: %s; tab_param: %ld\n", value1p, tab_param[num_tab][1]);
    //printf("value2p: %s; tab_param: %ld\n", value2p, tab_param[num_tab][2]);
    //........получение из памяти сценариев....................
    //каждый сценарий представляет строку содержащую две части:
    //event:kod_event,make:kod_make
    for(int i=1; i<17; i++) {
       //размер получаемых данных не фиксирован
        ret = nvs_get_str(my_handle,namespace_script[i],NULL,&required_size);
        if(ret==ESP_ERR_NVS_NOT_FOUND) {//если ключ не существует, первое чтение
        //char str_scripts = '\0';//строка сценариев, нулевая
        tab_event[num_tab][i] = 0;
        } else {
        char* str_script = malloc(required_size);
        nvs_get_str(my_handle,namespace_script[i],str_script,&required_size);
          //printf("str_script:%s\n", str_script);
        //разделить строку и записать в таблицы событий и исполнения
        char code_event[20];
        char code_make[20];
         char *p;
          p = strtok(str_script, ":");
          int n = 0;
          do {
            n++;
            p = strtok('\0', ":,");
            if(p && n==1) strcpy(code_event, p);
            if(p && n==3) strcpy(code_make, p);
          } while(p);
          //printf("code_event:%s, code_make:%s\n", code_event, code_make);
          //16-ричную строку в код типа long long int, long int
          uint64_t kod_event = strtoll(code_event, NULL, 16);
          uint32_t kod_make = htol(code_make);
          tab_event[num_tab][i] = kod_event;
          tab_make[num_tab][i] = kod_make;
          //printf("Take script!\n");
          //printf("kod_event[%d]: %lld\n", i, kod_event);
          //printf("kod_make[%d]: %ld\n", i, kod_make);
        }
    }
    nvs_close(my_handle);//закрытие памяти     
}
