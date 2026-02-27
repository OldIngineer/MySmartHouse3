//подключение общих программ
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"
//подключение пользовательских значений
#include "app_const.h"
#include "sdkconfig.h"

extern char number[3];//массив в котором хранится локальный номер устройства
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
//tab_param[*][0] - код типа функциональности для данного устройства
extern uint32_t tab_param[8][9];
extern uint8_t msg_for_mqtt;//флаг сформированного сообщения для передачи брокеру mqtt
extern char topic_on[40];//строка топика для передачи брокеру mqtt
extern char data_on[200];//строка данных для передачи брокеру mqtt
extern uint8_t number_slaves[MAX_MEMBERS];//массив в котором хранятся локальные номера
         //подчиненных устройств, а в нулевой ячейке число подключенных устройств
extern uint8_t flag_mode;//флаг режима работы
extern uint32_t buf_com_can[3];//буфер сформированной команды для CAN полученной из вне
                    // 0 ячейка 2 ст. байта - тип функциональности, 2 мл. байта лок.номер устройства
                    // 1 ячейка содержимое кадра, младшие байты
                    // 2 ячейка (старший байт - номер кадра) содержимое кадра, старшие байты
extern uint8_t byfer_can[256][9];// двухмерный массив в котором хранятся данные
            // для обмена в сети CAN, а в ячейке:
            // byfer_can[0][0] число кадров данных;
            // byfer_can[0][1] признак окончания данных =1;
extern uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности
extern uint64_t tab_event[8][17];//таблица событий
extern uint32_t tab_make[8][17];//таблица исполнения
extern uint8_t flag_cont;//флаг внешнего соединения

//Функция анализа сообщения полученного от mqtt-сервера
void mqtt_msg_in(int topic_len, char *topic, int data_len, char *data)
{
  uint8_t t_ok = 8;//указатель на код типа
  uint8_t num_ok = 0;//указатель наличия лок.номера в списке системы
   //Если топик является командой (начинается с "com")
    if(strncmp(topic, "com", 3) == 0) {
      //printf("%.*s, %.*s\n", topic_len, topic, data_len, data);
      char datas[60] = "";
      strncat(datas, data, data_len);
      char top[40] = "";
      strncat(top, topic, topic_len);   
      //запомнить топик без префикса для дальнейшей отправки подтверждения
      char *t_m;
      char topic_mem[40] = "";
      t_m = strchr(top, '/');
      strcat(topic_mem, t_m);
      //printf("Topic_mem: %s\n", topic_mem);  
      //разделить строку top на части
      char *t;
      char num[3];//лок.номер контроллера
      char type[4];//тип функциональности
      char theme[2];//тема
      char ordinal[3];//порядковый номер (параметра, сценария, имени)
      t = strtok(top, "/");      
      int i = 1;
      do {
        i++;
        t = strtok('\0', "/");
        if(t && i==2) strcpy(num, t);
        if(t && i==3) strcpy(type, t);
        if(t && i==4) strcpy(theme, t);
        if(t && i==5) strcpy(ordinal, t);                             
      } while(t);    
    //если команда для этого устройства  этого устройства           
    if(htol(num)==htol(number)) {
      //если указанный тип присутствует в перечне типов t_ok!=8 
      for(int n=0; n<8; n++) {
        if(tab_param[n][0]==htol(type)) t_ok = n;
      }
    }
    //если команда не для этого устройства, а для подчиненного
    if(strcmp(num, number) != 0) {
    //если указанный локальный номер контроллера присутствует в перечне num_ok!=0      
      for(int k=1; k<MAX_MEMBERS; k++) {
        if(number_slaves[k]==htol(num)) num_ok = number_slaves[k];
      }
    }
      if(theme[0]=='s') {//если сценарий
      //JSON разбор   {”event”:{“2/A0/p/2”:”1”},”make”:{“1”:”1”}}
      //убрать кавычки и скобки, т.к. все числа 16-ричные в символьном виде
        char dat[60];
        strcpy(dat, datas);
        char *d = dat;
        char *a = datas;
        do {
          if(*a != '"' && *a != '{' && *a != '}') *d++ = *a;
        } while (*a++);
        //printf("dat:%s\n", dat);   
        //разделить строку на части
        char num_e[3];
        char type_e[4];
        char theme_e[2];
        char ordinal_e[3];
        char value_e[6];
        char ordinal_m[3];
        char value_m[6];
        char *p;
        p = strtok(dat, ":");
        int n = 0;
        do {
          n++;
          p = strtok('\0', ":,/");
          if(p && n==1) strcpy(num_e, p);
          if(p && n==2) strcpy(type_e, p);
          if(p && n==3) strcpy(theme_e, p);
          if(p && n==4) strcpy(ordinal_e, p);
          if(p && n==5) strcpy(value_e, p);
          if(p && n==7) strcpy(ordinal_m, p);
          if(p && n==8) strcpy(value_m, p);
        } while(p);
        //printf("num_e:%s, type_e:%s, theme_e:%s, ordinal_e:%s,
        //  value_e:%s, ordinal_m:%s, value_m:%s\n", num_e, type_e,
        //  theme_e, ordinal_e, value_e, ordinal_m, value_m );
        //формирование кодов данных для посылки в CAN
        uint8_t num_cadr = (htol(ordinal)*2+(BEGIN_SCRIPT-2));//номер 1 кадра
        byfer_can[0][0] = num_cadr +1;//номер последнего кадра
        //event - событие, 1 кадр
        uint32_t id_even = (htol(type_e)<<18) + (1<<17) + htol(num_e);//идентификатор события
        uint64_t id_event = id_even;
        byfer_can[num_cadr][8] = 8;//размер кадра в байтах
        byfer_can[num_cadr][7] = num_cadr;
        byfer_can[num_cadr][6] = (id_even >> 24) & 0xFF;
        byfer_can[num_cadr][5] = (id_even >> 16) & 0xFF;
        byfer_can[num_cadr][4] = (id_even >> 8) & 0xFF;
        byfer_can[num_cadr][3] = id_even & 0xFF;
        byfer_can[num_cadr][2] = htol(ordinal_e);//порядковый номер параметра          
        byfer_can[num_cadr][1] = (htol(value_e)>>8) & 0xFF;//ст. байт данных параметра
        byfer_can[num_cadr][0] = htol(value_e) & 0xFF;//мл. байт данных параметра
        //make - выполнить, 2 кадр
        byfer_can[num_cadr+1][8] = 4;//размер кадра в байтах
        byfer_can[num_cadr+1][3] = num_cadr + 1;
        byfer_can[num_cadr+1][2] = htol(ordinal_m);//порядковый номер параметра
        byfer_can[num_cadr+1][1] = (htol(value_m)>>8) & 0xFF;//ст. байт данных параметра
        byfer_can[num_cadr+1][0] = htol(value_m) & 0xFF;//мл. байт данных параметра
        //запись во флеш память
        char str_script[30] = "event:";//строка сценария
        uint64_t cod_event = (id_event<<24) + (htol(ordinal_e)<<16) + htol(value_e);        
        char str_cod_event[20]; 
        sprintf(str_cod_event, "%llX", cod_event);//код в строку
        strcat(str_script, str_cod_event);
        strcat(str_script, ",make:");
        uint32_t cod_make = (htol(ordinal_m)<<16) + htol(value_m);
        char str_cod_make[10];
        sprintf(str_cod_make, "%lX", cod_make);
        strcat(str_script, str_cod_make);
        //если команда для этого устройства и тип есть в списке
        if((htol(num)==htol(number))&&(t_ok!=8)) {
          //запись во флеш память         
          change_profile_nvs(htol(num), htol(type), 's', htol(ordinal), str_script, 1);
          tab_event[t_ok][htol(ordinal)] = cod_event;//запись в таблицу событий
          tab_make[t_ok][htol(ordinal)] = cod_make;//запись в таблицу исполнения
          printf("cod_event: %lld\n", cod_event);
          printf("cod_make: %ld\n", cod_make);
          printf("str_script: %s\n", str_script);          
        }
    //если команда не для этого устройства, а для подчиненного лок.номер в списке
        if((strcmp(num, number)!=0)&&(num_ok!=0)) {
          //запись во флеш память
          change_profile_nvs(htol(num), htol(type), 's', htol(ordinal), str_script, 1);
          //передать два кадра данных по CAN
          flag_mode = 6;
        }
      }
      //если команда для этого устройства и тип есть в списке
      if((htol(num)==htol(number))&&(t_ok!=8)) {
        //printf("num=%s, type=%s, theme=%s, ordinal=%s\n", num, type, theme, ordinal);
        if(theme[0]=='p') {//если изменение параметра
          if(strcmp(type, "210") == 0) {
            //исполнить команду
            if(type_210_make(theme[0], htol(ordinal), datas, t_ok)) {
              //сформировать топик подтверждения
              topic_on[0] = '\0';
              strcat(topic_on, "inf"); 
              strcat(topic_on, topic_mem);
              //printf("Topic_on: %s\n", topic_on);
              data_on[0] = '\0';          
              strcat(data_on, datas);
              msg_for_mqtt = 1;
            }  
          }
        }
      } 
    //если команда не для этого устройства, а для подчиненного лок.номер в списке
      if((strcmp(num, number)!=0)&&(num_ok!=0)) {
        buf_com_can[0] = (htol(type)<<16) + htol(num);
        if(theme[0]=='p') {//если изменение параметра
          buf_com_can[1] = htol(datas);//данные параметра 2байта
          buf_com_can[2] = (htol(ordinal)*2+4)<<24;//номер кадра
          //передать команду по CAN
        flag_mode = 5;
        }
      }
    }
}
// Функция формирования сообщения о событии в сети CAN для отправки в MQTT,
    //а также запись события во флеш память
void event_on_mqtt(uint32_t identifier, uint8_t num_par, uint16_t val_par) {
  uint8_t num_div = identifier&0xFF;//локальный номер устройства
  uint16_t type = identifier >> 18;//тип функциональности
  char theme = 'p';//параметр
  uint8_t ordinal = num_par;//порядковый номер параметра
  char data[6];//преобразовать число в строку
  snprintf(data, sizeof data, "%X", val_par);
  // запись события во флеш память
  change_profile_nvs(num_div, type, theme, ordinal, data, 1);
  /*  //TEST
  char *s;
  s = change_profile_nvs(num_div, type, theme, ordinal, data, 0);
  printf("Value parametr from nvs: %s\n", s);
  */
  //если есть признак соединения с брокером mqtt
  if(flag_cont == 1) {
  //формирование информационного топика для MQTT
  topic_on[0] = '\0';//обнулить строку
  strcat(topic_on, "inf/");
  char num_loc[6];//преобразовать число в строку
  snprintf(num_loc, sizeof num_loc, "%X", num_div);
  strcat(topic_on, num_loc);
  strcat(topic_on, "/");
  char types[6];//преобразовать число в строку
  snprintf(types, sizeof types, "%X", type);
  strcat(topic_on, types);
  strcat(topic_on, "/p/");
  char ord[6];//преобразовать число в строку
  snprintf(ord, sizeof ord, "%X", num_par);
  strcat(topic_on, ord);
  strcpy(data_on, data);
  msg_for_mqtt = 1;//флаг сформированного сообщения для передачи брокеру mqtt
  }
}
// Функция формирования сообщений (топиков) для брокера mqtt
// из флеш-памяти, формируется полько при признаке соединения с брокером mqtt
void data_service_on_mqtt(uint8_t num_dev, uint8_t num_type,
     char theme, uint8_t ordinal)
     //где: num_dev - локальный номер устройства в сети;
          //num_type - номер типа функциональности устройства
            //  в порядке записи в профиле
          //theme - признак имя/параметр/сценарий
          //ordinal - индекс (порядковый номер)
  {
    if(flag_cont == 1) {
    uint16_t type = tabl_type[num_dev][num_type];//код типа
    char d[40] = "\0";
    char *str_data;
    //чтение строки данных из флеш памяти
    str_data = change_profile_nvs(num_dev, type, theme, ordinal, d, 0);
    //printf("str_data: %s\n", str_data);
    //формирование информационного топика для MQTT
    topic_on[0] = '\0';//обнулить строку
    strcat(topic_on, "inf/");
    char numd[6];//преобразовать число в строку
    snprintf(numd, sizeof numd, "%X", num_dev);
    strcat(topic_on, numd);
    strcat(topic_on, "/");
    char typ[6];//преобразовать число в строку
    snprintf(typ, sizeof typ, "%X", type);
    strcat(topic_on, typ);
    strcat(topic_on, "/");
    char cha[6];//преобразовать число в строку
    snprintf(cha, sizeof cha, "%c", theme);
    strcat(topic_on, cha);
    strcat(topic_on, "/");
    char ordin[6];//преобразовать число в строку
    snprintf(ordin, sizeof ordin, "%X", ordinal);
    strcat(topic_on, ordin);
    strcpy(data_on, str_data);
    msg_for_mqtt = 1;//флаг сформированного сообщения для передачи брокеру mqtt
    }
  }
