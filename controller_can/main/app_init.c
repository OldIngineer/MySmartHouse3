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
extern uint8_t number_slaves[MAX_MEMBERS];//массив в котором хранятся локальные номера
         //подчиненных устройств, а в нулевой ячейке число подключенных устройств
extern char namespase[64][10];//Пространство имен устройств в nvs-памяти
extern uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности
        //для 63 устройств сети по 9 типам (включая инф. сервис (0))
        //в порядке перечисления в инф. устройства
//Пространство имен для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
//Пространство имен для записи величины параметров
extern const char namespace_value_par[9][10];
//Пространство имен для записи имен параметров
extern const char namespace_name_par[9][7];

//====данная функция вызывается для инициализации входов/выходов=================
void init_GPIO()
{    
    //gpio_set_direction(CAN_TX, GPIO_MODE_OUTPUT);
    //gpio_set_level(CAN_TX, 1);
    //gpio_set_direction(CAN_RX, GPIO_MODE_INPUT);
    //gpio_set_direction(CAN_S, GPIO_MODE_OUTPUT);
    //gpio_set_level(CAN_S, 0);
    gpio_reset_pin(LED_WORK);
    gpio_set_direction(LED_WORK, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_WORK, 0);
    gpio_reset_pin(LED_ALARM);
    gpio_set_direction(LED_ALARM, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_ALARM, 0);   
    //gpio_reset_pin(M_CLOSE);
    //gpio_set_direction(M_CLOSE, GPIO_MODE_OUTPUT);
    //gpio_set_level(M_CLOSE, 0);
    //gpio_reset_pin(M_OPEN);
    //gpio_set_direction(M_OPEN, GPIO_MODE_OUTPUT);
    //gpio_set_level(M_OPEN, 0);
    gpio_reset_pin(SPI_MOSI);
    gpio_set_direction(SPI_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_level(SPI_MOSI, 1);
    gpio_reset_pin(SCLK);
    gpio_set_direction(SCLK, GPIO_MODE_OUTPUT);
    gpio_set_level(SCLK, 1);
    gpio_reset_pin(nSS);
    gpio_set_direction(nSS, GPIO_MODE_OUTPUT);
    gpio_set_level(nSS, 1);
    gpio_reset_pin(nRESET);
    gpio_set_direction(nRESET, GPIO_MODE_OUTPUT);
    gpio_set_level(nRESET, 1);
    gpio_reset_pin(INIT);
    gpio_set_direction(INIT, GPIO_MODE_INPUT);
    gpio_reset_pin(SPI_nINT);
    gpio_set_direction(SPI_nINT, GPIO_MODE_INPUT);
    gpio_reset_pin(SPI_MISO);
    gpio_set_direction(SPI_MISO, GPIO_MODE_INPUT);
    gpio_reset_pin(SIGNAL);
    gpio_set_direction(SIGNAL, GPIO_MODE_INPUT);
}
//====Функция получения из инф.сервиса данных и 
    // конфигурирование устройства в связи с ними =======
void get_DeviceInf()
{
    extern char obey[7];
    //таблица ячейки которой содержат код типа функциональности
    //extern uint16_t tabl_type[64][9];
    //инициализация раздела флеш-памяти "profile"
    esp_err_t ret = nvs_flash_init_partition("profile");
    //открытие раздела флеш-памяти "profile" с пространством имен  "d_inf"
    nvs_handle_t my_handle;
    ret = nvs_open_from_partition("profile", "d_inf",
         NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 1, error code: %i\n",ret);
            return;
        }
    //получение из памяти значения
    //размер получаемых данных не фиксирован
    size_t required_size;
    ret = nvs_get_str(my_handle,"name1",NULL,&required_size);
    char *name1 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name1",name1,&required_size);
    ret = nvs_get_str(my_handle,"name2",NULL,&required_size);
    char *name2 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name2",name2,&required_size);
    ret = nvs_get_str(my_handle,"name3",NULL,&required_size);
    char *name3 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name3",name3,&required_size);
    ret = nvs_get_str(my_handle,"name4",NULL,&required_size);
    char *name4 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name4",name4,&required_size);
    ret = nvs_get_str(my_handle,"name5",NULL,&required_size);
    char *name5 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name5",name5,&required_size);
    ret = nvs_get_str(my_handle,"name6",NULL,&required_size);
    char *name6 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name6",name6,&required_size);
    ret = nvs_get_str(my_handle,"name7",NULL,&required_size);
    char *name7 = malloc(required_size);
    ret = nvs_get_str(my_handle,"name7",name7,&required_size);
    strcpy(number, name1);//копирование
    nvs_close(my_handle);//закрытие памяти 
    printf("Profile controller initial\n");
    printf("Local number: %s\n", name1);
    printf("Serial number: %s, obey: %s\n",name2,name3);
    printf("Name device: %s\n",name4);
    printf("Place: %s\n",name5);
    printf("Program version: %s\n",name6);
    printf("Code types: %s\n",name7);
    //ЗАПИСЬ ИНФОРМАЦИОННОГО СЕРВИСА В ОБЛАСТЬ ПАМЯТИ "device(lok_number)"
    //инициализация раздела флеш-памяти "device(local_number)"
    ret = nvs_flash_init_partition(namespase[htol(number)]);
    //открытие, если нет создание раздела флеш-памяти "device(local_number)"
    //  с пространством имен  "d_inf"
    ret = nvs_open_from_partition(namespase[htol(number)], "d_inf",
      NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
         printf("Failed open NVS 2, error code: %i\n",ret);
         return;
     }
    //проверка наличия записи в памяти
    char fist_time[3] = "00";
    required_size = 3;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name1",fist_time,&required_size);
    printf("Test true local number: %s,%s\n", fist_time, number);    
    //запись производится только в первый раз (эта область ранее не использовалась)
    if(strcmp(number, fist_time) != 0) {
        nvs_set_str(my_handle,"name1",name1);
        nvs_set_str(my_handle, "name2", name2);
        nvs_set_str(my_handle, "name3", name3);
        nvs_set_str(my_handle, "name4", name4);
        nvs_set_str(my_handle, "name5", name5);
        nvs_set_str(my_handle, "name6", name6);
        nvs_set_str(my_handle, "name7", name7);
        nvs_commit(my_handle);//проверка записи в память
        printf("Initial profile entry\n");
    }
    required_size = 7;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name3",obey,&required_size);
    nvs_close(my_handle);//закрытие памяти
    printf("Real obey: %s\n", obey); 
    //ЕСЛИ УСТРОЙСТВО МАСТЕР
    if(strcmp(obey,"master")==0) {
      read_list_slaves(); // заполнить таблицу number_slaves
       read_tabl_type_flash();//заполнить таблицу tabl_type
    }
    tabl_type[htol(number)][0] = 0;//запись типа функциональности инф.сервис
    //РАЗОБРАТЬ СТРОКУ КОДОВ ТИПОВ
        //если последний символ "#", удалить его
    if(strlen(name7)>0 && name7[strlen(name7)-1]=='#') 
        {name7[strlen(name7)-1] = '\0';}
    //таблица кодов типа устройства
    char code_t[9][4];
    char* ct = strtok(name7, " ");//код первого типа
    uint8_t k = 1;
    // Перебираем строку для извлечения всех остальных кодов типа
    while (ct != NULL && k<9) 
    {
        strcpy(code_t[k], ct);
        ct = strtok(NULL, " ");//следующий код типа
        k++;
    }
    //ИНИЦИАЛИЗАЦИЯ В ЗАВИСИМОСТИ ОТ КОДА ТИПА
    for(uint8_t i=0; i<k; i++) {
        if(strcmp(code_t[i], "070")==0) {
            init_type_070(i);//i - порядковый номер типа в строке
        }
        if(strcmp(code_t[i], "210")==0) {
            init_type_210(i);//i - порядковый номер типа в строке
        }
        if(strcmp(code_t[i], "0A0")==0) {
            init_type_0A0(i);//i - порядковый номер типа в строке
        }
        if(strcmp(code_t[i], "220")==0) {
            init_type_220(i);//i - порядковый номер типа в строке
        }
    }
    uint8_t n = htol(number);
    
    printf("tabl_type[%s]:%d,%d,%d,%d,%d,%d,%d,%d,%d\n",number,tabl_type[n][0],
    tabl_type[n][1],tabl_type[n][2],tabl_type[n][3],tabl_type[n][4],
    tabl_type[n][5],tabl_type[n][6],tabl_type[n][7],tabl_type[n][8]); 
       
}
//===данная функция вызывается для чтения из флеш-памяти инф.сервиса
// и записи в буфер для выдачи в CAN =====
void read_infservice_forcan()
{
    nvs_handle_t my_handle;
    //инициализация раздела флеш-памяти "device(local_number)"
    esp_err_t ret = nvs_flash_init_partition(namespase[htol(number)]);
    //открытие раздела флеш-памяти "device(local_number)" с пространством имен  "d_inf"
    ret = nvs_open_from_partition(namespase[htol(number)], "d_inf",
    NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
     printf("Failed open NVS 3, error code: %i\n",ret);
     return;
    }
    //получение из памяти значения, преобразование в кадр данных и запись в буфер    
    char number_m[3];
    size_t required_size = 3;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name1",number_m,&required_size);
    form_cadr(1, number_m);//преобразование в кадр данных и запись в буфер
    char serial_number_m[6];
    required_size = 6;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name2",serial_number_m,&required_size);
    form_cadr(2, serial_number_m);
    char obey_m[7];   
    required_size = 7;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name3",obey_m,&required_size);
    form_cadr(3, obey_m);
    //размер получаемых данных не фиксирован
    ret = nvs_get_str(my_handle,"name4",NULL,&required_size);
    char* name_device = malloc(required_size);
    ret = nvs_get_str(my_handle,"name4",name_device,&required_size);
    form_many_cadr(4, name_device);
    ret = nvs_get_str(my_handle,"name5",NULL,&required_size);
    char* place = malloc(required_size);
    ret = nvs_get_str(my_handle,"name5",place,&required_size);
    form_many_cadr(6, place);
    ret = nvs_get_str(my_handle,"name6",NULL,&required_size);
    char* v_program = malloc(required_size);
    ret = nvs_get_str(my_handle,"name6",v_program,&required_size);
    //для отладки сбоя
    //if(err == 0) form_cadr(8, v_program);
    //else form_cadr(18, v_program);
    //err =0;
    form_cadr(8, v_program);
    ret = nvs_get_str(my_handle,"name7",NULL,&required_size);
    char* code_type = malloc(required_size);
    ret = nvs_get_str(my_handle,"name7",code_type,&required_size);
    form_many_cadr(9, code_type);
    nvs_close(my_handle);//закрытие памяти     
}
//===данная функция вызывается для чтения из флеш-памяти сервиса
// и записи в буфер для выдачи в CAN =====
void read_service_forcan(uint16_t type_func)
{
    nvs_handle_t my_handle;
    uint8_t num;//порядковый номер типа функциональности как он указан в инф.сервисе
    for(num=0; num<=9; num++) {
      if(tabl_type[htol(number)][num] == type_func) break;            
      }
    printf("number type in inf.service: %d\n", num); 
    if(num==9) return;//нет такого типа выход из п/п
    //инициализация раздела флеш-памяти "device(local_number)"
    esp_err_t ret = nvs_flash_init_partition(namespase[htol(number)]);
//открытие раздела флеш-памяти "device(local_number)" с пространством имен "num"
    ret = nvs_open_from_partition(namespase[htol(number)],
     namespase_type[num], NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
        printf("Failed open NVS 3, error code: %i\n",ret);
        return;
    }   
//получение из памяти значения, преобразование в кадр данных и запись в буфер   
    // имя
    char kode_type[4];
    size_t required_size = 4;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"name1",kode_type,&required_size);
    form_cadr(1, kode_type);//преобразование в кадр данных и запись в буфер
    //размер получаемых данных не фиксирован
    ret = nvs_get_str(my_handle,"name2",NULL,&required_size);
    char* name_type = malloc(required_size);
    ret = nvs_get_str(my_handle,"name2",name_type,&required_size);
    form_many_cadr(2, name_type);
    ret = nvs_get_str(my_handle,"name3",NULL,&required_size);
    char* name_user = malloc(required_size);
    ret = nvs_get_str(my_handle,"name3",name_user,&required_size);
    form_many_cadr(4, name_user);
    printf("kode_type: %s\n", kode_type);
    printf("name_type: %s\n", name_type);
    printf("name_user: %s\n", name_user);
    //название и величина параметра
    for(uint8_t i=1; i<9; i++) {        
    ret = nvs_get_str(my_handle,namespace_name_par[i],NULL,&required_size);
    char* name_p = malloc(required_size);
    ret = nvs_get_str(my_handle,namespace_name_par[i],name_p,&required_size);    
    form_cadr(4+i*2, name_p);
    if(strcmp(name_p, "#")==0) break;
    ret = nvs_get_str(my_handle,namespace_value_par[i],NULL,&required_size);
    char* value_p = malloc(required_size);
    ret = nvs_get_str(my_handle,namespace_value_par[i],value_p,&required_size);
    form_cadr(5+i*2, value_p);
    printf("name_p: %s\n", name_p);
    printf("value_p: %s\n", value_p);    
    }
    nvs_close(my_handle);//закрытие памяти
}            
//===данная функция вызывается для инициирования раздела флеш-памяти  
    // и записи в него списка подключенных устройств =====
void init_list_slaves()
{
    //инициализация раздела флеш-памяти "net_dev"
    esp_err_t ret = nvs_flash_init_partition("net_dev");
    if (ret != ESP_OK) {
        printf("Failed to init NVS net_device\n");
        return;
    }
    nvs_handle_t my_handle;
    //создание пространсво имен общее для подчиненных устройств
    ret = nvs_open_from_partition("net_dev", "general",
         NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 4, error code: %i\n",ret);
            return;
        } 
    //создание списка подчиненных устройств, первый член - число подключенных устр.
    char str_num[6];
    char list_slaves[number_slaves[0]*3];
    list_slaves[0] = '\0';
    for(int i=0; i<=number_slaves[0]; i++) {
        snprintf(str_num, 6, "%d", number_slaves[i]);
        strcat(list_slaves, str_num);
        strcat(list_slaves, ",");
    }
    //запись в nvs-память
    nvs_set_str(my_handle,"list_slaves",list_slaves);
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти    
} 
//=== Функция инициализации и записи в nvs-память информационного сервиса
    // подчиненного устройства ===========
void init_inf_service_slave(const char *slave_n)
{
    extern uint8_t byfer_can[256][9];
    char str[byfer_can[0][0]][8];  
    //формирование из массива полученных данных массив строк
    for(int i=1; i<=byfer_can[0][0]; i++) {
        int m = -1;
        for(int n=7; n>=0; n--) {
            if(byfer_can[i][n] != 0) {// старшие нулевые байты выбрасываются
                if(n!=0) m = n-1;                
                str[i-1][n] = 0;
                break;
            }
        }
        if(m!=-1) {//не нулевой кадр данных
        for(int k=0; k<=m; k++) {
            str[i-1][k] = (char)byfer_can[i][m-k];
            } 
        }
      //printf("str[%d]:%s\n",i-1,str[i-1]);
    }
    //printf("%s\n", slave_n);
    //инициализация раздела флеш-памяти "prof_device"
    esp_err_t ret = nvs_flash_init_partition(slave_n);
     if (ret != ESP_OK) {
        printf("Failed to init NVS-prof_slave\n");
        return;
    }
    nvs_handle_t my_handle;
    //создание пространства имен для данного подчиненного устройства
    ret = nvs_open_from_partition(slave_n,"d_inf",NVS_READWRITE,&my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 5, error code: %i\n",ret);
            return;
        } 
    //запись в nvs-память
    char str_long[70];//для объединения нескольких строк в одну
    nvs_set_str(my_handle,"name1",str[0]);
    nvs_set_str(my_handle,"name2",str[1]);
    nvs_set_str(my_handle,"name3",str[2]);
    str_long[0] = '\0'; strcat(str_long, str[3]); strcat(str_long, str[4]);
    nvs_set_str(my_handle,"name4",str_long);
    //printf("name_d:%s\n", str_long);
    str_long[0] = '\0'; strcat(str_long, str[5]); strcat(str_long, str[6]);
    nvs_set_str(my_handle,"name5",str_long);
    //printf("place:%s\n", str_long);
    nvs_set_str(my_handle,"name6",str[7]);
    str_long[0] = '\0';
    for(int p=1; p<(byfer_can[0][0]-7); p++) {
        strcat(str_long, str[7+p]);
    }
    nvs_set_str(my_handle,"name7",str_long);
    //printf("type_c:%s\n", str_long);
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти    
}
//=== Функция инициализации и записи в nvs-память сервиса
    // подчиненного устройства ===========
void init_service_slave(const char *slave_n)
{
   extern uint8_t byfer_can[256][9];
   extern int8_t ord_tab_type;//порядковый номер текущего кода типа
    char str[byfer_can[0][0]][8];  
    //формирование из массива полученных данных массив строк
    for(int i=1; i<=byfer_can[0][0]; i++) {
        int m = -1;
        for(int n=7; n>=0; n--) {
                if(byfer_can[i][n] != 0) {// старшие нулевые байты выбрасываются
                if(n!=0) m = n-1;                
                str[i-1][n] = 0;
                break;
            }
    }
    if(m!=-1) {//не нулевой кадр данных
    for(int k=0; k<=m; k++) {
        str[i-1][k] = (char)byfer_can[i][m-k];
        } 
    }
    printf("str[%d]:%s\n",i-1,str[i-1]);
  }
  //инициализация раздела флеш-памяти "device*"
    esp_err_t ret = nvs_flash_init_partition(slave_n);
    if (ret != ESP_OK) {
        printf("Failed to init NVS-prof_slave\n");
        return;
    }
    nvs_handle_t my_handle;
    //создание пространства имен данного типа для данного подчиненного устройства
    ret = nvs_open_from_partition(slave_n,namespase_type[ord_tab_type],
        NVS_READWRITE,&my_handle);
    if (ret != ESP_OK) {
        printf("Failed open NVS 5, error code: %i\n",ret);
        return;
    } 
    //запись в nvs-память
    char str_long[70];//для объединения нескольких строк в одну
    nvs_set_str(my_handle,"name1",str[0]);
    str_long[0] = '\0'; strcat(str_long, str[1]); strcat(str_long, str[2]);
    nvs_set_str(my_handle,"name2",str_long);
    //printf("name type:%s\n", str_long);
    str_long[0] = '\0'; strcat(str_long, str[3]); strcat(str_long, str[4]);
    nvs_set_str(my_handle,"name3",str_long);
    //printf("name user:%s\n", str_long);
    for(int j=1; j<9; j++) {
        nvs_set_str(my_handle,namespace_name_par[j],str[j*2+3]);
        //printf("name parametr: %s\n",str[j*2+3]);
        if(strcmp(str[j*2+3],"#")==0) break;
        nvs_set_str(my_handle,namespace_value_par[j],str[j*2+4]);
        //printf("value parametr: %s\n",str[j*2+4]);
    }
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти 
}
    //== Функция чтения информационного сервиса устройства из памяти =========
void read_infservice(const char *device_n)
{
    //инициализация раздела флеш-памяти "prof_device"
     esp_err_t ret = nvs_flash_init_partition(device_n);
     if (ret != ESP_OK) {
        printf("Failed to init NVS-prof_device\n");
        return;
    }
    nvs_handle_t my_handle;
    //открытие пространства имен для данного подчиненного устройства
    ret = nvs_open_from_partition(device_n,"d_inf",NVS_READONLY,&my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 6, error code: %i\n",ret);
            return;
        } 
    //чтение из nvs-памяти
    char number_m[3];
    char obey_m[7];
    char serial_number[6];
    size_t required_size = 3;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"name1",number_m,&required_size);
    required_size = 6;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"name2",serial_number,&required_size);    
    required_size = 7;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"name3",obey_m,&required_size);
    //размер получаемых данных не фиксирован
    nvs_get_str(my_handle,"name4",NULL,&required_size);
    char* name_device = malloc(required_size);
    nvs_get_str(my_handle,"name4",name_device,&required_size);
    nvs_get_str(my_handle,"name5",NULL,&required_size);
    char* place = malloc(required_size);
    nvs_get_str(my_handle,"name5",place,&required_size);
    nvs_get_str(my_handle,"name6",NULL,&required_size);
    char* v_program = malloc(required_size);
    nvs_get_str(my_handle,"name6",v_program,&required_size);
    nvs_get_str(my_handle,"name7",NULL,&required_size);
    char* code_type = malloc(required_size);
    nvs_get_str(my_handle,"name7",code_type,&required_size);
    printf("Profile controller\n");
    printf("Local number: %s\n", number_m);
    printf("Serial number: %s, obey: %s\n",serial_number,obey_m);
    printf("Name device: %s\n",name_device);
    printf("Place: %s\n",place);
    printf("Program version: %s\n",v_program);
    printf("Code types: %s\n",code_type);
    nvs_close(my_handle);//закрытие памяти
}
//=== Функция чтения списка подчиненных устройств =========
void read_list_slaves()
{ 
    extern uint8_t msg_for_mqtt;//флаг сформированного сообщения для передачи брокеру mqtt
    extern char topic_on[40];//строка топика для передачи брокеру mqtt
    extern char data_on[200];//строка данных для передачи брокеру mqtt
    extern uint8_t flag_cont;//флаг внешнего соединения
    extern char number[3];//массив в котором хранится локальный номер устройства
    
    //при первом чтении читается следующее
    char list_slaves[200] = "0,";
    //инициализация раздела флеш-памяти "device"
    esp_err_t ret = nvs_flash_init_partition("net_dev");
    if (ret != ESP_OK) {
        printf("Failed to init NVS net_dev\n");
        return;
    }
    nvs_handle_t my_handle;
    //открыть пространсво имен общее для подчиненных устройств
    ret = nvs_open_from_partition("net_dev", "general",
         NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS 7, error code: %i\n",ret);
            return;
        } 
    //чтение из nvs-памяти
    //размер получаемых данных не фиксирован
    size_t required_size;
    nvs_get_str(my_handle,"list_slaves",NULL,&required_size);
    char* l_s = malloc(required_size);
    ret = nvs_get_str(my_handle,"list_slaves",l_s,&required_size);
    if(ret == ESP_OK) {
        list_slaves[0] = '\0';//обнулить строку
        strcpy(list_slaves, l_s);//копировать
    }
    nvs_close(my_handle);//закрытие памяти    
    printf("LIST SLAVES from nvs-flash: %s\n", list_slaves);
    //перенос лок.номеров из списка в таблицу
        //разделить список на номера устройств
    char *num_slave;
    num_slave = strtok(list_slaves, ",");
    number_slaves[0] = strtol(num_slave, NULL, 10);
    //printf("number_slaves[0]: %d\n", number_slaves[0]);
    for(int i=1; i<=number_slaves[0]; i++) {
        num_slave = strtok('\0', ",");
        number_slaves[i] = strtol(num_slave, NULL, 10); 
        //printf("number_slaves[%d]: %d\n", i, number_slaves[i]);
    }
    //если есть соединение с брокером mqtt
    if(flag_cont==1) {
    //формирование информационного топика для MQTT,список устройств сети
    strcpy(topic_on, "inf/system");
    //преобразовать лок.номер в десятичную строку    
    sprintf(data_on, "%ld", htol(number));
      
    char *p;
    p = strchr(l_s, ',');//удалить строку до ","
    strcat(data_on, p);//объединить строки
    msg_for_mqtt = 1;//флаг сформированного сообщения для передачи брокеру mqtt
  }
}
   
    
    
    
