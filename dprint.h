#ifndef DPRINT_H
#define DPRINT_H

#ifndef NDEBUG
#include <stdio.h>
extern FILE *dprintfile(FILE *newfp);
extern int debugprintf(const char *format, ...);
extern int dprintencoder(void *data, int (*encoder)(FILE *, const void *data), void *offset);
extern FILE ** const g_dprint_logfpp;
#define dprintf debugprintf
#else
/* its arguments turn out to be a comma-separated list, might work out
  its side effects differently as functuon arguments in rare cases. */
#define dprintfile
#define dprintf
#define dprintencoder
#endif

#endif /* DPRINT_H */
