#ifndef NDEBUG /* if not defined, the entire file turns into a void */
#include "dprint.h"
#include <stdarg.h>
#ifdef WIN32
#define VC_EXTRALEAN
#include <windows.h>
#endif

#define ALLOCUNIT 0x80

static FILE *g_dbgfp = NULL;
FILE ** const g_dprint_logfpp = &g_dbgfp;

/* swap file pointer to a log file (must be in text mode) */
FILE *dprintfile(FILE *fp){
	FILE *old = g_dbgfp;
	g_dbgfp = fp;
	return old;
}

/* debug outputs */
int debugprintf(const char *s, ...){
	va_list vl;
#ifdef WIN32
	static size_t sz = 0;
	static char *buf = NULL;
#endif
	int len;
	if(!s) return 0;
	va_start(vl, s);
	len = vfprintf(stderr, s, vl)+1;
#ifdef WIN32
	if(sz < (size_t)len) buf = realloc(buf, sz = ((len + ALLOCUNIT - 1) / ALLOCUNIT) * ALLOCUNIT);
	vsprintf(buf, s, vl);
	OutputDebugString(buf);
#endif
	if(g_dbgfp) vfprintf(g_dbgfp, s, vl);
	va_end(vl);
	return len;
}

int dprintencoder(void *data, int (*encoder)(FILE*, const void*), void *offset){
	int ret;
	ret = encoder(stderr, &((char*)offset)[(int)data]);
	if(!ret) return ret;
	if(g_dbgfp){
		ret = encoder(g_dbgfp, &((char*)offset)[(int)data]);
		if(!ret) return ret;
	}
#if 0 && defined WIN32
	{
	static FILE *tfp = NULL;
	long size;
	char buf[2] = {'\0', '\0'};
	if(!tfp) tfp = tmpfile();
	if(!tfp) return ret;
	rewind(tfp);
	putc('\0', tfp);
	rewind(tfp);
	if(!(ret = encoder(tfp, &((char*)offset)[(int)data]))) return ret;
	size = ftell(tfp);
	rewind(tfp);
	while(size){
		buf[0] = getc(tfp);
		OutputDebugString(buf);
		size--;
	}
	}
#endif
	return ret;
}

#endif
