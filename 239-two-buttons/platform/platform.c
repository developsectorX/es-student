#include "pico/stdlib.h"
#include <stdint.h>
#include "platform.h"

#define BUTTON_2_PIN 14
#define BUTTON_1_PIN 15
#define LED_PIN 25

static const uint8_t button_pins[PLATFORM_BUTTON_COUNT]={
 [PLATFORM_BUTTON_1]=BUTTON_1_PIN,
 [PLATFORM_BUTTON_2]=BUTTON_2_PIN,
};

void platform_led_set(bool on){gpio_put(LED_PIN,on);}

bool platform_button_read(platform_button_t button){
 if((uint32_t)button>=PLATFORM_BUTTON_COUNT) return false;
 return !gpio_get(button_pins[button]);
}

void platform_init(void){
 stdio_init_all();
 gpio_init(LED_PIN);
 gpio_set_dir(LED_PIN,GPIO_OUT);
 for(uint32_t i=0;i<PLATFORM_BUTTON_COUNT;i++){
  const uint pin=button_pins[i];
  gpio_init(pin);
  gpio_set_dir(pin,GPIO_IN);
  gpio_pull_up(pin);
 }
}
