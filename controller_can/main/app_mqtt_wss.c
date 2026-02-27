#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "app_const.h"
#include "app_priv.h"

static const char *TAG = "mqttwss";//для создания журнала с таким заголовком
//Используется режим WebSocket через TLS только с проверкой сертификата сервера
    // CONFIG_WS_OVER_TLS_SERVER_AUTH
    //Сертификат сервера представляется в формате PEM в директории проекта main
    //Чтобы встроить его в двоичный файл приложения, PEM-файлу присваивается имя
    // в CMakeList.txt(component) Переменная EMBED_TEXTFILES wqtt.pem
  //Можно получить сертификат непосредственно от сайта, см. https://kotyara12.ru/iot/ssl-arduino/
    //Сертификат подключается к программе ввиде константы:
extern const uint8_t wqtt_pem_start[]    asm("_binary_wqtt_pem_start");
extern const uint8_t wqtt_pem_end[]    asm("_binary_wqtt_pem_end");
extern uint8_t msg_for_mqtt;//флаг сформированного сообщения для передачи брокеру mqtt
extern char topic_on[40];//строка топика для передачи брокеру mqtt
extern char data_on[200];//строка данных для передачи брокеру mqtt
extern uint8_t flag_cont;//флаг внешнего соединения
        //0 - отсутствие соединения
        //1 - соединение с брокером mqtt
extern uint8_t flag_mode;//флаг режима работы
extern uint8_t array_member;//порядковый номер члена массива в котором хранятся 
                            //локальные номера подчиненных устройств.

// Обработчик событий для MQTT-событий
 static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;//идентификатор сообщения
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_BEFORE_CONNECT://клиент инициализирован
        ESP_LOGI(TAG, "MQTT_EVENT_CLIENT_INIT");
        break;
    case MQTT_EVENT_CONNECTED://клиент установил соединение с брокером
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        //посылка сообщения о начале сессии связи
        msg_id = esp_mqtt_client_publish(client, "inf/status", "on", 0, 1, 1);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        //запрос клиента на подписку топиков команд
        msg_id = esp_mqtt_client_subscribe(client, "com/#", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        flag_cont = 1;//выставить флаг контакта с брокером mqtt 
        gpio_set_level(LED_ALARM, 0);
        array_member = 0;
        flag_mode = 16;//передачи данных профилей устройств  
        break;
    case MQTT_EVENT_DISCONNECTED://клиент разорвал соединение например:
    // из-за недоступности брокера
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        flag_cont = 0;//снять флаг контакта с брокером mqtt
        gpio_set_level(LED_ALARM, 1);
        break;
    case MQTT_EVENT_SUBSCRIBED://Брокер подтвердил запрос клиента на подписку
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED://Брокер подтвердил запрос клиента на отмену подписки
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED://Брокер подтвердил сообщение о публикации клиента        
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA://Клиент получил сообщение о публикации
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        //передача сообщения для анализа
        mqtt_msg_in(event->topic_len, event->topic, event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR://Клиент столкнулся с ошибкой
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        flag_cont = 0;//снять флаг контакта с брокером mqtt
        gpio_set_level(LED_ALARM, 1);
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}
 static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
     int32_t event_id, void *event_data)
{
    //К аргументу, переданному в esp_mqtt_client_register_event, можно обращаться
    // как к handler_args
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    mqtt_event_handler_cb(event_data);
}  
void mqtt_wss_client(void *arg)
{
    //Отображение начальной информации
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    //Установить уровень журнала для заданного тега.
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);    
   
    //Параметры соединения (конфигурация) 
    const esp_mqtt_client_config_t mqtt_cfg = {
     //предоставляется брокером MQTT
        .broker.address.uri = BROKER_URI,//адрес и порт wss
        .broker.verification.certificate = (const char *)wqtt_pem_start,
        .credentials.username = USER_NAME,
        .credentials.client_id = ID_CLIENT,
        .credentials.authentication.password = PASSWORD,        
     //Конфигурация размера буфера клиента
        //.buffer.size = 128,//размер буфера отправки/приема MQTT
        //.buffer.out_size = 1024,//Размер выходного буфера MQTT(1024 по умолч.)
     //Конфигурация, связанная с сетью
        //Ниже заремлено(REM) то , что установлено по умолчанию 4 строки
        //.network.disable_auto_reconnect = false,//Клиент будет переподключаться
                                    // к серверу (при возникновении ошибок/отключении)
        //.network.transport =  MQTT_TRANSPORT_OVER_WSS,//транспортный уровень
        //.network.if_name = WSS,//имя интерфейса
        //.network.reconnect_timeout_ms = 10000,//повторное подключение к серверу
    //К чему две нижеследующие строчки? При их вводе соединение переподключается через 5 с
        //.network.timeout_ms = 4000,//прервать сетевую операцию если за это время не завершена
        //.network.refresh_connection_after_ms = 4000,//Обновить соединение через это значение
     //Конфигурация исходящих сообщений клиента ???
        //.outbox_config.limit = 128,//Максимальный размер исх.сообщений байт
     //Конфигурация, связанная с сеансом MQTT (struct session_t)
        .session.disable_keepalive = false,//не отключение механизма keepalive       
        .session.keepalive = 30,//интервал поддержания активности в секундах
        .session.disable_clean_session = false,//очистка сессии 
        //.session.protocol_ver = MQTT_PROTOCOL_V_3_1,//Версия протокола MQTT по умолчанию
        .session. message_retransmit_timeout = 2000,//тайм-аут для повторной
                                                // передачи неудачного пакета
        //LWT - сообщение последняя воля и завещание(незапланированное отключение)
        .session.last_will.topic = "inf/status", // LWT topic
        .session.last_will.msg = "off", // LWT message
        .session.last_will.qos = 1, // LWT QoS (качество обслуживания)
        .session.last_will.retain = 1, // LWT retain flag (true)(запоминание)
     //Конфигурация клиентской задачи
        //.task.priority = //приоритет задачи
        //.task.stack_size = //размер стека задачи 
     //Конфигурация определенной темы
        //.task.filter =  //Фильтр тем для подписки
        //.task.qos =   // Максимальный уровень QoS подписки
    };
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    //Последний аргумент может использоваться для передачи данных в обработчик
    // событий, в данном примере mqtt_event_handler
esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    esp_mqtt_client_start(client);
//Основной цикл задачи
    while (1) {
        //клиент отправляет брокеру сообщение
        //проверка действительности дескриптора клиента
        if((client != NULL)&&(msg_for_mqtt)) {
        int msg_id = esp_mqtt_client_publish(client, topic_on, data_on, 0, 1, 1);
        msg_for_mqtt = 0;//сброс флага
        printf("Topic_on: %s\n", topic_on);
        printf("data_on: %s\n", data_on);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        }
       vTaskDelay(1);//задержка, 1 тик = 10мс
    }
    vTaskDelete(NULL);
}
//Создание задачи: mqtt client через соединение websocket secure
void mqtt_wss_client_task()
{
    TaskHandle_t TaskHandleMqttWssclient = NULL;
    xTaskCreate(mqtt_wss_client, "MqttWssClient", 4096, NULL, MWC_TSK_PRIO, &TaskHandleMqttWssclient);
    // Проверим, создалась ли задача
  if (TaskHandleMqttWssclient == NULL) {
    ESP_LOGE("TASK MQTT wss client", "Failed to task create");
  }    
}
