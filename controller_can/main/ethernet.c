#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "esp_eth_driver.h"
#include "app_const.h"

#define SPI_ETHERNETS_NUM           1//количество подключенных модулей SPI
#define INTERNAL_ETHERNETS_NUM      0//внутренний контроллер Ethernet MAC не используется
#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num) \
    do {                                                   \
        eth_module_config[num].spi_cs_gpio =  nSS;         \
        eth_module_config[num].int_gpio = SPI_nINT;        \
        eth_module_config[num].polling_ms = 0;             \
        eth_module_config[num].phy_reset_gpio = nRESET;    \
        eth_module_config[num].phy_addr = 1;               \
    } while(0)
typedef struct {
    uint8_t spi_cs_gpio;
    int8_t int_gpio;
    uint32_t polling_ms;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
    uint8_t *mac_addr;
}spi_eth_module_config_t;
static const char *TAG = "eth_init";
// указывает, что мы инициализировали службу GPIO ISR
//static bool gpio_isr_svc_init_by_eth = false;
//Инициализация шины SPI
static esp_err_t spi_bus_init(void)
{    
    esp_err_t ret = ESP_OK;
/*
// Установите обработчик GPIO ISR, чтобы иметь возможность обслуживать прерывания отдельных модулей
    ret = gpio_install_isr_service(0);
    if (ret == ESP_OK) {
        gpio_isr_svc_init_by_eth = true;
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
        ret = ESP_OK; //Обработчик ISR уже установлен, так что проблем нет
    } else {
        ESP_LOGE(TAG, "GPIO ISR handler install failed");
        goto err;
    }
*/
    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = SPI_MISO,
        .mosi_io_num = SPI_MOSI,
        .sclk_io_num = SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(1, &buscfg, SPI_DMA_CH_AUTO),
                        err, TAG, "SPI host #%d init failed", 1);

err:
    return ret;
}
//Инициализация Ethernet SPI modules
/*
@param[in] spi_eth_module_config specific SPI Ethernet module configuration
@param[out] mac_out optionally returns Ethernet MAC object
@param[out] phy_out optionally returns Ethernet PHY object
@return
         - esp_eth_handle_t if init succeeded
         - NULL if init failed
*/
static esp_eth_handle_t eth_init_spi(spi_eth_module_config_t *spi_eth_module_config, esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out)
{
    esp_eth_handle_t ret = NULL;
    // Установить общие настройки MAC и PHY по умолчанию
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    // Обновить конфигурацию PHY на основе конкретной конфигурации платы
    phy_config.phy_addr = spi_eth_module_config->phy_addr;
    phy_config.reset_gpio_num = spi_eth_module_config->phy_reset_gpio;
    // Настройка интерфейса SPI для конкретного модуля SPI
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = spi_eth_module_config->spi_cs_gpio
    };
    // Установите конфигурацию MAC для конкретного производителя по умолчанию и создайте новый экземпляр MAC SPI Ethernet
    // и новый экземпляр PHY на основе конфигурации платы USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(1, &spi_devcfg);
    w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
    w5500_config.poll_period_ms = spi_eth_module_config->polling_ms;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    // Инициализируйте драйвер Ethernet по умолчанию и установите его
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config_spi, &eth_handle) == ESP_OK, NULL, err, TAG, "SPI Ethernet driver install failed");
    // Возможно, у модуля SPI Ethernet отсутствует заводской MAC-адрес, установить его вручную.
    if (spi_eth_module_config->mac_addr != NULL) {
        ESP_GOTO_ON_FALSE(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, spi_eth_module_config->mac_addr) == ESP_OK,
                                        NULL, err, TAG, "SPI Ethernet MAC address config failed");
    }
    if (mac_out != NULL) {
        *mac_out = mac;
    }
    if (phy_out != NULL) {
        *phy_out = phy;
    }
    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
//Инициализация драйвера ethernet на основе SPI модуля W5500
/**
 * @краткое описание Инициализации драйвера Ethernet на основе конфигурации Espressif IoT Development Framework
 *
 * @param[out] eth_handles_out  массив для инициализированных дескрипторов драйвера Ethernet
 * @param[out] eth_cnt_out количество инициализированных Ethernet-сетей, не превышающее допустимого значения
 * @return
 * - ESP_OK в случае успешного выполнения
 * - ESP_ERR_INVALID_ARG при передаче недопустимых указателей
 * - ESP_ERR_NO_MEM, когда нет памяти, которую можно выделить для массива данных, обрабатываемых драйвером Ethernet
 * - ESP_FAIL при любом другом сбое
 */
esp_err_t eth_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out)
{
    esp_err_t ret = ESP_OK;
    esp_eth_handle_t *eth_handles = NULL;
    uint8_t eth_cnt = 0;
    ESP_GOTO_ON_FALSE(eth_handles_out != NULL && eth_cnt_out != NULL, ESP_ERR_INVALID_ARG,
            err, TAG, "invalid arguments: initialized handles array or number of interfaces");
    eth_handles = calloc(SPI_ETHERNETS_NUM + INTERNAL_ETHERNETS_NUM, sizeof(esp_eth_handle_t));
    ESP_GOTO_ON_FALSE(eth_handles != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");
    ESP_GOTO_ON_ERROR(spi_bus_init(), err, TAG, "SPI bus init failed");
    // // Инициализирует конкретную конфигурацию модуля SPI Ethernet (№1)
    spi_eth_module_config_t spi_eth_module_config[SPI_ETHERNETS_NUM] = { 0 };
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
    // У модулей SPI Ethernet могут отсутствовать заводские MAC-адреса, поэтому используйте
    // адреса, настроенные вручную.
    // Здесь используется локально управляемый MAC-адрес, полученный из базового
    // MAC-адреса ESP32x.
    // Обратите внимание, что локально управляемый диапазон OUI следует использовать только
    // при тестировании в локальной сети, находящейся под вашим контролем!
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ESP_GOTO_ON_ERROR(esp_efuse_mac_get_default(base_mac_addr), err, TAG, "get EFUSE MAC failed");
    uint8_t local_mac_1[ETH_ADDR_LEN];
    esp_derive_local_mac(local_mac_1, base_mac_addr);
    spi_eth_module_config[0].mac_addr = local_mac_1;
    for (int i = 0; i < SPI_ETHERNETS_NUM; i++) {
        eth_handles[eth_cnt] = eth_init_spi(&spi_eth_module_config[i], NULL, NULL);
        ESP_GOTO_ON_FALSE(eth_handles[eth_cnt], ESP_FAIL, err, TAG, "SPI Ethernet init failed");
        eth_cnt++;
    }
    *eth_handles_out = eth_handles;
    *eth_cnt_out = eth_cnt;
    return ret;
    err:
    free(eth_handles);
    return ret;
}