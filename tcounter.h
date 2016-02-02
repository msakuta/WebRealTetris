#ifndef TCOUNTER_H_INCLUDED
#define TCOUNTER_H_INCLUDED
/* TCounter -- C version of timecounter */

#ifdef __cplusplus
extern "C"{
#endif

typedef enum MeasureMethod{CLOCK_TIMER, WIN32_TIMER, HIRES_TIMER} MeasureMethod;
typedef struct TCounter tcounter_t;
typedef struct tc_init_args{double cps; enum MeasureMethod method; void *user;} tc_init_args_t;
typedef struct tc_period_args{double dt; double avg, weight; int invokes; void *user;} tc_period_args_t;
typedef void (*tc_init_func_t)(tc_init_args_t*);
typedef void (*tc_period_func_t)(tc_period_args_t*);

tcounter_t *CreateTCounter(tc_period_func_t, tc_init_func_t, void *period_user, void *init_user);
void StartTCounter(tcounter_t *);
void StopTCounter(tcounter_t *, double weightForAverageCalc);
#define DeleteTCounter(tcp) free(tcp)

extern void tc_default_period(tc_period_args_t*);
extern void tc_default_init(tc_init_args_t*);

#ifdef __cplusplus
}
#endif

#endif
