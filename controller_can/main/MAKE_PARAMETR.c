//подключение пользовательских значений
#include "app_const.h"
//подключение общих программ
#include <stdint.h>
#include "driver/gpio.h"

// Функция выполнения действий при обращении по изменению параметра функциональности
    //- первый параметр функции, зто код обработки =
    // код функциональности + номер параметра << 11
    //- второй параметр, это величина параметра функциональности
void make_parametr(uint16_t kod_make, uint16_t value_par)
{
    if(kod_make==(0x210+(1<<11))) {
        if(value_par>0) value_par = 1;
        gpio_set_level(RELAY_1, value_par);
    }
    if(kod_make==(0x210+(2<<11))) {
        if(value_par>0) value_par = 1;
        gpio_set_level(RELAY_2, value_par);
    }
}