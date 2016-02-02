#ifndef INI_H
#define INI_H
#include "realt.h"
#include <stdio.h>
#include "../lib/colseq.h"

extern int LoadInitFile(struct init *, const char *fname);
extern int SaveInitFile(const struct init *, const char *fname, int def);

struct codec{ /* a set of coder and decoder */
	int (*parser)(void*, const char*); /* function that parse the string into the object pointed, returns nonzero on success */
	int (*encoder)(FILE*, const void*); /* function that translate the object pointed out into the stream, returns nonzero on success */
	int (*printer)(char*, const void*, size_t); /* stringnizer */
	const char *name; /* type identifier string for the codec */
	size_t size; /* memory occupied for the datum */
};

extern const struct codec cdc_path, cdc_bool, cdc_eBlockDrawMethod;

struct matchset{
	const char *str;
#ifdef __cplusplus
	void init::*dst;
#else
	void *dst;
#endif
	const struct codec *cdc;
	int (*vrc)(const void*); /* value range checker */
	const char *desc; /* optional description */
	DWORD flags; /* 0 - whether modifiable during execution */
};

extern struct matchset initset[];
extern int g_initsetsize;

extern struct color_node cnl_darkpurple[];

#endif
