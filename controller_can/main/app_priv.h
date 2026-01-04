//объявление функций
#pragma once
#include "driver/gpio.h"
#include "esp_err.h"
#include "app_const.h"
#include "esp_eth_driver.h"


//======= объявление используемых функций ============================
void init_GPIO();
void get_DeviceInf();
void init_CAN(gpio_num_t TX,gpio_num_t RX);
void message_t_CAN(uint32_t ext,uint32_t echo,uint32_t ident,uint8_t len, uint64_t m_ident);
long htol(const char * ptr);
void app_driver_init();
void remote_frame(uint32_t echo,uint32_t ident);
void form_cadr(uint8_t num_cadra, char *str);
void form_many_cadr(uint8_t num_cadr, char *str);
void read_inf_service();
int true_data();
void init_list_slaves();
void init_inf_service_slave(const char *slave_n);
void read_prof_slave(const char *slave_n);
void read_list_slaves();
void init_type_210(uint8_t num_tab);
void read_service_type(uint8_t num);
void init_type_0A0(uint8_t num);
void event_processing(uint64_t event);
void make_parametr(uint16_t kod_make, uint16_t value_par);
esp_err_t eth_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out);
void mqtt_wss_client_task();
void mqtt_msg_in(int topic_len, char *topic, int data_len, char *data);
int type_210_make(char theme, uint32_t ordinal, char *datas, uint8_t num);
char *change_profile_nvs(uint8_t num_div, uint16_t type,
        char theme, uint8_t ordinal, char *data, uint8_t wr);
void com_processing(uint32_t identifier, uint16_t val_par, uint8_t n_cadr);
void event_on_mqtt(uint32_t identifier, uint8_t num_par, uint16_t val_par);
void name_script_processing(uint16_t kod_type, uint8_t data_len, uint64_t data);