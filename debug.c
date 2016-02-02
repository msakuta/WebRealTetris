#ifndef NDEBUG
#include "debug.h"
#include "dprint.h"

void tc_perioddprint(tc_period_args_t *a){
	dprintf("%s[%d]: %.10g s avg %.10g\n", a->user ? (const char*)a->user : "Timer", a->invokes, a->dt, a->avg);
}

void tc_initdprint(tc_init_args_t *a){
	dprintf("%s first invocation: High-res counter is %s, resolution = %g per second\n",
		a->user ? (const char*)a->user : "Timer", a->method == HIRES_TIMER ? "available" : "unavailable", a->cps);
}

void tc_handler(tc_period_args_t *a){
	if(a->user) *(double*)a->user = a->dt;
}

#endif
