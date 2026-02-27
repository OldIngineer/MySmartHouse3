//=============CAN=========================
#define CAN_TX      GPIO_NUM_2// выход данных
#define CAN_RX      GPIO_NUM_3// вход данных
#define BAUD_RATE   50// скорость обмена Kбит/сек; возможен выбор:
                            //100; 50; 20; 10
#if BAUD_RATE == 100
    #define TWAI_TIMING   TWAI_TIMING_CONFIG_100KBITS()
#endif
#if BAUD_RATE == 50
    #define TWAI_TIMING   TWAI_TIMING_CONFIG_50KBITS()
#endif
#if BAUD_RATE == 20
    #define TWAI_TIMING   TWAI_TIMING_CONFIG_20KBITS()
#endif
#if BAUD_RATE == 10
    #define TWAI_TIMING   TWAI_TIMING_CONFIG_10KBITS()
#endif
#define WAIT_CAN            200//длительность цикла приема/передачи в тактах CAN
#define SIZE_PAUSE          15//размер паузы при обмене по сети CAN, в циклах приема/передачи
#define MAX_MEMBERS         64//максимальное число участников сети CAN
#define ID_MASTER_REVIEW    0x400//идентификатор посылаемый в сеть для определения
                                //подключенных устройств
#define BEGIN_SCRIPT        22//начальный номер кадра данных для передачи сценариев
#define RX_TASK_PRIO        8//приоритет задачи чтения сообщений
#define TX_TASK_PRIO        9//приоритет задачи передачи сообщений
#define CTRL_TSK_PRIO       10//приоритет задачи контроля
#define TCPC_TSK_PRIO       7//приоритет задачи TCP-клиент
#define WS_TSK_PRIO         6//приоритет задачи websocket client
#define MWC_TSK_PRIO        5//приоритет задачи mqtt websocket secure client
#define RETRY               3//число повторов при передаче по сети CAN
    //========= ВЫХОДЫ УПРАВЛЕНИЯ ==============
#define LED_WORK    (GPIO_NUM_23)//управление светодиодом VD1 "РАБОТА", зел.
#define LED_ALARM   (GPIO_NUM_18)//управление светодиодом VD11 "АВАРИЯ", красн.
#define M_CLOSE     (GPIO_NUM_16)//управление мотором, закрытие лог.1
#define M_OPEN      (GPIO_NUM_17)//управление мотором, открытие лог.1
#define RELAY_1     (GPIO_NUM_11)//управление реле 1, вкл. лог.1
#define RELAY_2     (GPIO_NUM_10)//управление реле 2, вкл. лог.1
#define SPI_MOSI    (GPIO_NUM_4)//SPI выход данных для модуля ETHERNET
#define SCLK        (GPIO_NUM_6)//тактовый сигнал для модуля ETHERNET
#define nSS         (GPIO_NUM_5)//выбор модуля ETHERNET, лог.0
#define nRESET      (GPIO_NUM_1)//сброс модуля ETHERNET, лог.0
//========== ВХОДЫ УПРАВЛЕНИЯ ================
#define INIT        (GPIO_NUM_9)//вход для кнопки SB2
#define SIGN_H      (GPIO_NUM_21)//входной сигнал управления, высокое напряжение
#define SIGN_L      (GPIO_NUM_20)//входной сигнал управления, низкое напряжение
#define SPI_nINT    (GPIO_NUM_7)// прерывание от модуля ETHERNET, лог.0
#define SPI_MISO    (GPIO_NUM_0)// SPI вход данных от модуля ETHERNET
#define SIGNAL      (GPIO_NUM_22)// сигнал от датчика температуры/влажности
//=========================================================================
#define TWDT_TIMEOUT_MS        10000//период сторожевого таймера задач, ms
#define EVENT_TIMEOUT_MS        300//цикл задержки формирования событий от датчиков 
    //======== ETHERNET, TCP/IP
#define SPI_CLOCK_MHZ           16//частота обмена по SPI в Мгц
#define WAIT_TAKE_IP        8000//время задержки на получение адреса Ip от роутера
    //======= MQTT WSS CLIENT=======================
#define BROKER_URI  "wss://m3.wqtt.ru:13108/wss"//адрес брокера mqtt
#define USER_NAME   "user"//имя пользователя
#define ID_CLIENT   "12345"//идентификационный номер клиента
#define PASSWORD    "PassWord"//пароль

