#include "tcounter.h"

#if !defined _WINDOWS_ && defined _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#include <windows.h>
#endif //_WIN32

#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define true 1
#define false 0
typedef unsigned char bool;
typedef struct TCounter tt;

struct TCounter{
	union{
		clock_t clock;
		DWORD dword;
		LARGE_INTEGER hires;
	} ofs;
	double avg;
	double totalweight;
	int invokes;
	tc_period_func_t period;
	tc_init_func_t pfnInit;
	void *period_u;
	void *init_u;
};


static bool g_init = false;
static MeasureMethod g_method;
static double g_freq;

void tc_default_period(tc_period_args_t *a){
	int size;
	char *s;
	fprintf(stdout, "%s[%d]: %.10g s avg %.10g\n%n", a->user ? (const char*)a->user : "Timer", a->invokes, a->dt, a->avg, &size);
	s = (char*)malloc(size+1);
	sprintf(s, "%s[%d]: %.10g s avg %.10g\n", a->user ? (const char*)a->user : "Timer", a->invokes, a->dt, a->avg);
	OutputDebugString(s);
	free(s);
}
void tc_default_init(tc_init_args_t *a){
	int size;
	char *s;
	fprintf(stdout, "%s first invocation: High-res counter is %s, resolution = %g per second\n%n",
		a->user ? (const char*)a->user : "Timer", a->method == HIRES_TIMER ? "available" : "unavailable", a->cps, &size);
	s = (char*)malloc(size+1);
	sprintf(s, "%s first invocation: High-res counter is %s, resolution = %g per second\n",
		a->user ? (const char*)a->user : "Timer", a->method == HIRES_TIMER ? "available" : "unavailable", a->cps);
	OutputDebugString(s);
	free(s);
}

tcounter_t *CreateTCounter(tc_period_func_t period, tc_init_func_t init, void *period_user, void *init_user){
	tcounter_t *m = malloc(sizeof(tcounter_t));
	if(!m) return NULL;
	m->totalweight = (0), m->invokes = (0), m->period = (period), m->pfnInit = (init), m->period_u = (period_user), m->init_u = (init_user);
	if(g_init) return m;
	g_init = true;
	{
	BOOL hires;
	LARGE_INTEGER freq;
	if(hires = QueryPerformanceFrequency(&freq))
		g_method = HIRES_TIMER;
	else g_method = CLOCK_TIMER;
	g_freq = hires ? freq.HighPart * ((double)UINT_MAX + 1) + (double)freq.LowPart : CLOCKS_PER_SEC;
	}
	return m;
}

void StartTCounter(tcounter_t *m){
	if(m->invokes == 0 && m->pfnInit){
		tc_init_args_t args;
		args.cps = g_freq;
		args.method = g_method;
		args.user = m->init_u;
		m->pfnInit(&args);
	}
	if(g_method == HIRES_TIMER)
		QueryPerformanceCounter(&m->ofs.hires);
	else m->ofs.clock = clock();
}

void StopTCounter(tcounter_t *m, double weight){
	tc_period_args_t args;
	if(!m->period)
		return;
	args.user = m->period_u;
	if(g_method == HIRES_TIMER){
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		args.dt = ((double)(now.HighPart - m->ofs.hires.HighPart) * ((double)ULONG_MAX + 1)
			+ (double)(now.LowPart - m->ofs.hires.LowPart))
			/ g_freq;
	}
	else args.dt = (double)(clock() - m->ofs.clock) / CLOCKS_PER_SEC;
	args.avg = m->avg = (m->totalweight + weight) != 0.0 ? (m->totalweight * m->avg + weight * args.dt) / (m->totalweight + weight) : 0.0;
//	args.avg = m_avg = (m_invokes * m_avg + args.dt) / (m_invokes + 1);
	args.weight = m->totalweight += weight;
	args.invokes = ++m->invokes;
	m->period(&args);
}
