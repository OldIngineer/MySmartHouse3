// ПРОГРАММА УПРАЛЕНИЯ РАБОТОЙ GSM-МОДЕМА В РЕЖИМАХ SMS И
//ПЕРЕДАЧИ ЦИФРОВЫХ ДАННЫХ ЧЕРЕЗ ПРОТОКОЛЫ PPP, WSS БРОКЕРУ MQTT
//подключение пользовательских значений
#include "app_const.h"
//объявление используемых функций из других подключенных файлов *.
#include "app_priv.h"

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_event.h"
#include "ping/ping_sock.h"

extern uint8_t flag_cont;//флаг внешнего соединения
extern TaskHandle_t TaskHandleMqttWssclient;//дескриптор задачи mqtt wss client
extern TaskHandle_t TaskHandleSMSmodem;//дескриптор задачи sms modem
extern uint8_t flag_msg;//флаг сформированного сообщения в формате mqtt
extern uint8_t f_reg;//Признак регистрации модема в сотовой сети;

static const char *TAG = "GSMmodem";//для создания журнала с таким заголовком
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;
esp_err_t err;
uint32_t loss_percent = 0;//потеря пакетов в % от сессии пинг
uint8_t task_cycle_counter = 0;//счетчик циклов задачи
//изменение в канале ррр
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        //Событие прерывания пользователем от esp-netif
        esp_netif_t **p_netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", *p_netif);
    }
}
//событие получения IP адреса
static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}
// Функция, которая автоматически вызовется по окончании серии пингов
static void test_ping_end_cb(esp_ping_handle_t hdl, void *args) {
  uint32_t transmitted = 0;
  uint32_t received = 0;
  uint32_t total_time_ms = 0;
  
  // Извлекаем количество отправленных запросов (REQUEST)
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
                       sizeof(transmitted));
  // Извлекаем количество полученных ответов (REPLY)
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received,
                       sizeof(received));
  // Извлекаем общую продолжительность теста
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
                         sizeof(total_time_ms)); 
  // Безопасный расчет процента потерь
  if (transmitted > 0) {
    loss_percent = ((transmitted - received) * 100) / transmitted;
  }
  // Обязательно удаляем сессию пинга после завершения, чтобы не было утечки памяти
  esp_ping_delete_session(hdl);                   
}
//Функция Запуск сессии пинг
uint32_t start_my_ping(ip_addr_t target_ip) {
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_ip;
    ping_config.count = 4; // 4 пакетов

    esp_ping_callbacks_t cbs = {
        .on_ping_success = NULL, 
        .on_ping_timeout = NULL,
        .on_ping_end = test_ping_end_cb, // колбэк
        .cb_args = NULL
    };

    esp_ping_handle_t ping_handle;
    esp_ping_new_session(&ping_config, &cbs, &ping_handle);
    esp_ping_start(ping_handle);
    return loss_percent;
}
// Функция программного сброса модема и программы
void reset_modem_and_program(esp_modem_dce_t *dce) {
    ESP_LOGE(TAG, "Reset modem");
  // Сброс модема и если не удается, то сброс по выводу PWX
  err = esp_modem_reset(dce);
  ESP_LOGE(TAG, "result command: esp_modem_reset - %d %s",
  		 err, esp_err_to_name(err)); 
  if(err != ESP_OK) {
    gpio_reset_pin(RESET_MODEM);                      
    gpio_set_direction(RESET_MODEM, GPIO_MODE_OUTPUT_OD);//открытый сток
    gpio_set_level(RESET_MODEM, 0); 
    vTaskDelay(pdMS_TO_TICKS(3000));//ждем 3 секунды
    gpio_set_level(RESET_MODEM, 1);
    vTaskDelay(pdMS_TO_TICKS(3000));//ждем 3 секунды
    //Инициализация UART1
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
  // временный буфер для входящих данных                       
  uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
  uart_flush_input(UART_NUM_1); // Чистим старые данные                          
  //Проверка готовности модема, команда АТ                                  
  uart_write_bytes(UART_NUM_1, "AT", 2);                                
  uart_write_bytes(UART_NUM_1, "\r\n", 2);       
  //чтение данных через UART                                                    
  int len = uart_read_bytes(UART_NUM_1,data,(BUF_SIZE - 1),10);//ожидание 10 тик
  if(len > 0) {//если что-то получено, то в конец строки 0                      
    data[len] = '\0';                                                             
    printf("Modem GSM: %s\n", data);
    //printf("data[len-4]=%c; data[len-3]=%c\n", data[len-4], data[len-3]);
    //если модем не отвечает, то повторный сброс по выводу PWX
    if(data[len-3]!='K'&&data[len-4]!='O') {
      gpio_set_level(RESET_MODEM, 0);
      vTaskDelay(pdMS_TO_TICKS(3000));//ждем 3 секунды
      gpio_set_level(RESET_MODEM, 1);  	 
    }
  } 
  ESP_LOGE(TAG, "Restart program");
  vTaskDelay(pdMS_TO_TICKS(3000));//ждем 3 секунды
  esp_restart();//перезапуск программы
  }		
}
//функция управления передачи данных через модем: протокол TCP/IP+РРР или sms
void gsm_modem_manager(void *arg)
{
//=====инициализация и регистрация системных компонентов=======
// Инициализировать сетевой интерфейс TCP/IP, он же esp-netif
// (должен вызываться только один раз в приложении)
ESP_ERROR_CHECK(esp_netif_init());
// Создать цикл обработки событий по умолчанию, работающий в фоновом режиме
ESP_ERROR_CHECK(esp_event_loop_create_default());
// Регистрация пользовательских обработчиков событий
ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
								 &on_ip_event, NULL));
ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
								 &on_ppp_changed, NULL));
event_group = xEventGroupCreate();//создается группа событий в FreeRTOS
//===============================================
// Configure the PPP netif 
esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(APN);
esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
//конфигурация DTE
esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_config.uart_config.tx_io_num = TX_PIN;
  dte_config.uart_config.rx_io_num = RX_PIN;
  dte_config.uart_config.rts_io_num = UART_PIN_NO_CHANGE;
  dte_config.uart_config.cts_io_num = UART_PIN_NO_CHANGE;
  dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
  dte_config.uart_config.rx_buffer_size = 4096;
  dte_config.uart_config.tx_buffer_size = 2048;
  dte_config.uart_config.event_queue_size = 30;
  dte_config.task_stack_size = 4096;
  dte_config.task_priority = GSM_MODEM_PRIO;
  dte_config.dte_buffer_size = 4096 / 2;
//Инициализация netif
esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
assert(esp_netif);
// Инициализируем SIM800C
ESP_LOGI(TAG, "Initializing esp_modem for the SIM800 module...");
esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800,
                      &dte_config, &dce_config, esp_netif);
assert(dce);
xEventGroupClearBits(event_group, CONNECT_BIT | DISCONNECT_BIT); 
//Основной цикл задачи
while (1) {
  if(f_reg == 1) {//модем зарегистрирован в сотовой сети
    gpio_set_level(LED_WORK,1);//включить светодиод
  } 
  if(f_reg == 0) {
    gpio_set_level(LED_WORK,0);//выключить светодиод
  }
  if(flag_cont == 0)// внешнего соединения нет
  {
    //проверка установленного режима работы
    esp_modem_dce_mode_t mode;
    mode = esp_modem_get_mode(dce);
    ESP_LOGI(TAG, "Current modem mode: %d", mode);
    if(mode == ESP_MODEM_MODE_COMMAND) {
      task_cycle_counter++;
      if(task_cycle_counter <10) {
        goto end;//переход в конец цикла
      }
    }
    //попытка соединения по РРР по 10 циклу задачи
    if((mode == ESP_MODEM_MODE_COMMAND)&(task_cycle_counter>=10)) {
      task_cycle_counter = 0;
    //проверка качества сигнала
    int rssi, ber;//уровень сигнала и частота ошибок
    err = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if ((err != ESP_OK) | (rssi < 10)) {
      ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s",
					 err, esp_err_to_name(err));
      //проверка готовности модема, команда АТ
      if(esp_modem_at(dce, "AT", NULL, 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Modem not ready, resetting modem");
        reset_modem_and_program(dce);//сброс модема и программы
      }
      goto end;//переход в конец цикла        
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    //Если была создана задача обмена данными через sms, то удаляем её
    if (TaskHandleSMSmodem != NULL) {
      ESP_LOGI(TAG, "Deleting sms modem task");
      vTaskDelete(TaskHandleSMSmodem);
      TaskHandleSMSmodem = NULL;
    }
    // переводим в Data-режим (PPPoS)    
    err =  esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", err);
      reset_modem_and_program(dce);//сброс модема и программы
    }
    xEventGroupClearBits(event_group, CONNECT_BIT | DISCONNECT_BIT);
    ESP_LOGI(TAG, "Waiting for IP address");//ожидание IP адреса
    xEventGroupWaitBits(event_group, CONNECT_BIT | DISCONNECT_BIT, pdFALSE,
			    pdFALSE, pdMS_TO_TICKS(30000));
    //Если модем не подключен к серверу PPP, возвращаемся в командный режим
    if ((xEventGroupGetBits(event_group) & CONNECT_BIT) != CONNECT_BIT) {
      ESP_LOGW(TAG, "Modem not connected server PPP, switching back to the command mode");
      //vTaskDelay(pdMS_TO_TICKS(1000));
      err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
         reset_modem_and_program(dce);//сброс модема и программы
      }
      ESP_LOGI(TAG, "Command mode restored");
      //Если не была создана задача обмена данными через sms, то создаем её
      if (TaskHandleSMSmodem == NULL) {
        init_sms_modem();
      }
      goto end;//переход в конец цикла
     }
    }
    //проверка брокера MQTT
    mode = esp_modem_get_mode(dce);
    ESP_LOGI(TAG, "Current modem mode: %d", mode);
    if(mode == ESP_MODEM_MODE_DATA) {
      ip_addr_t target_ip;
      ipaddr_aton(BROKER_PING, &target_ip);
      uint32_t p = start_my_ping(target_ip);
      vTaskDelay(pdMS_TO_TICKS(6000));//задержка на пингование
      printf("Ping loss: %ld percent\n", p);
      if(p > 25) {//если нет ответа при пинговании
      //if(true) {//если нет ответа при пинговании
      err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);//переходим в командный режим
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        reset_modem_and_program(dce);//сброс модема и программы
      }
      ESP_LOGW(TAG, "No ping broker, switching to the command mode");
      //Если не была создана задача обмена данными через sms, то создаем её
      if (TaskHandleSMSmodem == NULL) {
        init_sms_modem();
      }
      goto end;//переход в конец цикла
    }
    //Если была создана задача обмена данными через sms, то удаляем её
    if (TaskHandleSMSmodem != NULL) {
      ESP_LOGI(TAG, "Deleting sms modem task");
      vTaskDelete(TaskHandleSMSmodem);
      TaskHandleSMSmodem = NULL;
    }
    //если не была создана задача mqtt wss client, то создаем её
    if (TaskHandleMqttWssclient == NULL) {
      ESP_LOGI(TAG, "Starting mqtt wss client task");
      mqtt_wss_client_task();    
    }    
   } else {//это начало подключения модема, после сброса
    err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);//переходим в командный режим
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
      reset_modem_and_program(dce);//сброс модема и программы
    }
    ESP_LOGI(TAG, "Initial connection after reset, command mode");
    //Если не была создана задача обмена данными через sms, то создаем её
    if (TaskHandleSMSmodem == NULL) {
      init_sms_modem();
    }
    task_cycle_counter = 9;
    vTaskDelay(pdMS_TO_TICKS(30000));
    continue;
   }
  } else flag_msg = 0;//сброс флага форм.sms, т.к. данные уже переданы по wss
  end:
  vTaskDelay(pdMS_TO_TICKS(TIME_CYCLE));
 }
  vTaskDelete(NULL);
}
//Создание задачи: обмена данными через gsm-modem
void gsm_modem_manager_task()
{
  TaskHandle_t TaskHandleGsmModemManager = NULL;
  xTaskCreate(gsm_modem_manager, "GsmModemManager", 4096, NULL, GSM_MODEM_PRIO,
                &TaskHandleGsmModemManager);
  // Проверим, создалась ли задача
  if(TaskHandleGsmModemManager == NULL) {
    ESP_LOGE("TASK GSM modem manager", "Failed to task create"); 
  }
}