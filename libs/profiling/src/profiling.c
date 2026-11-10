#include "pico/stdlib.h"
#include "profiling/profiling.h"

typedef struct{
 bool is_used;
 bool is_running;
 uint32_t start_us;
 uint32_t max_us;
 uint32_t total_us;
 uint32_t count;
 const char *name;
} stopwatch_t;

static stopwatch_t stopwatches[PROFILING_MAX_STOPWATCHES];

void profiling_reset(void){
 for(uint32_t i=0;i<PROFILING_MAX_STOPWATCHES;i++){
  if(!stopwatches[i].is_used) continue;
  stopwatches[i].max_us=0;
  stopwatches[i].total_us=0;
  stopwatches[i].count=0;
 }
}

bool profiling_get(uint32_t id,profiling_result_t *result){
 if(id>=PROFILING_MAX_STOPWATCHES||!stopwatches[id].is_used) return false;
 result->max_us=stopwatches[id].max_us;
 result->mean_us=stopwatches[id].count?stopwatches[id].total_us/stopwatches[id].count:0;
 result->count=stopwatches[id].count;
 result->name=stopwatches[id].name;
 return true;
}

void profiling_stopwatch_init(uint32_t id,const char *name){
 if(id>=PROFILING_MAX_STOPWATCHES) return;
 stopwatches[id].is_used=true;
 stopwatches[id].is_running=false;
 stopwatches[id].start_us=0;
 stopwatches[id].max_us=0;
 stopwatches[id].total_us=0;
 stopwatches[id].count=0;
 stopwatches[id].name=name;
}

#if PROFILING_ENABLED
void profiling_stop(uint32_t id){
 if(id>=PROFILING_MAX_STOPWATCHES) return;
 if(!stopwatches[id].is_used||!stopwatches[id].is_running) return;
 uint32_t diff=(uint32_t)time_us_32()-stopwatches[id].start_us;
 stopwatches[id].is_running=false;
 stopwatches[id].count++;
 stopwatches[id].total_us+=diff;
 if(diff>stopwatches[id].max_us) stopwatches[id].max_us=diff;
}

void profiling_start(uint32_t id){
 if(id>=PROFILING_MAX_STOPWATCHES||!stopwatches[id].is_used) return;
 stopwatches[id].is_running=true;
 stopwatches[id].start_us=(uint32_t)time_us_32();
}
#endif
