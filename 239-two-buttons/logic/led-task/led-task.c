#include "systime/systime.h"
#include "led-task/led-task.h"
#include "platform/platform.h"

#define DEFAULT_PERIOD_MS 1000U

static uint64_t last_toggle_us=0;
static uint32_t led_half_period_us=(DEFAULT_PERIOD_MS*1000U)/2U;
static uint32_t led_period_ms=DEFAULT_PERIOD_MS;
static led_state_t led_state=LED_STATE_BLINK;
static bool led_on=true;

uint32_t led_task_get_period_ms(void){return led_period_ms;}
led_state_t led_task_get_state(void){return led_state;}

void led_task_set_period_ms(uint32_t period_ms){
 led_period_ms=period_ms;
 led_half_period_us=(period_ms*1000U)/2U;
}

void led_task_set_state(led_state_t state){
 led_state=state;
 if(state==LED_STATE_OFF){led_on=false;platform_led_set(false);return;}
 if(state==LED_STATE_ON){led_on=true;platform_led_set(true);return;}
 last_toggle_us=systime_us()-led_half_period_us;
}

void led_task_init(void){
 led_on=true;
 led_state=LED_STATE_BLINK;
 led_period_ms=DEFAULT_PERIOD_MS;
 led_half_period_us=(DEFAULT_PERIOD_MS*1000U)/2U;
 last_toggle_us=systime_us();
 platform_led_set(true);
}

void led_task_handle(void){
 if(led_state!=LED_STATE_BLINK) return;
 if(!systime_period_elapsed(&last_toggle_us,led_half_period_us)) return;
 led_on=!led_on;
 platform_led_set(led_on);
}

void led_task_next_state(void){
 switch(led_state){
  case LED_STATE_OFF: led_task_set_state(LED_STATE_ON); break;
  case LED_STATE_ON: led_task_set_state(LED_STATE_BLINK); break;
  default: led_task_set_state(LED_STATE_OFF); break;
 }
}

void led_task_next_period(void){
 if(led_period_ms==250){led_task_set_period_ms(500);return;}
 if(led_period_ms==500){led_task_set_period_ms(1000);return;}
 if(led_period_ms==1000){led_task_set_period_ms(250);return;}
 led_task_set_period_ms(500);
}
