#include "driver/gpio.h"
#include "app_const.h"
#include "driver/uart.h"
#include "stdio.h"
#include <stdint.h>
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "app_priv.h"
#include "driver/twai.h"
#include "hal/twai_types.h"
#include <stdint.h>
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/twai_periph.h"    // For GPIO matrix signal index
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>

extern char number[3];//массив в котором хранится локальный номер устройства
extern char obey[7];//массив в котором хранится подчиненность устройства
extern uint32_t tab_param[8][9];//таблица параметров
extern uint64_t tab_event[8][17];//таблица событий
extern uint32_t tab_make[8][17];//таблица исполнения
extern const char namespase[64][10];//Пространство имен устройств в nvs-памяти
//Пространство имен для для записи сервиса функциональности устройства
extern const char namespase_type[9][6];
//Пространство имен для записи величины параметров
extern const char namespace_value_par[9][10];
extern uint32_t buf_proof[2];//буфер подтверждения хранения события изменения параметра
    // для передачи в CAN, 0 ячейка расширенный идентификатор,
                        // 1 ячейка номер параметра и его величина
extern uint8_t flag_mode;
extern uint8_t byfer_can[256][9];// двухмерный массив в котором хранятся данные
            // для обмена в сети CAN
extern uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности
extern uint8_t msg_for_mqtt;//флаг сформированного сообщения для передачи брокеру mqtt
extern char topic_on[40];//строка топика для передачи брокеру mqtt
extern char data_on[200];//строка данных для передачи брокеру mqtt

//====данная функция вызывается для инициализации интерфейса CAN==================
void init_CAN(gpio_num_t TX,gpio_num_t RX)
{
    // Initialize configuration structures using macro initializers
    //twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX, RX, TWAI_MODE_NORMAL);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX, RX, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING;
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver TWAI installed\n");
    } else {
        printf("Failed to install driver TWAI\n");
        return;
    }
    // Start TWAI driver
    if (twai_start() == ESP_OK) {
        printf("Driver TWAI started\n");
    } else {
        printf("Failed to start driver TWAI \n");
        return;
    }
}
//===данная функция вызывается для передачи данных по интерфейсу CAN ======
void message_t_CAN(uint32_t ext,uint32_t echo,uint32_t ident,uint8_t len, uint64_t data_transmit)
{
    //формирование массива байт из uint64_t
    uint8_t data_b[8];
    data_b[0] = (uint8_t)((data_transmit)&0xFF);
    data_b[1] = (uint8_t)((data_transmit>>8)&0xFF);
    data_b[2] = (uint8_t)((data_transmit>>16)&0xFF);
    data_b[3] = (uint8_t)((data_transmit>>24)&0xFF);
    data_b[4] = (uint8_t)((data_transmit>>32)&0xFF);
    data_b[5] = (uint8_t)((data_transmit>>40)&0xFF);
    data_b[6] = (uint8_t)((data_transmit>>48)&0xFF);
    data_b[7] = (uint8_t)((data_transmit>>56)&0xFF);
    //Настройка сообщения для передачи 
    twai_message_t message = {
        // Настройки типа и формата сообщения
        .extd = ext, // базовый(0),расширенный формат(1) идентификатора
        .rtr = 0,    // Не кадр удаленного запроса(0)
        .ss = echo,  //Является ли сообщение однократным(1), многократным(0) при ошибке 
        .self = 0, // Является ли сообщение запросом на самостоятельный прием (обратная связь)
        .dlc_non_comp = 0,      // DLC составляет менее 8
        // Идентификатор сообщения и полезная нагрузка
        .identifier = ident,
        .data_length_code = len,
        .data = {data_b[0],data_b[1],data_b[2],data_b[3],data_b[4],data_b[5],data_b[6],data_b[7]},
    };

    //Сообщение в очереди на передачу, время ожидания 200 тактов
    if (twai_transmit(&message, pdMS_TO_TICKS(WAIT_CAN/BAUD_RATE)) == ESP_OK) {    
        //printf("Message queued for transmission\n");
    } else {
        //printf("Failed to queue message for transmission\n");
    } 
}
//===данная функция вызывается для запроса данных от устройства сети CAN
//              (кадр удаленного запроса) ===================================
void remote_frame(uint32_t ext,uint32_t echo,uint32_t ident,uint8_t len)
{
    //Настройка сообщения для передачи 
    twai_message_t message = {
        // Настройки типа и формата сообщения
        .extd = ext, // базовый(0),расширенный формат(1) идентификатора
        .rtr = 1,    // кадр удаленного запроса(1)
        .ss = echo,  //Является ли сообщение однократным(1), многократным(0) при ошибке 
        .self = 0, // Является ли сообщение запросом на самостоятельный прием (обратная связь)
        .dlc_non_comp = 0,      // DLC составляет менее 8
        // Идентификатор сообщения и полезная нагрузка
        .identifier = ident,
        .data_length_code = len,
    };
    //Сообщение в очереди на передачу, время ожидания 400 тактов
    if (twai_transmit(&message, pdMS_TO_TICKS(2*WAIT_CAN/BAUD_RATE)) == ESP_OK) {    
       // printf("Message queued for transmission\n");
    } else {
       // printf("Failed to queue message for transmission\n");
    }
}
//=== данная функция вызывается для обработки событий в сети CAN ======
void event_processing(uint64_t event)
{
    printf("event:%lld\n", event);        
    //сравнение с кодами событий записанных в сценариях устройства
    for(uint8_t i=0; i<8; i++) {
        for(uint8_t k=1; k<17; k++) {
            if(tab_event[i][k] == event) {
                uint32_t make = tab_make[i][k];
                uint16_t val_par = make & 0xFFFF;//величина параметра для изменения
                uint8_t num_par = (make >> 16) & 0xFF;//номер параметра
                uint16_t kod_make = tab_param[i][num_par]>>16;
                make_parametr(kod_make, val_par);//обработать изменение параметра
            //сохранить в буфере подтверждения
            buf_proof[0] = tab_param[i][0];//код типа функциональности
            buf_proof[1] = tab_make[i][k];//данные
            flag_mode = 11;//флаг режима подтверждение исполнения                                          
            //если устройство мастер
            if(strcmp(obey,"master")==0) {                
                //сформировать сообщение о событии для MQTT и записать в память
                uint32_t identifier = event>>24;
                uint8_t num_par = (event>>16)&0xFF;
                uint16_t val_par = event&0xFFFF;
                event_on_mqtt(identifier, num_par, val_par);
            } else {
             //записать измененный параметр в nvs-память
            char data[8];
            sprintf(data, "%d", val_par); 
            change_profile_nvs(htol(number), tab_param[i][0], 'p', num_par, data, 1); 
        }    
      }
    }
  }
}
//==== данная функция вызывается для обработки команды изменения параметра устройства ===
void com_processing(uint32_t identifier, uint16_t val_par, uint8_t n_cadr) 
{    
    uint8_t numb = identifier&0xFF;//лок.номер устройства к которому обращаются
    if(numb==htol(number)) {//это номер данного устройства
        uint16_t type = (identifier >> 18)&0x3FF;//тип функциональности
        if(type==0x210) {
            char theme = 'p';
            uint8_t ordinal = (n_cadr - 4)/2;
            char datas[6];
            snprintf(datas, sizeof datas, "%d", val_par);            
            // выполнить команду и записать в память
            if(type_210_make(theme, ordinal, datas, numb)) {//при удачном выполнении
                //сохранить в буфере подтверждения
                buf_proof[0] = type;//код типа функциональности
                buf_proof[1] = (ordinal << 16) + val_par;
                flag_mode = 11;//флаг режима подтверждение исполнения
            }
        }
    }
} 
//===== Данная функция вызывается для обработки команды изменения имени или сценария ===
void name_script_processing(uint16_t kod_type, uint8_t data_len, uint64_t data)
{
  printf("kod_type:%d, data_len:%d, data:%lld\n", kod_type, data_len, data);
  uint8_t num_cadr = (data>>((data_len-1))*8)&0xFF;//номер полученного кадра
  printf("num_cadr: %d\n", num_cadr);
	uint8_t num;//порядковый номер типа функциональности как он указан в инф.сервисе
	for(num=0; num<=9; num++) {
    if(tabl_type[htol(number)][num] == kod_type) break;                   
	}
	if(num==9) return;//нет такого типа выход из п/п
  //если два кадра данных: сценарий; изменяется имя в инф.сервисе;
												// изменяется имя в сервисе типа
    if((num_cadr>=BEGIN_SCRIPT)||((kod_type==0)&&(3<num_cadr)&&(num_cadr<8))
				||((kod_type!=0)&&(1<num_cadr)&&(num_cadr<6))) {
      if(!(num_cadr&1)) {//первый кадр, четный
        for(int i=0; i<data_len-1; i++) {//запись кадра в буфер            
            byfer_can[num_cadr][i] = (data>>(i*8))&0xFF;
            }
						byfer_can[num_cadr][7] = data_len -1;//длина данных, без номера кадра
            byfer_can[num_cadr][8] = num_cadr;//сохранение для проверки
        } else {//иначе если второй кадр,нечетный
            if((num_cadr-1)==(byfer_can[num_cadr-1][8])) {//проверка
                uint64_t data_rem = 0;
                for(int i=0; i<byfer_can[num_cadr-1][7]; i++) {//восстановление 1-го кадра
                uint64_t ky = byfer_can[num_cadr-1][i];
                data_rem = data_rem + (ky<<(i*8));
                }
						//сценарий
                if(num_cadr>=BEGIN_SCRIPT) { 
                uint8_t ord = (num_cadr-(BEGIN_SCRIPT-1))/2;//порядковый номер сценария
                //запись в таблицу событий
                tab_event[num][ord] = data_rem;
                printf("data_rem:%lld\n", data_rem);
                //запись в таблицу исполнения
                tab_make[num][ord] = data&0xFFFFFF;
                printf("tab_make[%d][%d]:%ld\n", num, ord, tab_make[num][ord]);
                //запись во флеш-память
                char str_script[30] = "event:";//строка сценария
                char str_cod_event[20]; 
                sprintf(str_cod_event, "%llX", data_rem);//код в строку
                strcat(str_script, str_cod_event);
                strcat(str_script, ",make:");
                char str_cod_make[10];
                sprintf(str_cod_make, "%lX", tab_make[num][ord]);
                strcat(str_script, str_cod_make);
                change_profile_nvs(htol(number), kod_type, 's', ord, str_script, 1);
              } else {
						//изменяется имя в сервисе
							char str_name[15] = "";
							uint8_t len1 = byfer_can[num_cadr-1][7];//длина 1 части имени
							uint8_t len = byfer_can[num_cadr-1][7] + (data_len - 1);//длина имени
								for(int i=0; i<len1; i++) {
									str_name[i] = byfer_can[num_cadr-1][len1-1-i];
								}								
								for(int i=len1; i<len; i++) {
									str_name[i] = (data>>((len-1-i)*8))&0xFF;
								}
								//str_name[len+1] = '\0';
								//printf("name: %s\n", str_name);
								//запись во flash-память
								uint8_t ordi = 0;
								if(kod_type==0) {//инф.сервис
									if((num_cadr-1)==4) ordi = 4;//изм. наимен. устройства
									if((num_cadr-1)==6) ordi = 5;//изм. пользоват. имени
								}
								if(kod_type!=0) {//сервис типа
									if((num_cadr-1==2)) ordi = 2;//изм. наименования типа
									if((num_cadr-1)==4) ordi = 3;//изм. пользоват. имени
								}
								if(ordi!=0) {//запись во флеш-память
								change_profile_nvs(htol(number), kod_type, 'n', ordi, str_name, 1);
								}
							}
						}
					}
        }     
  //если изменяется наименование параметра(один кадр)
	if((kod_type!=0)&&(5<num_cadr)&&(num_cadr<BEGIN_SCRIPT)) {
		char name_par[8] = "";
		for(int i=0; i<data_len-1; i++) {
			name_par[i] = (data>>((data_len-2-i)*8))&0xFF;
		}		
		//запись во флеш-память
		uint8_t ord = (num_cadr - 4)/2;
		change_profile_nvs(htol(number), kod_type, 't', ord, name_par, 1);
	}		
}
    