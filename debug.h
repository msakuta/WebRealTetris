#ifndef DEBUG_H
#define DEBUG_H
/* project's private debug header */

#ifndef NDEBUG
#include "tcounter.h"
#define STARTTIMER(name,data) {static tcounter_t *tcp##name = NULL;\
	if(!tcp##name && NULL == (tcp##name = CreateTCounter(tc_handler, tc_initdprint, data, #name)))\
		abort();\
		StartTCounter(tcp##name);{
#define STARTTIMERP(name) {static tcounter_t *tcp##name = NULL;\
	if(!tcp##name && NULL == (tcp##name = CreateTCounter(tc_perioddprint, tc_initdprint, #name, #name)))\
		abort();\
		StartTCounter(tcp##name);{
#define STOPTIMER(name) }StopTCounter(tcp##name, 1);}
extern void tc_perioddprint(tc_period_args_t *a);
extern void tc_initdprint(tc_init_args_t *);
extern void tc_handler(tc_period_args_t *a);
#else
#define STARTTIMER(name,data)
#define STARTTIMERP(name)
#define STOPTIMER(name)
#endif /* NDEBUG */

#endif /* DEBUG_H */
