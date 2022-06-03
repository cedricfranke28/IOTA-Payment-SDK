#include "led.h"
#include "esp_log.h"
#include "driver/gpio.h"

int RedLED = 2;
int GreenLED = 33;

void initGreenLED()
{
    gpio_set_direction(GreenLED, GPIO_MODE_OUTPUT);
}

void initRedLED()
{
    gpio_set_direction(RedLED, GPIO_MODE_OUTPUT);
}

void setGreenLEDOn()
{
    gpio_set_level(GreenLED, 1);
}

void setRedLEDOn()
{
    gpio_set_level(RedLED, 1);
}

void setGreenLEDOff()
{
    gpio_set_level(GreenLED, 0);
}

void setRedLEDOff()
{
    gpio_set_level(RedLED, 0);
}