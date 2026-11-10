#include "systime/systime.h"
#include "led-task/led-task.h"
#include "platform/platform.h"

#define DEFAULT_PERIOD_MS 1000U

static bool led_on=true;
static uint64_t last_toggle_us=0;
static uint32_t led_half_period_us=(DEFAULT_PERIOD_MS*1000U)/2U;
static uint32_t led_period_ms=DEFAULT_PERIOD_MS;
static led_state_t led_state=LED_STATE_BLINK;

led_state_t led_task_get_state(void){return led_state;}
uint32_t led_task_get_period_ms(void){return led_period_ms;}

void led_task_set_period_ms(uint32_t period_ms){
 led_half_period_us=(period_ms*1000U)/2U;
 led_period_ms=period_ms;
}

void led_task_set_state(led_state_t state){
 led_state=state;
 if(state==LED_STATE_OFF){platform_led_set(false);led_on=false;return;}
 if(state==LED_STATE_ON){platform_led_set(true);led_on=true;return;}
 last_toggle_us=systime_us()-led_half_period_us;
}

void led_task_init(void){
 led_state=LED_STATE_BLINK;
 led_period_ms=DEFAULT_PERIOD_MS;
 led_half_period_us=(DEFAULT_PERIOD_MS*1000U)/2U;
 led_on=true;
 last_toggle_us=systime_us();
 platform_led_set(true);
}

void led_task_handle(void){
 if(led_state!=LED_STATE_BLINK) return;
 if(systime_period_elapsed(&last_toggle_us,led_half_period_us)){led_on=!led_on;platform_led_set(led_on);}
}

void led_task_next_state(void){
 if(led_state==LED_STATE_OFF){led_task_set_state(LED_STATE_ON);return;}
 if(led_state==LED_STATE_ON){led_task_set_state(LED_STATE_BLINK);return;}
 led_task_set_state(LED_STATE_OFF);
}

void led_task_next_period(void){
 switch(led_period_ms){
  case 250: led_task_set_period_ms(500); break;
  case 500: led_task_set_period_ms(1000); break;
  case 1000: led_task_set_period_ms(250); break;
  default: led_task_set_period_ms(500); break;
 }
}
