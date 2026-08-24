//ДАТЧИК ТЕМПЕРАТУРЫ И ВЛАЖНОСТИ (DHT22/AM2302)
//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <rom/ets_sys.h>//для формирования задержки в мкс
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"

extern char number[3];//массив в котором хранится локальный номер устройства
extern uint32_t tab_param[8][9];//таблица параметров
//таблица ячейки которой содержат код типа функциональности
extern uint16_t tabl_type[64][9];
extern uint8_t flag_event;//флаг разрешения формирования событий в CAN от датчико
extern uint8_t flag_mode;
extern uint32_t param_change;//дискриптор флага изменения параметра

//функция проверки максимальной длительности уровня сигнала на выбранном входе
static int dht_wait_level(gpio_num_t pin, int level, uint32_t timeout_us)
{
    while (gpio_get_level(pin) == level) {
        if (!timeout_us--) return -1;
        ets_delay_us(1);
    }
    return 0;
}
//функция чтения датчика DHT22, Цикл опроса занимает около 6 мс
esp_err_t dht22_read(gpio_num_t pin, int16_t *tem, uint16_t *hum)
{
  uint8_t data[5] = {0};//массив принимаемых байт от датчика
  //== Cтартовый сигнал ====    
  // формирует контроллер
  gpio_set_level(pin, 0);
  ets_delay_us(2000);              // 2 ms лог.0 (0.8-20 ms)
  gpio_set_level(pin, 1);
  ets_delay_us(40);                // 40 us лог.1 (20-200 us) 
  //===== Ответ от датчика ======
  // полож.импульс согласования 80мкс
  if (dht_wait_level(pin, 0, 85) < 0) return ESP_ERR_TIMEOUT;
  if (dht_wait_level(pin, 1, 85) < 0) return ESP_ERR_TIMEOUT;
  // чтение 40 бит
  for (int i = 0; i < 40; i++) {
    // проверка интервала времени лог.0
    if (dht_wait_level(pin, 0, 60) < 0) return ESP_ERR_TIMEOUT;
    // измерение длительности импульса лог.1
    uint32_t t = 0;
    while (gpio_get_level(pin)) {//выполнять пока высокий сигнал
			if (++t > 80) return ESP_ERR_TIMEOUT;
            ets_delay_us(1);
    }
    int byte = i / 8;
    data[byte] <<= 1;
    if (t > 40) data[byte] |= 1;// >40 us определен bit = 1
  }
  //===== Проверка суммы байтов =========
  uint8_t sum = data[0] + data[1] + data[2] + data[3];
  if ((sum & 0xFF) != data[4]) {
        return ESP_ERR_INVALID_CRC;
  }
  //==== Расчет величин температуры и влажности =========  
  //двоичный код до 0.1%
  *hum =  (data[0] << 8) | data[1];
  //двоичный код до 0.1*С, 1 в старшем разряде это "-"
  uint16_t temp = (data[2] << 8) | data[3];
  if(temp & 0x8000) {
    temp &= 0x7FFF;
    *tem = - temp;
  } else *tem = temp;
  return ESP_OK;
}
//программа выполнения задачи опроса датчика температуры/влажности
void request_dht(void *arg)
{
  //Режим GPIO: вывод и ввод в режиме открытого стока
  gpio_set_direction(SIGNAL_DHT, GPIO_MODE_INPUT_OUTPUT_OD);
  static int16_t mem_temp = 0;//запомненное значение температуры  
  //---чтение корректирующих констант из nvs-памяти-----
  static int8_t corec_t = 0;//корректирующая величина датчика температуры
  static int8_t corec_h = 0;//корректирующая величина датчика влажности  
  nvs_handle_t my_handle;
  //инициализация раздела флеш-памяти user_nvs
  esp_err_t ret = nvs_flash_init_partition("user_nvs");
  if (ret != ESP_OK) {
    printf("Failed to init NVS user_nvs\n");    
  } 
  //открытие раздела флеш-памяти "user_nvs" с пространством имен "const
  ret = nvs_open_from_partition("user_nvs", "const",
          NVS_READONLY, &my_handle);
  if (ret != ESP_OK) {
    printf("Failed open user_nvs\n");
  }   
  //размер получаемых данных фиксирован
  size_t required_size = 10;
  char temp[required_size];
  strcpy(temp, "+000");//по умолчанию
  char humid[required_size];
  strcpy(humid, "-000");//по умолчанию  
  ret = nvs_get_str(my_handle,"corec_t",NULL,&required_size);
  if(ret == ESP_OK) {
    char *t = malloc(required_size);
    nvs_get_str(my_handle,"corec_t",t,&required_size);
    strcpy(temp, t);
  }
  ret = nvs_get_str(my_handle,"corec_h",NULL,&required_size);
  if(ret == ESP_OK) {
    char *h = malloc(required_size);
    nvs_get_str(my_handle,"corec_h",h,&required_size);
    strcpy(humid, h);
  }
  nvs_close(my_handle);//закрытие памяти
  printf("temp: %s, humid: %s\n", temp, humid);   
  corec_t = atoi(temp);
  corec_h = atoi(humid);
  printf("correct_t = %d, correct_h = %d\n", corec_t, corec_h);
  //-------------------------------------------------------  
  //Основной цикл задачи
  while (1) {
    int16_t tem = 0;
    uint16_t hum = 0;
    //printf("Read DHT22\n");
    if (dht22_read(SIGNAL_DHT, &tem, &hum) == ESP_OK) {
      tem = tem + corec_t;
      hum = hum + corec_h;
    }
    //printf("Temperature: %d, Humidity: %d\n", tem, hum);
    if(abs(mem_temp-tem)>10) {
      uint8_t num;//порядковый номер типа функциональности как он указан в инф.сервисе
      for(num=0; num<=9; num++) {
        if(tabl_type[htol(number)][num] == 0x070) break;            
      } 
      if(num==9) break;//нет такого типа выход из цикла 
      tab_param[num][1] = tem/10;//с точностью до 1*С
      //запись новой величины параметра в nvs-память
      char str[8];//преобразование величины параметра в строку
      uint16_t val = tab_param[num][1] & 0xFFFF;
      sprintf(str, "%d", val);
      change_profile_nvs(htol(number),0x070,'p',1,str,1);
      tab_param[num][2] = hum/10;//с точностью до 1%
      val = tab_param[num][2] & 0xFFFF;
      sprintf(str, "%d", val);
      change_profile_nvs(htol(number),0x070,'p',2,str,1);      
      //цикл ожидания если запрет формирования событий или не режим ожидания
      while((flag_event==0)||(flag_mode!=0)){
        vTaskDelay(pdMS_TO_TICKS(EVENT_TIMEOUT_MS));
      }
      //два ст.байта - число повторов, средний байт
      // - номер таблиц, мл.байт - № параметра      
      param_change = 2 + (num << 8) + (RETRY << 16);
      flag_mode = 10;//изменение контролируемых сигналов на входе
      //цикл ожидания если запрет формирования событий или не режим ожидания
      while((flag_event==0)||(flag_mode!=0)){
      vTaskDelay(pdMS_TO_TICKS(EVENT_TIMEOUT_MS));
      } 
      param_change = 1 + (num << 8) + (RETRY << 16);     
      flag_mode = 10;   
      mem_temp = tem;     
    }
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
  vTaskDelete(NULL);
}
//Данная функция вызывается для инициализации входов/выходов и
//шаблона сервиса типа в nvs памяти, а также задачи для ОС
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
  //создание задачи опроса датчика
  TaskHandle_t TaskHandleRequestDHT = NULL;
  xTaskCreate(request_dht, "RequestDHT", 4096, NULL, INF_SENSOR_TSK_PRIO,
               &TaskHandleRequestDHT);
  // Проверим, создалась ли задача
  if (TaskHandleRequestDHT == NULL) {
    ESP_LOGE("TASK REQUEST SENSOR DHT", "Failed to task create");
  }    
}