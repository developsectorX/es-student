#include "systime/systime.h"
#include <stddef.h>
#include "pi-task/pi-task.h"

#define PI_TERMS_PER_CALL 100

static uint64_t pi_end_us=0;
static uint64_t pi_start_us=0;
static uint32_t pi_terms_total=0;
static uint32_t pi_k=0;
static double pi_sign=1.0;
static double pi_sum=0.0;
static pi_state_t pi_state=PI_STATE_NOT_STARTED;

pi_state_t pi_task_get_state(void){return pi_state;}
double pi_task_get_value(void){return pi_sum*4.0;}
uint32_t pi_task_get_terms(void){return pi_terms_total;}

void pi_task_init(void){pi_state=PI_STATE_NOT_STARTED;}

uint64_t pi_task_get_duration_ms(void){
 if(pi_state==PI_STATE_NOT_STARTED) return 0;
 if(pi_state==PI_STATE_RUNNING) return (systime_us()-pi_start_us)/1000;
 return (pi_end_us-pi_start_us)/1000;
}

bool pi_task_get_result(double *value,uint64_t *duration_ms){
 if(pi_state!=PI_STATE_DONE) return false;
 if(value) *value=pi_sum*4.0;
 if(duration_ms) *duration_ms=(pi_end_us-pi_start_us)/1000;
 return true;
}

bool pi_task_start(uint32_t terms){
 if(pi_state==PI_STATE_RUNNING) return false;
 pi_end_us=0;
 pi_start_us=systime_us();
 pi_terms_total=terms;
 pi_k=0;
 pi_sign=1.0;
 pi_sum=0.0;
 pi_state=PI_STATE_RUNNING;
 return true;
}

void pi_task_handle(void){
 if(pi_state!=PI_STATE_RUNNING) return;
 uint32_t end=pi_k+PI_TERMS_PER_CALL;
 if(end>pi_terms_total||end<pi_k) end=pi_terms_total;
 double sum=pi_sum;
 double sign=pi_sign;
 double denom=2.0*(double)pi_k+1.0;
 for(uint32_t i=pi_k;i<end;i++){sum+=sign/denom;sign=-sign;denom+=2.0;}
 pi_sum=sum;
 pi_sign=sign;
 pi_k=end;
 if(pi_k>=pi_terms_total){pi_state=PI_STATE_DONE;pi_end_us=systime_us();}
}
