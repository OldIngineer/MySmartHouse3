//ПРИЕМ/ПЕРЕДАЧА SMS через GSM-модуль
//объявление используемых функций из других подключенных файлов *.
#include "app_priv.h"
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
#include "driver/uart.h"
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>

extern uint8_t flag_msg;//флаг сформированного сообщения в формате mqtt
extern uint16_t subs[3];//коды подписки полученные через sms
extern char topic_on_sms[20];//строка топика для передачи через sms
extern char data_on_sms[10];//строка данных для передачи через sms

//Функция посылки AT-команд
void send_at_com(const char* cmd) {
    uart_write_bytes(UART_NUM_1, cmd, strlen(cmd));
    uart_write_bytes(UART_NUM_1, "\r\n", 2);
}
//Функция чтения данных от модема через UART
void read_from_modem(uint8_t *data)
{
	int len = uart_read_bytes(UART_NUM_1,data,(BUF_SIZE - 1),10);//ожидание 10 тик
	if(len > 0) {//если что-то получено, то в конец строки 0
	data[len] = '\0';
	printf("Modem GSM: %s", data);
  	}
}
//Функция разбора строки подписки полученной от sms
void parsing_subs(char* text_subs)
{
	//если сброс подписок
	if(strncmp(text_subs, "inf/!#", 6)==0) {
		subs[0]=subs[1]=subs[2]=0;
	} else {
	char *p = strtok(text_subs, "/");
	for(uint8_t i=0; i<4; i++) {
		p = strtok('\0', "/");
		if(i==0) {
			if(strncmp(p, "+", 1)==0) {
				subs[0] = 0xFF;
			} else	subs[0] = htol(p);//номер узла
		}
		if(i==1) {
			if(strncmp(p, "+", 1)==0) {
				subs[1] = 0xFF;
			} else	subs[1] = htol(p);//код функции
		}
		if(i==3) {
			if(strncmp(p, "+", 1)==0) {
				subs[2] = 0xFF;
			} else	subs[2] = htol(p);//номер параметра
		} 
	}
 }
	printf("subs[0]=%d; subs[1]=%d; subs[2]=%d\n", subs[0], subs[1], subs[2]);
}
//Функция отправки SMS
void sim800_send_sms(const char* phone_number, const char* text) {
    char cmd[64];
		uart_flush_input(UART_NUM_1); // Чистим старые данные
    // 1. Команда инициализации отправки SMS
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", phone_number);
    uart_write_bytes(UART_NUM_1, cmd, strlen(cmd));
    vTaskDelay(pdMS_TO_TICKS(100)); // Ожидание символа приглашения '>'
    // 2. Отправка самого текста сообщения
    uart_write_bytes(UART_NUM_1, text, strlen(text));
    // 3. Отправка символа Ctrl+Z (ASCII код 26) для подтверждения отправки
    char ctrl_z = 26;
    uart_write_bytes(UART_NUM_1, &ctrl_z, 1);    
    //vTaskDelay(pdMS_TO_TICKS(3000)); // Ожидание завершения отправки сетью
    //ESP_LOGI(TAG, "SMS отправлено.");
}
//Программа выполнения задачи получения/отправки SMS через GSM-модем
void sms_modem(void *arg) 
{
	//Инициализация UART1:
	uart_config_t uart_config = {
		.baud_rate = 115200, // Скорость по умолчанию для большинства модулей
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
  		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TX_PIN, RX_PIN, -1, -1);
	uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
	// Настройте временный буфер для входящих данных
	uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
	//Признак регистрации модема в сотовой сети
	uint8_t f_reg = 0;// не зарегистрирован
	 //ПОСЛЕДОВАТЕЛЬНОСТЬ АТ-команд по тестированию модуля GSM и установке режима работы
	printf("Modem GSM, ready?\n");
	uart_flush_input(UART_NUM_1); // Чистим старые данные			
	send_at_com("AT");//готовность модуля?
	read_from_modem(data);
	send_at_com("AT+CSQ");//мощность сигнала, дцб
	read_from_modem(data);
	send_at_com("AT+CCID");//номер sim карты?
	read_from_modem(data);
	send_at_com("AT+COPS?");//имя подключенного оператора сети
	read_from_modem(data);
	send_at_com("AT+CMGF=1");//установка текстового режима SMS
	read_from_modem(data);
	send_at_com("AT+CSCS=\"GSM\""); // Кодировка текста стандартная GSM
	read_from_modem(data);
		//Модем сохраняет SMS на  Sim-карту
	send_at_com("AT+CNMI=0,0,0,0,0"); 
	read_from_modem(data);
	//Удалить все SMS-сообщения с SIM-карты	
	send_at_com("AT+CMGD=1,4");
	vTaskDelay(pdMS_TO_TICKS(1000));//задержка в 1с на полную очистку
		read_from_modem(data);
	//Чтение из nvs памяти строки подписки полученной из sms
	// и формирование кодов подписки: subs[3]		
		nvs_handle_t my_handle;
		//инициализация раздела флеш-памяти user_nvs
		esp_err_t ret = nvs_flash_init_partition("user_nvs");
		if (ret != ESP_OK) {
  			printf("Failed to init NVS user_nvs\n");    
		} 
		//открытие раздела флеш-памяти "user_nvs" с пространством имен "gsm_modem"
		ret = nvs_open_from_partition("user_nvs", "gsm_modem",
        		NVS_READONLY, &my_handle);
		if (ret != ESP_OK) {
  			printf("Failed open user_nvs/gsm_modem\n");			
		} else {
			char subs[20];
		//чтение строки подписки
		size_t required_size;
		nvs_get_str(my_handle, "subs", NULL, &required_size);
		char *s = malloc(required_size);
		nvs_get_str(my_handle, "subs", s, &required_size);
		strcpy(subs, s);		
		parsing_subs(subs);//разбор строки, формирование subs[3]
	 }
	//Основной цикл задачи
	while (1) {	
	//Проверка регистрации в сотовой сети модема
  	send_at_com("AT+CREG?");//регистрация в сети?
	//чтение данных через UART
	int len = uart_read_bytes(UART_NUM_1,data,(BUF_SIZE - 1),10);//ожидание 10 тик
	if(len > 0) {//если что-то получено, то в конец строки 0
	data[len] = '\0';
	printf("Modem GSM: %s", data);
	//если в данных после "," 1 - модем регистрирован в сотовой сети
		for(int i=0; i<len; i++) {
		if(data[i]==',') {
		if(data[i+1]=='1') {
			printf("This modem is registered!\n");
			if(f_reg==0) {
				printf("Send a message about modem GSM connection\n");
				//посылка sms если есть подписка
				if(subs[0]!=0) {
				sim800_send_sms(NUM_TEL, "Modem GSM connection!");
				}
				f_reg = 1;
			}				
		} else {
			printf("Not registered!\n");
			f_reg = 0;
		}
		break;
	  }
	}
  }
	//Чтение входящих SMS
		if(f_reg==1) {
			uart_flush_input(UART_NUM_1); // Чистим старые данные
			send_at_com("AT+CMGL=\"REC UNREAD\"");//Вывести список всех SMS из памяти модуля
			int len = uart_read_bytes(UART_NUM_1,data,(BUF_SIZE - 1),20);//ожидание 20 тик
			if(len>0 && data[len-1]=='\n') {//если что-то получено, то в конец строки 0
			data[len] = '\0';
			printf("Modem GSM: %s", data);
			char text_uart[BUF_SIZE];
			for(int n=0; n<=len; n++) {
				text_uart[n] = data[n];
			}
		//В принятом сообщение указывается: номер телефона, время, само сообщение.Типа:
		/*
		+CMGL: 1,"REC UNREAD","+7XXXXXXXXXX","","26/07/15,13:05:38+12"
		ky-ky!
		*/
		//Считаем, что за цикл приходит не более 1 сообщения.
		//Выделяем номер телефона и текст SMS, разделив строку по символу переноса
		// и убрав пустые строки и заголовок "AT+CMGL="REC UNREAD""
		  	char phone[20] = {0};
		  	char *text_sms;			
			uint8_t ft = 0;//флаг признака строки с номером телефона
			text_sms = strtok(text_uart, "\n");//указатель на следующую подстроку
			do {
				text_sms = strtok('\0', "\n");//указатель на следующую подстроку
				if(text_sms && text_sms[0]!='\r' && ft==1) {
					//эта строка следующая после номера и времени - текст sms
					break;
				}
        //если строка не пустая; начинается с "+CMGL:"; нет признака строки с тел.
				if(text_sms && strncmp(text_sms, "+CMGL:", 6)==0 && ft==0) {					
					// Извлекаем номер телефона, находящийся в первых кавычках
					// Регулярное выражение считывает до 12 символов
					sscanf(text_sms, "+CMGL: 1,\"REC UNREAD\",\"%12s", phone);
					ft = 1;					
				}
			 } while(text_sms);
    if(ft==1) {//получено сообщение sms
			printf("Phone: %s\n", phone);
			printf("Text SMS: %s\n", text_sms);
			//Удалить все SMS-сообщения с SIM-карты	
			send_at_com("AT+CMGD=1,4");	
			read_from_modem(data);	
			//ДАЛЕЕ ОБРАБОТКА ПОСТУПИВШЕГО СООБЩЕНИЯ
			if(strcmp(phone, NUM_TEL)==0) {//номер телефона соответствует
				if(strncmp(text_sms, "com", 3)==0) {//команда изменения
				//разбор sms на topik, данные, если есть символ ":" и запуск
				char *sim = strchr(text_sms, ':');
				if(sim!=0){					
					char *dat = NULL;
					dat = sim + 1;//Указываем dat на следующий символ после двоеточия    
    				// Удаляем пробел в начале данных, если он есть
    				if (*dat == ' ') {
        				dat++;
    				}
					//удаляем символы "\n", "\r". Обычно подставляются автоматически
					//при отсылке sms.
					size_t size = strlen(dat);
					if(size>0&&(dat[size-1]=='\n'||'\r')) {
						dat[size-1] = '\0';
					}									
					char *topic = strtok(text_sms, ":");
					int topic_len = strlen(topic);
					int dat_len = strlen(dat);
					//printf("Dat: %s, dat_len: %d\n", dat, dat_len);					
					mqtt_msg_in(topic_len, topic, dat_len, dat);
				}
			 }
			 if(strncmp(text_sms, "inf", 3)==0) {//команда подписки					
				//запись в nvs память
				nvs_handle_t my_handle;
					//инициализация раздела флеш-памяти user_nvs
					esp_err_t ret = nvs_flash_init_partition("user_nvs");
					if (ret != ESP_OK) {
						printf("Failed to init NVS user_nvs\n");    
					} 
				//открытие раздела флеш-памяти "user_nvs" с пространством имен "gsm_modem"
				nvs_open_from_partition("user_nvs", "gsm_modem",
    					NVS_READWRITE, &my_handle);							
				//запись строки подписки
				nvs_set_str(my_handle, "subs", text_sms);
				//отсылка sms подтверждения
				char s[40] = "Received: ";
				strcat(s, text_sms);
				sim800_send_sms(NUM_TEL, s);
				parsing_subs(text_sms);//разбор строки подписки и формирование subs[3]				
			 }
			}
     }								
    }	  				
	}
	//Посылка сформированного сообщения от сети
	if(flag_msg && f_reg) {//если есть флаг сообщения и модуль зарегистрирован.
		char msg_sms[256] = {0};
		strcat(msg_sms, topic_on_sms);
		strcat(msg_sms, ":");
		strcat(msg_sms, data_on_sms);
	//Расшифровка сообщения			
		//разделить строку на части
		char num[3];
		char type[4];
		char theme[2];
		char ordinal[3];
		char value[6];
		char s[40];
		strcpy(s, msg_sms);//копирование
		char *p;
		p = strtok(s, "/:");
		p = strtok('\0', "/:");
		strcpy(num, p);
		p = strtok('\0', "/:");
		strcpy(type, p);
		p = strtok('\0', "/:");
		strcpy(theme, p);
		p = strtok('\0', "/:");
		strcpy(ordinal, p);
		p = strtok('\0', "/:");
		strcpy(value, p);
		//printf("num: %s, type: %s,theme: %s, ordinal: %s, value: %s\n",
		//	 num, type, theme, ordinal, value);
		strcat(msg_sms, "\ndecoding: info/");
		//открыть nvs-память для устройства и прочитать наименования
		uint8_t num_d = htol(num);
		uint16_t typ = htol(type);
		uint8_t ord = atoi(ordinal);
		char d[40] = "\0";
		char *str_name;		
		//наименование устройства
		str_name = change_profile_nvs(num_d, 0, 'n', 4, d, 0);
		strcat(msg_sms, str_name);	strcat(msg_sms, "/");		
		//местоположение
		str_name = change_profile_nvs(num_d, 0, 'n', 5, d, 0);
		strcat(msg_sms, str_name);	strcat(msg_sms, "/");
		//функция
			str_name = change_profile_nvs(num_d, typ, 'n', 3, d, 0);		
		strcat(msg_sms, str_name);	strcat(msg_sms, "/");		
		//наименование параметра
		str_name = change_profile_nvs(num_d, typ, 't', ord, d, 0);
		strcat(msg_sms, str_name);	strcat(msg_sms, ":");
		//значение
		if(!strcmp(value, "0")) {
			strcpy(value, "off");
		}
		if(!strcmp(value, "1")) {
			strcpy(value, "on");
		}
		strcat(msg_sms, value);
		printf("MESSAGE SMS: %s\n", msg_sms);
		//посылка sms если есть подписка
		if(subs[0]!=0) {
			sim800_send_sms(NUM_TEL, msg_sms);
		}		
		flag_msg = 0;
	}
	vTaskDelay(pdMS_TO_TICKS(TIME_SMS));
  }
	vTaskDelete(NULL);
}
//Данная функция вызывается для создания задачи для ОС
void init_sms_modem()
{		
	TaskHandle_t TaskHandleSMSmodem = NULL;
	xTaskCreate(sms_modem,"SMSmodem",4096,NULL,SMS_MODEM_PRIO,&TaskHandleSMSmodem);
	// Проверим, создалась ли задача
	if(TaskHandleSMSmodem != NULL) {
		printf("TASK SMS MODEM create!\n");
	} else printf("TASK SMS MODEM FAILED!\n");
}