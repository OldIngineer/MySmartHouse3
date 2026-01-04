#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "app_const.h"
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>


extern uint8_t tab_type[0x400];//таблица где номер ячейки это код типа, а содержимое
    // - порядковый номер типа функциональности как он указан в инф.сервисе
extern const char namespase[64][10];//Пространство имен устройств в nvs-памяти
extern const char namespase_type[9][6];//Пространство имен для для записи
                                       // сервиса функциональности устройства
extern const char namespace_value_par[9][10];//Пространство имен для записи
                                             // величины параметров
extern const char namespace_name[9][6];//Пространство имен для записи имен сервиса
extern const char namespace_name_par[9][7];//Пространство имен для записи имен параметров
extern const char namespace_script[17][9];//Пространство имен для записи сценариев

//Функция преобразования числа в шестнадцатиричном коде представленного
// в символьном отображении в двоичный код размерностью 32 бита(тип long)
//Возвращает начальную часть строки ptr(задана в коде ASCII), преобразованную
//в значение типа long; преобразование завершается
//при обнаружении первого символа, не являющегося частью
//числа; начальные пробелы пропускаются; если число в строке
//не найдено, возвращается ноль
long htol(const char * ptr)
{
   unsigned int value = 0;
   char ch = *ptr;
   //начальные пробелы и табуляция пропускаются
   while(ch == ' ' || ch == '\t')
   ch = *(++ptr);
 for (;;) 
 {
 if (ch >= '0' && ch <= '9')
 value = (value << 4) + (ch - '0');
 else if (ch >= 'A' && ch <= 'F')
 value = (value << 4) + (ch - 'A' + 10);
 else if (ch >= 'a' && ch <= 'f')
 value = (value << 4) + (ch - 'a' + 10);
 else
 return value;
 ch = *(++ptr);
 }
}
//Функция преобразования строки в кадр данных и записи в буфер для передачи в CAN,
// где:
  //- старший байт = номер кадра, это для контроля потери информации на приеме;
  //- 8-й байт размер кадра;(не передается в CAN)
void form_cadr(uint8_t num_cadra, char *str)
{
  extern uint8_t byfer_can[256][9];
  uint8_t size = strlen(str);//размер строки без нулевого байта
  byfer_can[num_cadra][8] = size+1;
  byfer_can[num_cadra][size] = num_cadra;
  for(int i=0; i<size; i++) {
    byfer_can[num_cadra][(size-1)-i] = str[i];    
  }
    byfer_can[0][0]++;//счетчик кадров увеличен на 1
  /*
  printf("byfer_can[%d]: %d %d %d %d %d %d %d %d %d\n",
     num_cadra, byfer_can[num_cadra][8], byfer_can[num_cadra][7],
     byfer_can[num_cadra][6], byfer_can[num_cadra][5], byfer_can[num_cadra][4],
     byfer_can[num_cadra][3], byfer_can[num_cadra][2], byfer_can[num_cadra][1],
     byfer_can[num_cadra][0]);
  */   
}
//Функция преобразования строки в несколько кадров данных
void form_many_cadr(uint8_t num_cadr, char *str) 
{
  //printf("num_cadr: %d\n", num_cadr);
  //разделить на строки по 7 символов
  char (*r)[7] = (char (*)[7])str;
  char str1[8] = {r[0][0],r[0][1],r[0][2],r[0][3],r[0][4],r[0][5],r[0][6],0};
  char str2[8] = {r[1][0],r[1][1],r[1][2],r[1][3],r[1][4],r[1][5],r[1][6],0};
  char str3[8] = {r[2][0],r[2][1],r[2][2],r[2][3],r[2][4],r[2][5],r[2][6],0};
  char str4[8] = {r[3][0],r[3][1],r[3][2],r[3][3],r[3][4],r[3][5],r[3][6],0}; 
  //printf("String: %s,%s,%s,%s,\n",str1,str2,str3,str4);
  uint8_t len = strlen(str);//размер строки без нулевого байта
  if(len<15) {
    form_cadr(num_cadr, str1);
    form_cadr((num_cadr+1), str2);
  }
  if(len>14) {
    form_cadr(num_cadr, str1);
    form_cadr((num_cadr+1), str2);
    form_cadr((num_cadr+2), str3);
  }
  if(len>21) {
    form_cadr((num_cadr+3), str4);
  }
}
//Функция проверка правильности полученного пакета данных от сети CAN
  //на соответствие номера полученного пакета данных с номером пакета внутри
int true_data()
{
  extern uint8_t byfer_can[256][9];
  uint8_t bul = 1;
uint8_t n_str = 0;
for(uint8_t i=1; i<=byfer_can[0][0]; i++) {
    for(int n=7; n>=0; n--) {
        if(byfer_can[i][n] != 0) {
        n_str = byfer_can[i][n];//первый не нулевой байт начиная со старшего
        //byfer_can[i][n] = 0;//номер строки далее не нужен - удалить
        break;
      }
    }
    if(i!=n_str) {
    bul = 0;//признак ошибки
    break;
    }
  }
  //если есть  признак окончания данных и нет ошибки при приеме
  if((byfer_can[0][1]==1)&&(bul==1)) return 1;
  else return 0;  
}
//Функция записи/чтения в nvs-память измененых данных типа устройства, где:
  //num_div - локальный номер устройства, если запись в профиль данного устройства то =255;
  //type - код типа ввиде шестнадцатиричного кода;
  //theme - тип данных: 'p' параметр, 'n' имя; 's' сценарий;
  //ordinal - порядковый номер (параметра, сценария, имени);
  //data - данные которые надо записать/прочитать;
  //wr - признак записи (1), чтения (0).
char *change_profile_nvs(uint8_t num_div, uint16_t type,
   char theme, uint8_t ordinal, char *data, uint8_t wr)
{
  esp_err_t ret;
  nvs_handle_t my_handle;
  size_t required_size;
  uint8_t i = tab_type[type];//порядковый номер типа функциональности как он указан в инф.сервисе
  if(num_div == 255) {
    //инициализация раздела флеш-памяти "profile"
    ret = nvs_flash_init_partition("profile");
    //если не хватает памяти или ошибка при инициализации
    if (ret==ESP_ERR_NVS_NO_FREE_PAGES||ret==ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ret = nvs_flash_erase_partition("profile");
        /* Retry nvs_flash_init */
        ret = nvs_flash_init_partition("profile");
    }
    if (ret != ESP_OK) {
        printf("Failed to init NVS-profile");
        return 0;
    }
      //открытие раздела флеш-памяти "profile" с пространством имен  типа "type*"                
      ret = nvs_open_from_partition("profile", namespase_type[i+1],
                NVS_READWRITE, &my_handle);
      if (ret != ESP_OK) {
        printf("Failed open NVS, error code: %i\n",ret);
        return 0;
      }
  } else {
    //инициализация раздела флеш-памяти "device*"
    ret = nvs_flash_init_partition(namespase[num_div]);
    //если не хватает памяти или ошибка при инициализации
    if (ret==ESP_ERR_NVS_NO_FREE_PAGES||ret==ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ret = nvs_flash_erase_partition(namespase[num_div]);
        /* Retry nvs_flash_init */
        ret = nvs_flash_init_partition(namespase[num_div]);
    }
    if (ret != ESP_OK) {
        printf("Failed to init NVS-profile");
        return 0;
    }
    //открытие раздела флеш-памяти "device*" с пространством имен  типа "type*"
    ret = nvs_open_from_partition(namespase[num_div], namespase_type[i+1],
                NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        printf("Failed open NVS, error code: %i\n",ret);
        return 0;
    }
  }
  if(theme == 'p') {//тип данных: 'p' параметр
    if(wr == 1) {
      //запись в nvs-память
      nvs_set_str(my_handle,namespace_value_par[ordinal], data);
      nvs_commit(my_handle);//проверка записи в память
    } else {
      //чтение из nvs-памяти
      //размер получаемых данных не фиксирован
      nvs_get_str(my_handle,namespace_value_par[ordinal],NULL,&required_size);
      char* value = malloc(required_size);
      nvs_get_str(my_handle,namespace_value_par[ordinal],value,&required_size);
      strcpy(data, value);//копирование прочитанных данных
    }
  }
  if(theme == 's') {//тип данных: 's' сценарий
    if(wr == 1) {//запись в nvs-память
      nvs_set_str(my_handle,namespace_script[ordinal],data);
      nvs_commit(my_handle);//проверка записи в память
    } else {
      //чтение из nvs-памяти
      //размер получаемых данных не фиксирован
      nvs_get_str(my_handle,namespace_script[ordinal],NULL,&required_size);
      char* script = malloc(required_size);
      nvs_get_str(my_handle,namespace_script[ordinal],script,&required_size);
      strcpy(data, script);//копирование прочитанных данных
    }
  }
  nvs_close(my_handle);//закрытие памяти
  return (data);//возврат считанного значения
}
 