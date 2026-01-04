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

extern uint8_t number_slaves[MAX_MEMBERS];//массив в котором хранятся локальные номера
         //подчиненных устройств, а в нулевой ячейке число подключенных устройств
extern char namespase[64][10];//Пространство имен устройств в nvs-памяти

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
    extern char number[3];
    extern char obey[7];
    char serial_number[6];
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
    //открытие раздела флеш-памяти "profile" с пространством имен  "device_inf"
    nvs_handle_t my_handle;
    ret = nvs_open_from_partition("profile", "d_inf",
         NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        }
    //получение из памяти значения    
    size_t required_size = 3;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"number",number,&required_size);
    required_size = 6;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"s/n",serial_number,&required_size);    
    required_size = 7;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"obey",obey,&required_size);
    //размер получаемых данных не фиксирован
    ret = nvs_get_str(my_handle,"name_d",NULL,&required_size);
    char* name_device = malloc(required_size);
    ret = nvs_get_str(my_handle,"name_d",name_device,&required_size);
    ret = nvs_get_str(my_handle,"place",NULL,&required_size);
    char* place = malloc(required_size);
    ret = nvs_get_str(my_handle,"place",place,&required_size);
    ret = nvs_get_str(my_handle,"v_program",NULL,&required_size);
    char* v_program = malloc(required_size);
    ret = nvs_get_str(my_handle,"v_program",v_program,&required_size);
    ret = nvs_get_str(my_handle,"type_c",NULL,&required_size);
    char* code_type = malloc(required_size);
    ret = nvs_get_str(my_handle,"type_c",code_type,&required_size);
    nvs_close(my_handle);//закрытие памяти 
    printf("Profile controller\n");
    printf("Local number: %s\n", number);
    printf("Serial number: %s, obey: %s\n",serial_number,obey);
    printf("Name device: %s\n",name_device);
    printf("Place: %s\n",place);
    printf("Program version: %s\n",v_program);
    printf("Code types: %s\n",code_type);
    //ЕСЛИ УСТРОЙСТВО МАСТЕР, заполнить таблицу number_slaves
    if(strcmp(obey,"master")==0) {
      read_list_slaves();  
    }
    //РАЗОБРАТЬ СТРОКУ КОДОВ ТИПОВ
        //если последний символ "#", удалить его
    if(strlen(code_type)>0 && code_type[strlen(code_type)-1]=='#') 
        {code_type[strlen(code_type)-1] = '\0';}
    //таблица кодов типа устройства
    char code_t[8][4];
    char* ct = strtok(code_type, " ");//код первого типа
    uint8_t k = 0;
    // Перебираем строку для извлечения всех остальных кодов типа
    while (ct != NULL && k<8) 
    {
        strcpy(code_t[k], ct);
        ct = strtok(NULL, " ");//следующий код типа
        k++;
    }
    //ИНИЦИАЛИЗАЦИЯ В ЗАВИСИМОСТИ ОТ КОДА ТИПА
    for(uint8_t i=0; i<k; i++) {
        if(strcmp(code_t[i], "070")==0) {}
        if(strcmp(code_t[i], "210")==0) {
            init_type_210(i);//i - порядковый номер типа в строке
        }
        if(strcmp(code_t[i], "0A0")==0) {
            init_type_0A0(i);//i - порядковый номер типа в строке
        }
        if(strcmp(code_t[i], "220")==0) {}
    }    
}
//===данная функция вызывается для чтения из флеш-памяти инф.сервиса
// и записи в буфер для выдачи в CAN =====
void read_inf_service()
{
    //extern uint8_t err;
    //инициализация раздела флеш-памяти "profile"
    esp_err_t ret = nvs_flash_init_partition("profile");
   //если не хватает памяти или ошибка при инициализации
    if (ret==ESP_ERR_NVS_NO_FREE_PAGES||ret==ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ret = nvs_flash_erase_partition("profile");
        // Retry nvs_flash_init
        ret = nvs_flash_init_partition("profile");
    }
    if (ret != ESP_OK) {
        printf("Failed to init NVS-profile");
        return;
    }
    //открытие раздела флеш-памяти "profile" с пространством имен  "device_inf"
    nvs_handle_t my_handle;
    ret = nvs_open_from_partition("profile", "d_inf",
         NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        }
    //получение из памяти значения, преобразование в кадр данных и запись в буфер
    char number_m[3];
    size_t required_size = 3;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"number",number_m,&required_size);
    form_cadr(1, number_m);//преобразование в кадр данных и запись в буфер
    char serial_number_m[6];
    required_size = 6;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"s/n",serial_number_m,&required_size);
    form_cadr(2, serial_number_m);
    char obey_m[7];   
    required_size = 7;//размер получаемых данных фиксирован
    ret = nvs_get_str(my_handle,"obey",obey_m,&required_size);
    form_cadr(3, obey_m);
    //размер получаемых данных не фиксирован
    ret = nvs_get_str(my_handle,"name_d",NULL,&required_size);
    char* name_device = malloc(required_size);
    ret = nvs_get_str(my_handle,"name_d",name_device,&required_size);
    form_many_cadr(4, name_device);
    ret = nvs_get_str(my_handle,"place",NULL,&required_size);
    char* place = malloc(required_size);
    ret = nvs_get_str(my_handle,"place",place,&required_size);
    form_many_cadr(6, place);
    ret = nvs_get_str(my_handle,"v_program",NULL,&required_size);
    char* v_program = malloc(required_size);
    ret = nvs_get_str(my_handle,"v_program",v_program,&required_size);
    //для отладки сбоя
    //if(err == 0) form_cadr(8, v_program);
    //else form_cadr(18, v_program);
    //err =0;
    form_cadr(8, v_program);
    ret = nvs_get_str(my_handle,"type_c",NULL,&required_size);
    char* code_type = malloc(required_size);
    ret = nvs_get_str(my_handle,"type_c",code_type,&required_size);
    form_many_cadr(9, code_type);
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
            printf("Failed open NVS, error code: %i\n",ret);
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
        uint8_t m = 0;
        for(int n=7; n>=0; n--) {
            if(byfer_can[i][n] != 0) {// старшие нулевые байты выбрасываются
                if(n!=0) m = n-1;                
                str[i-1][n] = 0;
                break;
            }
        }
        if(m!=0) {//не нулевой кадр данных
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
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        } 
    //запись в nvs-память
    char str_long[70];//для объединения нескольких строк в одну
    nvs_set_str(my_handle,"number",str[0]);
    nvs_set_str(my_handle,"s/n",str[1]);
    nvs_set_str(my_handle,"obey",str[2]);
    str_long[0] = '\0'; strcat(str_long, str[3]); strcat(str_long, str[4]);
    nvs_set_str(my_handle,"name_d",str_long);
    //printf("name_d:%s\n", str_long);
    str_long[0] = '\0'; strcat(str_long, str[5]); strcat(str_long, str[6]);
    nvs_set_str(my_handle,"place",str_long);
    //printf("place:%s\n", str_long);
    nvs_set_str(my_handle,"v_program",str[7]);
    str_long[0] = '\0';
    for(int p=1; p<(byfer_can[0][0]-7); p++) {
        strcat(str_long, str[7+p]);
    }
    nvs_set_str(my_handle,"type_c",str_long);
    //printf("type_c:%s\n", str_long);
    nvs_commit(my_handle);//проверка записи в память
    nvs_close(my_handle);//закрытие памяти    
}
    //===== Функция чтения профиля подчиненных устройств из памяти =========
void read_prof_slave(const char *slave_n)
{
    //инициализация раздела флеш-памяти "prof_device"
     esp_err_t ret = nvs_flash_init_partition(slave_n);
     if (ret != ESP_OK) {
        printf("Failed to init NVS-prof_slave\n");
        return;
    }
    nvs_handle_t my_handle;
    //открытие пространства имен для данного подчиненного устройства
    ret = nvs_open_from_partition(slave_n,"d_inf",NVS_READONLY,&my_handle);
    if (ret != ESP_OK) {
            printf("Failed open NVS, error code: %i\n",ret);
            return;
        } 
    //чтение из nvs-памяти
    char number_m[3];
    char obey_m[7];
    char serial_number[6];
    size_t required_size = 3;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"number",number_m,&required_size);
    required_size = 6;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"s/n",serial_number,&required_size);    
    required_size = 7;//размер получаемых данных фиксирован
    nvs_get_str(my_handle,"obey",obey_m,&required_size);
    //размер получаемых данных не фиксирован
    nvs_get_str(my_handle,"name_d",NULL,&required_size);
    char* name_device = malloc(required_size);
    nvs_get_str(my_handle,"name_d",name_device,&required_size);
    nvs_get_str(my_handle,"place",NULL,&required_size);
    char* place = malloc(required_size);
    nvs_get_str(my_handle,"place",place,&required_size);
    nvs_get_str(my_handle,"v_program",NULL,&required_size);
    char* v_program = malloc(required_size);
    nvs_get_str(my_handle,"v_program",v_program,&required_size);
    nvs_get_str(my_handle,"type_c",NULL,&required_size);
    char* code_type = malloc(required_size);
    nvs_get_str(my_handle,"type_c",code_type,&required_size);
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
    //при первом чтении читается следующее
    char list_slaves[3] = "0,";
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
            printf("Failed open NVS, error code: %i\n",ret);
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
}
   
    
    
    