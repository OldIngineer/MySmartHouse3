//подключение пользовательских значений
#include "app_const.h"
#include "sdkconfig.h"
//подключение общих программ
#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"//задачи
#include "freertos/semphr.h"//семафоры
#include "freertos/queue.h"//очередь запроса
#include "esp_err.h"
#include <esp_log.h>
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "soc/twai_periph.h" // For GPIO matrix signal index
//для создания NVS - библиотеки энергонезависимого хранилища ( NVS ) 
//предназначена для хранения пар ключ-значение во флэш-памяти
#include <nvs_flash.h>
//объявление используемых функций из других подключенных файлов *.с
#include "app_priv.h"
//подключение управления сторожевым таймером 
#include "esp_task_wdt.h"
#include "driver/twai.h"
#include "hal/twai_types.h"
//Относится к Ethernet
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "sdkconfig.h"
//управление таймером
//#include "driver/timer.h"

//==========объявление глобальных переменных =========================
char number[3];//массив в котором хранится локальный номер устройства
char obey[7];//массив в котором хранится подчиненность устройства
uint32_t number_master;//ячейка в которой хранится локальный номер master
uint8_t flag_mode = 0;//флаг режима работы
    //режим ожидания =0;
    //признак выдачи в сеть идентификатора сканирования =1;
    //признак получения мастером откликов от подчиненных устройств сети
        //после выдачи в сеть идентификатора сканирования =2;
    //признак для чтения информационного сервиса подключенных устройств =3;
    //признак передачи информационного сервиса =4;
    //признак наличия команды из вне системы =5;
    //признак получения сценария/имени =6;
    //признак отправки в CAN сценария/имени =7;
    //признак режима передачи данных инф.профилей устройств =8;
    //признак режима передачи данных типа функциональности устройств =9;
    //признак изменение контролируемых сигналов на входе устройства = 10;
    //подтверждение исполнения команды от сценария или от мастера = 11, 12;
    //признак чтения сервиса подключенного к сети устройства = 13;
    //признак ожидания/передачи данных от запрашиваемого сервиса = 14;
    //признак запроса данных сервисов подчиненных устройств = 15;
		//признак режима передачи данных типов функциональности всех устройств сети=16;

uint8_t number_slaves[MAX_MEMBERS];//массив в котором хранятся локальные номера
         //подчиненных устройств, а в нулевой ячейке число подключенных устройств
uint8_t array_member = 1;//порядковый номер члена массива в котором хранятся 
        //локальные номера подчиненных устройств. Это для полного чтения профилей
uint8_t byfer_can[256][9];// двухмерный массив в котором хранятся данные
            // для обмена в сети CAN, а в ячейке:
            // byfer_can[0][0] число кадров данных;
            // byfer_can[0][1] признак окончания данных =1;
uint8_t number_cadr;//номер передаваемого/принимаемого кадра данных CAN в пакете
uint8_t retry;//счетчик повторов передачи данных в сети CAN
//Пространство имен устройств в nvs-памяти
const char namespase[64][10] = {"net_dev","device1","device2","device3","device4","device5",
    "device6","device7","device8","device9","device10","device11","device12",
    "device13","device14","device15","device16","device17","device18","device19",
    "device20","device21","device22","device23","device24","device25","device26",
    "device27","device28","device29","device30","device31","device32","device33",
    "device34","device35","device36","device37","device38","device39","device40",
    "device41","device42","device43","device44","device45","device46","device47",
    "device48","device49","device50","device51","device52","device53","device54",
    "device55","device56","device57","device58","device59","device60","device61",
    "device62","device63"};
//Пространство имен для записи сервиса функциональности устройства
const char namespase_type[9][6] = {"d_inf", "type1", "type2", "type3", "type4",
     "type5", "type6", "type7", "type8"};
//Пространство имен для записи величины параметров
const char namespace_value_par[9][10] = {"","value1p","value2p","value3p",
    "value4p","value5p","value6p","value7p","value8p"};
//Пространство имен для записи имен сервиса
const char namespace_name[9][6] = {"","name1","name2","name3","name4","name5",
    "name6","name7", "name8"};
//Пространство имен для записи имен параметров
const char namespace_name_par[9][7] = {"","name1p","name2p","name3p","name4p",
    "name5p","name6p","name7p","name8p"};
//Пространство имен для записи сценариев
const char namespace_script[17][9] = {"","script1","script2","script3","script4",
    "script5","script6","script7","script8","script9","script10","script11",
    "script12","script13","script14","script15","script16"};
//Таблицы для обслуживания сервисов типа устройства
    //первый индекс - порядковый номер типа функциональности как он указан в инф.сервисе 
uint32_t tab_param[8][9];//таблица параметров:младшие 2 байта - величина, старшие -
      //код обработки параметра, код типа+номер параметра (для функции make_parametr) 
      //tab_param[*][0] - код типа функциональности
uint64_t tab_event[8][17];//таблица событий
uint32_t tab_make[8][17];//таблица исполнения
uint16_t tabl_type[64][9];//таблица ячейки которой содержат код типа функциональности
        //для 63 устройств сети по 9 типам (включая инф. сервис (0))
        //в порядке перечисления в инф.сервисе устройства
int8_t ord_tab_type;//порядковый номер текущего кода типа (для передачи данных сервисов, мастер)
uint16_t type_func;//код сервиса для отправки в TWAI подчиненым устройством
uint32_t param_change;//дискриптор флага изменения параметра
uint32_t buf_proof[2];//буфер подтверждения хранения события изменения параметра для передачи в CAN,
                    // 0 ячейка код типа функциональности,
                    // 1 ячейка номер параметра и его величина
uint32_t buf_com_can[3];//буфер сформированной команды для CAN полученной из вне
                    // 0 ячейка 2 ст. байта - тип функциональности, 2 мл. байта лок.номер устройства
                    // 1 ячейка содержимое кадра, младшие байты
                    // 2 ячейка (старший байт - номер кадра) содержимое кадра, старшие байты
uint8_t err = 1;//для внесения ошибок при отладке 
//----------------- MQTT ---------------------------------------------
uint8_t msg_for_mqtt = 0;//флаг сформированного сообщения для передачи брокеру mqtt
char topic_on[40];//строка топика для передачи брокеру mqtt
char data_on[200];//строка данных для передачи брокеру mqtt
uint8_t flag_cont = 0;//флаг внешнего соединения
        //0 - отсутствие соединения
        //1 - соединение с брокером mqtt
//====================================================================
uint8_t ordinal;//порядковый номер (параметра, сценария, имени);
uint8_t sum_par = 8;//сумма передаваемых параметров
uint8_t flag_event = 1;//флаг разрешения формирования событий в CAN от датчиков

//объявление дескрипторов задач
TaskHandle_t TaskHandleTwaiReceive = NULL;
TaskHandle_t TaskHandleTwaiTransmit = NULL;
TaskHandle_t TaskHandleControl = NULL;
//перечисление для приема сообщений
typedef enum {
    RX_RECEIVE,
    RX_RECEIVE_TYPE,
} rx_task_action_t;
//структура передаваемая в очередь для передачи в CAN
typedef struct {
    enum {
        TX_REVIEW_RESP,
        TX_REVIEW,
        TX_QUERY_SERVICE,
        TX_ANSWER_SERVICE,
        TX_CHANGE_PARAMETR,
        TX_COM_CADR,
        TX_NAME_SCRIPT,
        TX_EVENT_YES,
        TX_EVENT_NO
    } tx_task_action;//перечисление для передачи сообщений
    uint8_t local_number;
    uint16_t code_type;
    uint8_t number_cadr;
    uint8_t num_param;
    uint16_t value_param;
} tx_message_t;

//объявление очереди запроса
static QueueHandle_t tx_task_que;
static QueueHandle_t rx_task_queue;
//объявление семафоров
static SemaphoreHandle_t ctrl_task_sem;
//=====Задачи для ОС==============================================
//задача чтения сообщений передаваемых по сети CAN
static void twai_receive_task(void *arg)
{    
    while (1) //бесконечный цикл
    {        
        rx_task_action_t action;
        //ожидание внешних данных из очереди задачи, приостановка задачи
        xQueueReceive(rx_task_queue, &action, portMAX_DELAY);
     if (action == RX_RECEIVE) 
     {
            //Прочитать сообщение из интерфейса CAN
            twai_message_t rx_msg;
            //блокировка на время получения сообщения                
                twai_receive(&rx_msg, pdMS_TO_TICKS(WAIT_CAN/BAUD_RATE));
                twai_clear_receive_queue ();//очистить очередь приема

        if((rx_msg.extd)&&(rx_msg.data_length_code == 3)&&
            (flag_mode==0)&&!rx_msg.rtr) { 
        uint64_t id =  rx_msg.identifier;      
        uint64_t event = rx_msg.data[0] + (rx_msg.data[1] << 8) + 
                (rx_msg.data[2] << 16) + (id << 24);                
        uint32_t ident = rx_msg.identifier;
        uint16_t val_par = rx_msg.data[0] + (rx_msg.data[1] << 8);
        uint8_t n_cadr = rx_msg.data[2];

        //если старший бит идентификатора =1 (изменение), 0 в 18р (признак записи) 
        if((rx_msg.identifier&0x10000000)&&!(rx_msg.identifier&0x20000)) {        
           // вызов функции изменения параметра
           com_processing(ident, val_par, n_cadr);
        }
        //если старший бит идентификатора =0, 1 в 18р (признак чтения) 
        if(!(rx_msg.identifier&0x10000000)&&(rx_msg.identifier&0x20000)) {
            event_processing(event);//обработка события
        }
        }       
        //ЕСЛИ ПОДЧИНЕННОСТЬ SLAVE
        if(strcmp(obey,"master")!=0) 
        {
            printf("Identifier: %ld\n", rx_msg.identifier);
            //пришел идентификатор обзора сети  
            if ((rx_msg.identifier == ID_MASTER_REVIEW)&&(rx_msg.extd==0)&&
								(rx_msg.data_length_code == 1)&&!rx_msg.rtr) { 
                //запомнить локальный номер "master"
                number_master = (rx_msg.data[2]<<16) + (rx_msg.data[1]<<8) + rx_msg.data[0];
                flag_event = 0;//запрет формирования событий от датчиков 
                    flag_mode = 1;//признак сканирования
                }
            //пришел идентификатор запроса чтения инф.сервиса
            if((rx_msg.extd==1)&&(rx_msg.identifier==
                (htol(number)+(ID_MASTER_REVIEW<<18)))&&rx_msg.rtr&&
                (rx_msg.data_length_code == 0)) {
                    flag_mode = 3;//признак отправки инф.сервиса
                }
            //пришел идентификатор разрешения формирования событий от датчиков
            if((rx_msg.identifier == ID_MASTER_REVIEW)&&(rx_msg.extd==0)&&
				(rx_msg.data_length_code == 0)&&rx_msg.rtr) {
                    flag_event = 1;
                }
            //пришел идентификатор запрета формирования событий от датчиков
            if((rx_msg.identifier == ID_MASTER_REVIEW)&&(rx_msg.extd==0)&&
	                (rx_msg.data_length_code == 0)&&!rx_msg.rtr) {
                flag_event = 0;
            }
            //пришел идентификатор запроса чтения сервиса
            if((rx_msg.extd==1)&&(rx_msg.identifier&0x10000000)&&
                ((rx_msg.identifier>>18)!=ID_MASTER_REVIEW)&&
                ((rx_msg.identifier&0xFF)==htol(number))&&rx_msg.rtr&&
                (rx_msg.data_length_code == 0)) {
                    //printf("ky-ky\n");
                    type_func = (rx_msg.identifier>>18) - 0x400;//код сервиса
                    flag_mode = 13;//признак отправки сервиса
                }
            //пришел идентификатор записи имени/сценария (данных > 3)
            if(rx_msg.extd&&(rx_msg.data_length_code > 3)&&(flag_mode==0)&&
                ((rx_msg.identifier&0xFF)==htol(number))&&
            //если старший бит идентификатора =1 (изменение), 0 в 18р (признак записи)
            (rx_msg.identifier&0x10000000)&&!(rx_msg.identifier&0x20000)) {                
            uint16_t kod_type = (rx_msg.identifier >> 18) - 0x400;//код функиональности
            uint64_t data = 0;//полученные данные в одну ячейку
            for(int i=0; i<rx_msg.data_length_code; i++) {
                uint64_t ky = rx_msg.data[i];
                data = data + (ky<<(i*8));
            }
                //printf("data:%lld\n", data);
                //вызов функции обработки
                name_script_processing(kod_type,rx_msg.data_length_code,data);
            }
                //printf("Flag mode: %d, ident: %ld\n",flag_mode,rx_msg.identifier);
        }
        //ЕСЛИ ПОДЧИНЕННОСТЬ МАСТЕР
        if(strcmp(obey,"master")==0) 
        {
            //если признак ответов от сканирования, расширенный идентификатор 
            //с базовой частью "ID_MASTER_REVIEW"+ признак чтения
            if((flag_mode==2)&&(rx_msg.extd==1)&&
                (((ID_MASTER_REVIEW<<1)+1)==(rx_msg.identifier>>17))) {
                uint8_t s_n_s = rx_msg.data[0];
                //внести новый номер в массив
                for(int i=1; i<sizeof number_slaves; i++) {
                    if(s_n_s==number_slaves[i]) break;//уже есть этот номер                     
                    if(number_slaves[i]==0) {
                        number_slaves[i]=s_n_s;//запись
                        number_slaves[0]=i;
                        break;
                    }
                }
            }
            //если признак ожидания данных информационного сервиса, расширенный идентификатор 
            //с базовой частью "ID_MASTER_REVIEW"+ признак чтения + лок.номер
            if((flag_mode==4)&&(rx_msg.extd==1)&&
            ((number_slaves[array_member]+(1<<17)+(ID_MASTER_REVIEW<<18))==
            (rx_msg.identifier))) {
                byfer_can[0][0]++;
                //запись в буфер полученного кадра данных
                for(int i=0; i<8; i++) {
                    byfer_can[byfer_can[0][0]][i] = rx_msg.data[i];
                }//если в конце кадра # это означает окончание приема данных
                if(rx_msg.data[0]=='#') {
                    byfer_can[0][1] = 1;
                }
            }
            //если признак ожидания данных сервиса, расширенный идентификатор 
            //с базовой частью не "ID_MASTER_REVIEW"+ признак чтения + лок.номер
            // + 1 в старшем разряде идентификатора
            if((flag_mode==14)&&(rx_msg.extd==1)&&
            ((rx_msg.identifier>>18)!=ID_MASTER_REVIEW)&&
            ((number_slaves[array_member]+(1<<17))==(rx_msg.identifier&0x3FFFF))&&
            (rx_msg.identifier&0x10000000)) {
                byfer_can[0][0]++;
                //запись в буфер полученного кадра данных
                for(int i=0; i<8; i++) {
                    byfer_can[byfer_can[0][0]][i] = rx_msg.data[i];
                }
    
                 printf("byfer_can[%d]: %d %d %d %d %d %d %d %d\n",
    byfer_can[0][0], byfer_can[byfer_can[0][0]][7],
    byfer_can[byfer_can[0][0]][6], byfer_can[byfer_can[0][0]][5], byfer_can[byfer_can[0][0]][4],
    byfer_can[byfer_can[0][0]][3], byfer_can[byfer_can[0][0]][2], byfer_can[byfer_can[0][0]][1],
    byfer_can[byfer_can[0][0]][0]);
    
                //если в конце кадра # это означает окончание приема данных
                if(rx_msg.data[0]=='#') {
                    byfer_can[0][1] = 1;
                }
            }
            //если режим ожидания и расширенный идентификатор
            if((flag_mode==0)&&(rx_msg.extd==1)) {                
                //сформировать сообщение для MQTT
                uint16_t val_par = (rx_msg.data[1] << 8) + rx_msg.data[0];
                event_on_mqtt(rx_msg.identifier, rx_msg.data[2], val_par);
            }
        }
      }        
        xSemaphoreGive(ctrl_task_sem);//отдать семафор задаче контроля
    }
    vTaskDelete(NULL);
}
//задача отправки сообщений в сеть CAN
static void twai_transmit_task(void *arg)
{
    while (1) {       
        tx_message_t tx_message;//создать экземпляр структуры
        xQueueReceive(tx_task_que, &tx_message, portMAX_DELAY);//прочитать очередь
        if (tx_message.tx_task_action ==TX_REVIEW_RESP) {
            //отправить ответ на сканирование
            uint32_t ext = 1;//расширенный идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint32_t num = htol(number);//преобразование номера
            //выставляемый идентификатор кадра
            uint32_t ident = num + (1<<17) + (ID_MASTER_REVIEW<<18);
            uint8_t len = 1;//длина данных        
            uint64_t data_transmit = num;//данные 
            //printf("s/n: %lld\n",data_transmit);       
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN            
        }
        if (tx_message.tx_task_action ==TX_REVIEW) {
            //Передача кадра данных по интерфейсу CAN - сканирование сети
            uint32_t ext = 0;//базовый идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint32_t ident = ID_MASTER_REVIEW;//идентификатор адресуемых устройств
            uint8_t len = 1;//длина данных        
            uint64_t data_transmit = htol(number);//преобразование локального номера                  
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN            
        }
        if (tx_message.tx_task_action == TX_QUERY_SERVICE) {
            uint32_t ext = 1;//расширенный идентификатор
            //идентификатор адресуемых устройств            
            uint32_t ident = tx_message.local_number + (0<<17) 
                            + (tx_message.code_type<<18);
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint8_t len = 0;//длина данных
            //printf("TX_QUERY_SERVICE -ident: %ld\n",ident);           
            remote_frame(ext, echo, ident, len);            
        }
        if(tx_message.tx_task_action == TX_ANSWER_SERVICE) {
            //Передача кадра данных по интерфейсу CAN - данные сервиса
            uint32_t ext = 1;//расширенный идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint32_t num = htol(number);//преобразование номера
            //выставляемый идентификатор кадра
            uint32_t ident = num + (1<<17) + (tx_message.code_type<<18);
            uint8_t len = byfer_can[tx_message.number_cadr][8];//длина данных
            //формирование данных
            uint64_t data_transmit = 0;
            for(int i=0; i<8; i++) {
                uint64_t ky = byfer_can[tx_message.number_cadr][i];
                data_transmit = data_transmit + (ky<<(i*8));
            }
            printf("Cadr %d: data-%lld, ident-%ld\n", tx_message.number_cadr, data_transmit, ident);
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN
        }
        if(tx_message.tx_task_action == TX_CHANGE_PARAMETR) {
            //Передача кадра данных по изменению параметра устройства
            uint32_t ext = 1;//расширенный идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            //выставляемый идентификатор кадра
            uint32_t ident = tx_message.local_number + (1<<17) + (tx_message.code_type<<18);
            uint8_t len = 3;//длина данных
            //формирование данных
            uint64_t data_transmit = tx_message.value_param + (tx_message.num_param << 16);
            //printf("Cadr change parametr: data-%lld, ident-%ld\n", data_transmit, ident);
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN
        }
        if(tx_message.tx_task_action == TX_COM_CADR) {
            //Передача кадра данных по интерфейсу CAN - команда
            uint32_t ext = 1;//расширенный идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            //выставляемый идентификатор кадра
            uint32_t ident = tx_message.local_number + (tx_message.code_type<<18);
            uint8_t len = 3;//длина данных
            //формирование данных
            uint64_t data_transmit = (tx_message.number_cadr << 16) + tx_message.value_param;
            //printf("Command change parametr: data-%lld, ident-%ld\n", data_transmit, ident);
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN
        }
        if(tx_message.tx_task_action == TX_NAME_SCRIPT) {
            //Передача кадра данных по интерфейсу CAN - имя, сценарий
             uint32_t ext = 1;//расширенный идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            //выставляемый идентификатор кадра
            uint32_t ident = tx_message.local_number + (tx_message.code_type<<18);
            uint8_t len = byfer_can[tx_message.number_cadr][8];//длина данных
            //формирование данных
            uint64_t data_transmit = 0;
            for(int i=0; i<len; i++) {
                uint64_t ky = byfer_can[tx_message.number_cadr][i];
                data_transmit = data_transmit + (ky<<(i*8));
            }
            //printf("Cadr %d: data-%lld, len-%d, ident-%ld\n",tx_message.number_cadr,data_transmit,len,ident);
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN
        }
        if(tx_message.tx_task_action == TX_EVENT_YES) {
            //Разрешение событий от датчиков в сети CAN
            uint32_t ext = 0;//базовый идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint32_t ident = ID_MASTER_REVIEW;//идентификатор адресуемых устройств
            uint8_t len = 0;//длина данных 
            remote_frame(ext, echo, ident, len);//кадр запроса                    
        }
        if(tx_message.tx_task_action == TX_EVENT_NO) {
            //Запрещение событий от датчиков в сети CAN
            uint32_t ext = 0;//базовый идентификатор
            uint32_t echo = 0;//сообщение многократное(при ошибке)
            uint32_t ident = ID_MASTER_REVIEW;//идентификатор адресуемых устройств
            uint8_t len = 0;//длина данных
            uint64_t data_transmit = 0;
            message_t_CAN(ext, echo, ident, len, data_transmit);//кадр данных в CAN  
        }
        xSemaphoreGive(ctrl_task_sem);//передача семафора
    }
    vTaskDelete(NULL);
}
//задача контроля работы всех задач
static void ctrl_task(void *arg)
{
    //ожидание получения семафора
    xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
    printf("START TASK CONTROL\n");
    //подключить перечень активностей и структуру
    rx_task_action_t rx_action;
    tx_message_t tx_message;
    //перенастроить сообщения от драйвера TWAI:
    //- Кадр получен и добавлен в очередь RX
    //twai_reconfigure_alerts(TWAI_ALERT_RX_DATA,NULL);
    //+ Один из счетчиков ошибок превысил предел предупреждения об ошибке
    twai_reconfigure_alerts(TWAI_ALERT_RX_DATA | TWAI_ALERT_ABOVE_ERR_WARN, NULL);
    uint32_t alerts;//где сохраняются сообщения от драйвера TWAI    
    while(1)//бесконечный цикл
    {   
      //сброс сторожевого таймера задачи
      if(esp_task_wdt_reset()==!ESP_OK) {printf("Reset TWDT - error\n");}
      
      if(flag_mode == 10) {//изменение контролируемых сигналов на входе
        //создать событие в сети CAN
        uint8_t num_par = param_change & 0xFF;//номер параметра
        uint8_t num_type = (param_change >> 8) & 0xFF;//номер типа, как указан в инф.сервисе
        uint8_t num_retry = (param_change >> 16) & 0xFF;//счетчик повторов
        if(num_retry > 0) {
        tx_message.tx_task_action = TX_CHANGE_PARAMETR;
        tx_message.local_number = htol(number);//преобразование номера
        tx_message.code_type = tab_param[num_type][0];
        tx_message.num_param = num_par;
        tx_message.value_param = tab_param[num_type][num_par] & 0xFFFF;
        //выставить запрос в очередь
        xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
        //ожидание получения семафора от задачи передачи сообщений в CAN
        xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        num_retry--;
        param_change = num_par + (num_type << 8) + (num_retry << 16);
        //если устройство мастер, отправить топик
        if(strcmp(obey,"master")==0) {
            data_service_on_mqtt(htol(number), num_type, 'p', num_par);
        }
        vTaskDelay(1);//задержка, 1 тик = 10мс
        } else flag_mode = 0;
      }
      //ЕСЛИ УСТРОЙСТВО МАСТЕР
      if(strcmp(obey,"master")==0) {  
         //чтение сообщений от TWAI
         if(flag_mode==0) twai_read_alerts(&alerts,0);
         if(alerts & TWAI_ALERT_ABOVE_ERR_WARN) {
            printf("Alerts ABOVE_ERR_WARN: %ld\n",alerts);
         }
            //если есть сообщение о приеме сообщения по CAN
            if(alerts & TWAI_ALERT_RX_DATA) {
                rx_action = RX_RECEIVE;
                //выставить запрос в очередь
                xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);
                //ожидание получения семафора от задачи чтения сообщений из CAN
                xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
                //printf("Semafor: from RECEIVE\n");
            } 
            alerts = 0;//сброс      
        if (flag_mode == 1) {//если есть признак сканирования сети мастером
            gpio_set_level(LED_ALARM,0);//выключить светодиод
            //gpio_set_level(LED_WORK,1);//включить светодиод
            twai_clear_receive_queue();//очистить очередь приема TWAI
            retry = RETRY;//задать максимальное число повторов
            //стиреть флеш-память отведенную под профили подчиненных устройств
            for(uint8_t i=0; i<64; i++) {
                esp_err_t ret;
                if(i!=htol(number)) {//только не для этого устройства
                    ret = nvs_flash_erase_partition(namespase[i]);
                }                
                if (ret != ESP_OK) {
                    printf("Failed to erase NVS-prof_device!\n");
                }
            }
            //обнулить массив номеров подчиненных устройств
            //memset(number_slaves,0,MAX_MEMBERS);
            for(int i=0; i<MAX_MEMBERS; i++) {
                number_slaves[i] = 0;
            }
            //выставить в сеть разрешение формирования событий от датчиков
            tx_message.tx_task_action = TX_EVENT_NO;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            //послать запрос сканирования сети
            tx_message.tx_task_action = TX_REVIEW;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            //printf("Semafor: from SEND\n");
            flag_mode = 2;//признак ожидания ответов сканирования 
        } 
        if(flag_mode == 2) { //признак ожидания ответов сканирования 
            //если истекло максимальное время ответов от сети               
            if(twai_read_alerts(&alerts,
            pdMS_TO_TICKS((WAIT_CAN*(MAX_MEMBERS + 1))/BAUD_RATE))==ESP_ERR_TIMEOUT) {
            //несколько раз сканировать сеть и суммировать адреса ответивших
            if(retry>0) {
               //послать запрос сканирования сети
                tx_message.tx_task_action = TX_REVIEW;
                //выставить запрос в очередь
                xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
                //ожидание получения семафора от задачи передачи сообщений в CAN
                xSemaphoreTake(ctrl_task_sem, portMAX_DELAY); 
                retry--;
            } else {
            //если хотя бы один ответил
            if(number_slaves[0]>0) {
                array_member =1;
                init_list_slaves();//запись в память списка подчиненных устройств
                flag_mode = 3;
                retry = RETRY;//задать максимальное число повторов
            }  else flag_mode = 0;
            //gpio_set_level(LED_WORK,0);//выключить светодиод
            printf("Number_slaves: %d, %d, %d, %d. Sum slaves: %d\n",number_slaves[1],
            number_slaves[2],number_slaves[3],number_slaves[4],number_slaves[0]);
            }
          }
        }
        //если есть признак чтения инф.сервисов подключенных к сети устройств 
        if (flag_mode == 3) {
            //формирование запроса информационного сервиса устройства сети
            tx_message.tx_task_action = TX_QUERY_SERVICE;
            tx_message.local_number = number_slaves[array_member];
            tx_message.code_type = ID_MASTER_REVIEW;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            flag_mode =4;//признак ожидания информационного сервиса
            byfer_can[0][0] = 0;//обнулить счетчик приема кадров данных
            byfer_can[0][1] = 0;// обнулить признак конца данных
        }
        //признак режима передачи данных инф.профилей устройств
        if(flag_mode == 8) {
            if(array_member == 0) {
                //информационный профиль мастера
                data_service_on_mqtt(htol(number), 0, 'n', ordinal);
                ordinal++;
                if(ordinal>7) {
                ordinal = 1;
                array_member = 1;
                }
            } else {//информационные профили подчиненных устройств             
                data_service_on_mqtt(number_slaves[array_member], 0, 'n', ordinal);
                ordinal++;
                if((ordinal>7)&&(array_member<number_slaves[0])) {
                    array_member++;
                    ordinal = 1;
                }
                if((ordinal>7)&&(array_member>=number_slaves[0])) {//окончание
                    array_member = 1;
                    flag_mode = 15;
                    //flag_mode = 0;
                }
            }
        }
        //признак ожидания информационного сервиса
        if(flag_mode == 4) { 
            //если истекло максимальное время ответов от сети            
          if(twai_read_alerts(&alerts,
                pdMS_TO_TICKS((WAIT_CAN*SIZE_PAUSE)/BAUD_RATE))==ESP_ERR_TIMEOUT) {
                if(byfer_can[0][0]==0) {//не получены данные
                    retry--;
                    if(retry==0) {
            printf("ERROR! Device lok.number %d no ping\n", number_slaves[array_member]);
                    gpio_set_level(LED_ALARM,1);//включить светодиод
                    flag_mode = 0;//чтобы не было сбоя
                    } else flag_mode = 3;
                } else {   
                printf("Inf.service device %d:\n",number_slaves[array_member]);             
            //проверка правильности полученных данных
            if(true_data()) {
            //запись информационного сервиса в память
            init_inf_service_slave(namespase[number_slaves[array_member]]);
            flag_mode = 3;
            array_member = array_member +1;//следующий номер устройства в списке
            retry = RETRY;//задать максимальное число повторов
            //окончание запросов
            if(array_member > number_slaves[0]) {
                //вывод списка устройств сети на печать + передача mqtt
                read_list_slaves();
                array_member = 0;                
                ordinal = 1;              
                flag_mode = 8;//режим передачи данных профилей 
                /*
                //вывод списка и полученных профилей на печать
                read_list_slaves();
                read_infservice(namespase[htol(number)]);//профиль "мастера"
                for(int i=1; i<=number_slaves[0]; i++) {
                    read_infservice(namespase[number_slaves[i]]);
                }
                array_member = 1;
                flag_mode = 0;
                */
             }
            } else {                
                printf("ERROR! Data loss\n");
                twai_clear_receive_queue();//очистить очередь приема TWAI
                retry--;
                if(retry<1) {//так и не получен профиль без сбоя
                    gpio_set_level(LED_ALARM,1);//включить светодиод                    
                    flag_mode = 0;//чтобы не было сбоя
                } else {
                    flag_mode = 3;//запрос опять этого номера устройства в списке
                }                                
            }
          }
         }
        }
        //признак чтения сервиса подключенного к сети устройства
        if(flag_mode == 13) {
            //формирование запроса сервиса устройства сети
            tx_message.tx_task_action = TX_QUERY_SERVICE;
            tx_message.local_number = number_slaves[array_member];
            tx_message.code_type = tabl_type[number_slaves[array_member]][ord_tab_type]+0x400;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);            
            byfer_can[0][0] = 0;//обнулить счетчик приема кадров данных
            byfer_can[0][1] = 0;// обнулить признак конца данных
            flag_mode = 14;//признак ожидания данных от запрашиваемого сервиса
        }
				//признак режима передачи данных типов функциональности всех устройств сети
				if(flag_mode == 16) {
					uint8_t num_dev;//лок.номер устройства
					//сервисы типа функциональности устройства "мастер"
					if(array_member == 0) {
						num_dev = htol(number);
					} else {
						//сервисы типа функциональности подчиненного устройства из списка
						num_dev = number_slaves[array_member];
					}
					//получение количества типов функциональности для этого устройства
					// из таблицы tabl_type[][] отбросив нулевые коды
					uint8_t k;
					for(k=8; k>0; k--) {
						if(tabl_type[num_dev][k]!=0) break;
					}
					uint8_t n = num_dev; 
 					printf("tabl_type[%d]:%d,%d,%d,%d,%d,%d,%d,%d,%d\n",n,tabl_type[n][0],
 					tabl_type[n][1],tabl_type[n][2],tabl_type[n][3],tabl_type[n][4],
					tabl_type[n][5],tabl_type[n][6],tabl_type[n][7],tabl_type[n][8]);    
					printf("k: %d\n", k);
      		//запрос начинается с последнего кода сервиса
      		ord_tab_type = k;						
					ordinal = 1;
					sum_par = 8;//априори
					flag_mode = 9;
				}
        //признак режима передачи данных типов функциональности устройствa
        if(flag_mode == 9) {
					uint8_t num_dev;//лок.номер устройства
					uint8_t num_type;//порядковый номер типа в перечне профиля устройства
					//сервисы типа функциональности устройства "мастер"
					if(array_member == 0) {
						num_dev = htol(number);
					} else {
						//сервисы типа функциональности подчиненного устройства из списка
						num_dev = number_slaves[array_member];
					}
					num_type = ord_tab_type;
					printf("ord_tab_type: %d\n", ord_tab_type);
					printf("ordinal: %d\n", ordinal);
					printf("sum_par: %d\n", sum_par);
					//формирование топиков имен сервиса
					if(ordinal<=3) {
						data_service_on_mqtt(num_dev,num_type,'n', ordinal);
					}										
					if((ordinal>3)&&(ordinal<=(sum_par+4))) {
						//формирование топиков имен параметров
						data_service_on_mqtt(num_dev,num_type,'t', ordinal-3);
					}
					if((ordinal>(sum_par+4))&&(ordinal<=(2*sum_par+4))) {
  					//формирование топиков величин параметров
  					data_service_on_mqtt(num_dev,num_type,'p', ordinal-(sum_par+4));
					}
					if(ordinal>(2*sum_par+3)) {
						ordinal = 0;
						sum_par = 8;
						ord_tab_type--;
						if(ord_tab_type<=0) {
							array_member++;
							flag_mode = 16;
						}
					}
					vTaskDelay(2);//1 тик = 10мс
					ordinal++;
					if(array_member>number_slaves[0]) {//окончание опроса
                        //формирование разрешения событий в CAN
                        tx_message.tx_task_action = TX_EVENT_YES;
                        //выставить запрос в очередь
                        xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
                //ожидание получения семафора от задачи передачи сообщений в CAN
                        xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);            
						flag_mode = 0;
					}
				}
        //признак ожидания данных от запрашиваемого сервиса
        if(flag_mode == 14) {
          //если истекло максимальное время ответов от сети            
          if(twai_read_alerts(&alerts,
            pdMS_TO_TICKS((WAIT_CAN*SIZE_PAUSE)/BAUD_RATE))==ESP_ERR_TIMEOUT) {
            if(byfer_can[0][0]==0) {//не получены данные
               retry--;
                if(retry==0) {
                    printf("ERROR! Device lok.number %d, type %X no ping\n",
                         number_slaves[array_member],
												 tabl_type[number_slaves[array_member]][ord_tab_type]);
                    gpio_set_level(LED_ALARM,1);//включить светодиод
                    flag_mode = 0;//чтобы не было сбоя
                } else flag_mode = 13;
            } else {
               printf("Service %X, device %d\n",
								tabl_type[number_slaves[array_member]][ord_tab_type],
                number_slaves[array_member]);
                //проверка правильности полученных данных
                if(true_data()) {
                //запись информационного сервиса в память                   
                init_service_slave(namespase[number_slaves[array_member]]);
                ord_tab_type--;//следующий тип функциональности устройства
                retry = RETRY;//задать максимальное число повторов
                flag_mode = 13;
                //окончание запросов типа для этого устройства
                if(ord_tab_type<=0) {
                    array_member++;
                    //запрос следующего устройства если список устройств не закончился
                    if(array_member<=number_slaves[0]) {
                        flag_mode = 15;
                    } else { //конец опроса устройств
											//запись tabl_type[][] во флеш память
											write_tabl_type_flash();
                      array_member = 0;
                      flag_mode = 16;												
                    }                   
                }
             } else {
                    printf("ERROR! Data loss\n");
                    twai_clear_receive_queue();//очистить очередь приема TWAI
                    retry--;
                    if(retry<1) {//так и не получен профиль без сбоя
                    gpio_set_level(LED_ALARM,1);//включить светодиод                 
                    flag_mode = 0;//чтобы не было сбоя
                    } else {
                    flag_mode = 13;//запрос опять этого типа
                    }                                
                }
              } 
            }    
        }
        //признак запроса данных сервисов подчиненных устройств
        if(flag_mode == 15) {
            //локальный номер устройства
            uint8_t loc_num_dev = number_slaves[array_member];
            printf("Device number: %d\n", loc_num_dev);
            //получение списка кодов типов для этого устройства ввиде строки таблицы "tabl_type[][]"
            uint8_t k = list_type_device(loc_num_dev);
            ord_tab_type = k;//запрос начинается с последнего кода сервиса
            printf("ord_tab_type: %d\n", ord_tab_type);
            flag_mode = 13;
        }
        //признак получения внешней команды
        if(flag_mode == 5) {
            tx_message.local_number = buf_com_can[0]&0xFFFF;
            tx_message.code_type = (buf_com_can[0]>>16) + 0x400;
            tx_message.number_cadr = buf_com_can[2]>>24;
            //printf("Number cadr: %d\n", tx_message.number_cadr);
            tx_message.value_param = buf_com_can[1];
            //послать команду в сеть на исполнение
            tx_message.tx_task_action = TX_COM_CADR;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            flag_mode = 0;
        }        
        //признак отправки в CAN сценария/имени
        if(flag_mode == 7) {
           tx_message.local_number = buf_com_can[0]&0xFFFF;
            tx_message.code_type = (buf_com_can[0]>>16) + 0x400;
            tx_message.number_cadr = byfer_can[0][0];
            tx_message.tx_task_action = TX_NAME_SCRIPT;
            flag_mode = 0;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY); 
        }
        //признак получения сценария/имени
        if(flag_mode == 6) {
            tx_message.local_number = buf_com_can[0]&0xFFFF;
            tx_message.code_type = (buf_com_can[0]>>16) + 0x400;
            tx_message.number_cadr = byfer_can[0][0] - 1;
            tx_message.tx_task_action = TX_NAME_SCRIPT;
            flag_mode = 7;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        }
        //подтверждение исполнения команды от сценария
        if(flag_mode == 12) {
            // формированиe сообщения о изм.параметра для отправки в MQTT,
            //а также запись изм. во флеш память
            uint32_t identifier = (buf_proof[0]<<18) + htol(number);
            uint8_t num_par = (buf_proof[1]>>16)&0xFF;
            uint16_t val_par = buf_proof[1]&0xFFFF;
            event_on_mqtt(identifier, num_par, val_par);
            flag_mode = 0;
        }
        if(!(alerts & TWAI_ALERT_RX_DATA)) vTaskDelay(1);//1 тик = 10мс
      }
      //ЕСЛИ УСТРОЙСТВО SLAVE
      if(strcmp(obey,"master")!=0) {
        //чтение сообщений от драйвера TWAI
        twai_read_alerts(&alerts,1);
        if(alerts & TWAI_ALERT_ABOVE_ERR_WARN) {
            printf("Alerts ABOVE_ERR_WARN: %ld\n",alerts);
         }
        //если есть сообщение о приеме сообщения по CAN
        if(alerts & TWAI_ALERT_RX_DATA) {
            //printf("Alert: RX [%ld]\n",xTaskGetTickCount());
            rx_action = RX_RECEIVE;
            //выставить запрос в очередь
            xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);
            //ожидание получения семафора от задачи чтения сообщений из CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            //printf("Semafor\n");
        }
        alerts = 0;//сброс
        //если есть признак сканирования сети мастером
        if (flag_mode == 1) {
            uint32_t num = htol(number);//преобразование номера
            int wait = (WAIT_CAN/BAUD_RATE)*(num -1); 
            //printf("WAIT: %d ms\n",wait);
            vTaskDelay(pdMS_TO_TICKS(wait));//задержка
            flag_mode =0;//сбросить признак сканирования
            //twai_clear_receive_queue();//очистить очередь приема TWAI
            tx_message.tx_task_action = TX_REVIEW_RESP;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            //printf("TX SEND[%ld]\n",xTaskGetTickCount());
        }
        //если есть флаг инициализации отправки инф.сервиса
        if(flag_mode == 3) {
            byfer_can[0][0] = 0;//обнулить счетчик кадров
            //прочитать из flash и занести в буфер CAN данные инф.сервиса
            read_infservice_forcan();
            flag_mode = 4;//признак передачи инф.сервиса
            tx_message.tx_task_action = TX_ANSWER_SERVICE;
            tx_message.code_type = ID_MASTER_REVIEW;
            number_cadr = 1;
            tx_message.number_cadr = number_cadr;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        }
        //если есть флаг передачи инф.сервиса
        if(flag_mode == 4) {
            if(number_cadr<byfer_can[0][0]) {
            tx_message.tx_task_action = TX_ANSWER_SERVICE;
            tx_message.code_type = ID_MASTER_REVIEW;
            number_cadr = number_cadr + 1;
            tx_message.number_cadr = number_cadr;            
            vTaskDelay(pdMS_TO_TICKS(12));//задержка
            //vTaskDelay(1);//задержка, 1 тик = 10мс
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            } else {
                printf("Service sent!\n");
                twai_clear_receive_queue();//очистить очередь приема TWAI
                flag_mode = 0;
            }
        }
        //если есть флаг передачи сервиса
        if(flag_mode == 14) {
            if(number_cadr<byfer_can[0][0]) {
            tx_message.tx_task_action = TX_ANSWER_SERVICE;
            tx_message.code_type = type_func + 0x400;
            number_cadr = number_cadr + 1;
            tx_message.number_cadr = number_cadr;            
            //vTaskDelay(pdMS_TO_TICKS(12));//задержка
            //vTaskDelay(1);//задержка, 1 тик = 10мс
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            } else {
                printf("Service sent!\n");
                twai_clear_receive_queue();//очистить очередь приема TWAI
                flag_mode = 0;
            }
        }
         //если есть флаг инициализации отправки сервиса
         if(flag_mode == 13) {
            byfer_can[0][0] = 0;//обнулить счетчик кадров                      
            //прочитать из flash и занести в буфер CAN данные сервиса
            read_service_forcan(type_func);
            //flag_mode = 0;
            tx_message.tx_task_action = TX_ANSWER_SERVICE;
            tx_message.code_type = type_func + 0x400;
            number_cadr = 1;
            tx_message.number_cadr = number_cadr;
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            flag_mode = 14;//признак передачи сервиса
         }
        if(flag_mode == 12) {//подтверждение исполнения команды от сценария или от мастера
            tx_message.tx_task_action = TX_CHANGE_PARAMETR;
            tx_message.local_number = htol(number);//преобразование номера
            tx_message.code_type = buf_proof[0];
            tx_message.num_param = (buf_proof[1]>>16) & 0xFF;
            tx_message.value_param = buf_proof[1] & 0xFFFF;       
            //выставить запрос в очередь
            xQueueSend(tx_task_que, &tx_message, portMAX_DELAY);
            //ожидание получения семафора от задачи передачи сообщений в CAN
            xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
            flag_mode = 0;
        }
      }
      if(flag_mode == 11) {//задержка исполнения
        vTaskDelay(pdMS_TO_TICKS(2*WAIT_CAN/BAUD_RATE));        
        flag_mode = 12;
      }
    }
    vTaskDelete(NULL);
}
//=============ETHERNET==========================================
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
//================================================================
void app_main()
{
    init_GPIO();//инициализация используемых выводов
    get_DeviceInf();//получения из инф.сервиса данных и 
                    // конфигурирование устройства в связи с ними
    app_driver_init();//инициализация кнопки "INIT"
     //создание очередей из одного значения размером заданного перечисления
     rx_task_queue = xQueueCreate(1, sizeof(rx_task_action_t));
     tx_task_que = xQueueCreate(1, sizeof(tx_message_t));
     //создание бинарных семафоров (1\0)
     ctrl_task_sem = xSemaphoreCreateBinary();
     // создание задач для ОС     
     xTaskCreate(twai_receive_task,"TWAI_rx",4096,NULL,RX_TASK_PRIO,&TaskHandleTwaiReceive);
     xTaskCreate(twai_transmit_task,"TWAI_tx",4096,NULL,TX_TASK_PRIO,&TaskHandleTwaiTransmit);
     xTaskCreate(ctrl_task,"TASK_ctrl",4096,NULL,CTRL_TSK_PRIO,&TaskHandleControl);    
     //Задание конфигурации сторожевого таймера задач
     esp_task_wdt_config_t twdt_config = {
        .timeout_ms = TWDT_TIMEOUT_MS,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,//Бит маска всех ядер
        .trigger_panic = false,
    };
    #if !CONFIG_ESP_TASK_WDT_INIT
    //Если TWDT не был инициализирован автоматически при запуске, инициализировать его
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    printf("TWDT initialized\n");
    #else//Если TWDT был инициализирован автоматически при запуске, перенастроить
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&twdt_config));
    printf("TWDT reconfigure\n");
    #endif    
    esp_task_wdt_add(TaskHandleControl);//подписка задачи на сторожевой таймер
    uint32_t num = htol(number);//преобразование локального номера данного контроллера
  if(num ==1) {//если лок.номер 1, устройство подключено к LAN        
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
    //Запуск mqtt client через соединение websocket security
    mqtt_wss_client_task();
  }  
    //Инициализация интерфейса CAN
    init_CAN(CAN_TX,CAN_RX);
    xSemaphoreGive(ctrl_task_sem);//передача семафора, старт задачи контроля
}
