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
  //Пространство имен для записи величины параметров
  extern char namespace_value_par[9][10];
  //Пространство имен для записи имен сервиса
  extern const char namespace_name[9][6];
  //Пространство имен для записи имен параметров
  extern char namespace_name_par[9][7];
  //Пространство имен для записи сценариев
  extern const char namespace_script[17][9];

    // По умолчанию при первом чтении сервиса читается следующее:
    char code[8];   strcpy(code, "no code");//код типа функциональности
    char name[15];  strcpy(name, "no name");
    char name_p[8]; strcpy(name_p, "no_name");//наименование параметра
    char value[8];  strcpy(value, "0");    
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
    size_t required_size;//размер получаемых данных    
    nvs_get_str(my_handle,namespace_name[1],NULL,&required_size);
    nvs_get_str(my_handle,namespace_name[1],code,&required_size);
    tab_param[num_tab][0] = strtol(code, NULL, 0);
    //чтение и обратная запись в память для исключения неопределенности первого раза
    nvs_get_str(my_handle,namespace_name[2],NULL,&required_size);
    nvs_get_str(my_handle,namespace_name[2],name,&required_size);
    nvs_set_str(my_handle,namespace_name[2],name);
    strcpy(name, "no name");
    nvs_get_str(my_handle,namespace_name[3],NULL,&required_size);
    nvs_get_str(my_handle,namespace_name[3],name,&required_size);
    nvs_set_str(my_handle,namespace_name[3],name);    
    //...получение из памяти наименования и значения величины параметров......     
    //преобразование числа представленного в шестнадчатиричном коде 
    //в символьном отображении в виде числа uint16 и запись в таблицу
    //и обратная запись в память (исключение неопределенности первого раза)
    for(uint8_t v=1; v<9; v++) {
      nvs_get_str(my_handle,namespace_value_par[v],NULL,&required_size);
      nvs_get_str(my_handle,namespace_value_par[v],value,&required_size);
      nvs_set_str(my_handle,namespace_value_par[v],value);
      tab_param[num_tab][v] = strtol(value, NULL, 0);
      nvs_get_str(my_handle,namespace_name_par[v],NULL,&required_size);
      nvs_get_str(my_handle,namespace_name_par[v],name_p,&required_size);
      nvs_set_str(my_handle,namespace_name_par[v],name_p);
      strcpy(name_p, "no name");
      strcpy(value, "0");
    }   
    ret = nvs_commit(my_handle);//проверка записи в память
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