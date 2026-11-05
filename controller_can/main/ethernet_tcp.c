//Создание транспортного уровня TCP/IP для передачи данных по Ethernet

//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include <esp_log.h>
#include "driver/gpio.h"
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"
//Относится к Ethernet
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"

static const char *TAG = "ethernet";
// Обработчик событий для Ethernet-событий 
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    //  можно получить дескриптор драйвера ethernet из данных события
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        gpio_set_level(LED_WORK,1);//включить светодиод
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        gpio_set_level(LED_WORK,0);//выключить светодиод
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}
// Обработчик событий для IP_EVENT_ETH_GOT_IP
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;


    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}
//функция инициализации канала передачи данных по Ethernet протокол TCP/IP
void ethernet_tcp_init(void)
{
	//Инициализация Ethernet driver
	uint8_t eth_port_cnt = 0;
	esp_eth_handle_t *eth_handles;
	//ESP_ERROR_CHECK(eth_init(&eth_handles, &eth_port_cnt));
	if((eth_init(&eth_handles, &eth_port_cnt)) != ESP_OK) {
    gpio_set_level(LED_ALARM,1);//включить светодиод
	}
	// Инициализировать сетевой интерфейс TCP/IP, он же esp-netif
	// (должен вызываться только один раз в приложении)
	ESP_ERROR_CHECK(esp_netif_init());
	// Создать цикл обработки событий по умолчанию, работающий в фоновом режиме
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *eth_netifs[eth_port_cnt];
	esp_eth_netif_glue_handle_t eth_netif_glues[eth_port_cnt];
	// Создать экземпляр esp-netif для Ethernet
 // Используйте ESP_NETIF_DEFAULT_ETH, когда используется только один интерфейс Ethernet
 // и вам не нужно изменять параметры конфигурации asp-netif по умолчанию.   
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netifs[0] = esp_netif_new(&cfg);
    eth_netif_glues[0] = esp_eth_new_netif_glue(eth_handles[0]);
    // Подключите драйвер Ethernet к стеку TCP/IP
    ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[0], eth_netif_glues[0]));
	// Регистрация пользовательских обработчиков событий
	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
	// Запустить конечный автомат драйвера Ethernet
	for (int i = 0; i < eth_port_cnt; i++) {
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
	}    
	vTaskDelay(pdMS_TO_TICKS(WAIT_TAKE_IP));//задержка на получение адреса Ip     
}