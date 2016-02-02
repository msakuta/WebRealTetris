/* Private header for temporary entities */
#ifndef TENT_P_H
#define TENT_P_H
#include "tent.h"

/*#############################################################################
	macro definitions
#############################################################################*/
#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#ifndef MAX
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef ABS
#define ABS(a) ((a)>0?(a):-(a))
#endif

/* basic macros */
#define numof(a) (sizeof(a)/sizeof*(a)) /* number of elements in an array */
#define ADV(a,m) ((a) = ((a) + 1) % (m)) /* advance a ring buffer pointer. */
#define DEV(a,m) ((a) = ((a) - 1 + (m)) % (m)) /* devance a ring buffer pointer. */
#define drand() ((double)rand() / RAND_MAX)

/* bitmap macros */
#define BITMAPHIT(bm,x,y) (0<=(x)&&(x)<(bm).bmWidth&&0<=(y)&&(y)<(bm).bmHeight)
#define BITMAPBITS(bm,x,y) ((COLORREF*)&((BYTE*)(bm).bmBits)[(x)*sizeof(COLORREF)+(y)*(bm).bmWidthBytes])
#define BITMAPHITBITS(bm,x,y) (BITMAPHIT(bm,x,y)?BITMAPBITS(bm,x,y):NULL)
#define TRANSX(mat,x) ((mat)[0] * (x) + (mat)[2] * (x) + (mat)[4])
#define TRANSY(mat,y) ((mat)[1] * (y) + (mat)[3] * (y) + (mat)[5])
#define DTRANSX(mat,x) ((mat)[0] * (x) + (mat)[2] * (x))
#define DTRANSY(mat,y) ((mat)[1] * (y) + (mat)[3] * (y))
#define RECTHIT(r, x, y) ((r).left < (x) && (x) < (r).right && (r).top < (y) && (y) < (r).bottom)
#define RECTINTERSECT(r1,r2) ((r1).left <= (r2).right && (r2).left <= (r1).right && (r1).top <= (r2).bottom && (r2).top <= (r1).bottom)

/* Color management macros */
#define ADDBLEND(a,b) RGB(MIN(255,GetRValue(a)+GetRValue(b)),MIN(255,GetGValue(a)+GetGValue(b)),MIN(255,GetBValue(a)+GetBValue(b)))
#define SUBBLEND(a,b) RGB(MAX(0,GetRValue(a)-GetBValue(b)),MAX(0,GetGValue(a)-GetGValue(b)),MAX(0,GetBValue(a)-GetRValue(b)))
#define ALPHABLEND(a,b,m) RGB(((255-(m))*GetRValue(a)+(m)*GetBValue(b))/256,((255-(m))*GetGValue(a)+(m)*GetGValue(b))/256,((255-(m))*GetBValue(a)+(m)*GetRValue(b))/256)
#define RATEBLEND(a,b,c,d) RGB((((d)-(c))*GetRValue(a)+(c)*GetRValue(b))/(d),(((d)-(c))*GetGValue(a)+(c)*GetGValue(b))/(d),(((d)-(c))*GetBValue(a)+(c)*GetBValue(b))/(d))
#define SWAPRB(c) RGB(GetBValue(c),GetGValue(c),GetRValue(c))
#define SWAPRB32(c) ((c)&0xff00ff00|((c)>>16)&0xff|((c)&0xff<<16)) /* faster?? */

/* constant values */
#define MAX_TENT 128
#define MAX_TELINE (g_pinit->TelineMaxCount)/*256*/
#define MAX_TEFBOX (g_pinit->TefboxMaxCount)/*64*/
#define INIT_TELINE (g_pinit->TelineInitialAlloc)
#define INIT_TEFBOX (g_pinit->TefboxInitialAlloc)
#define UNIT_TELINE (g_pinit->TelineExpandUnit)
#define UNIT_TEFBOX (g_pinit->TefboxExpandUnit)
#define SHRINK_START (g_pinit->TelineShrinkStart)
#define FADE_START (g_pinit->TelineShrinkStart)
#define REFLACTION (g_pinit->TelineElasticModulus)/*0.7*/ /* reflection velocity fraction */

#ifndef NDEBUG
#define TOTALMEMORY (g_tetimes.teline_m * g_tetimes.so_teline_t +\
		g_tetimes.tefbox_m * g_tetimes.so_tefbox_t +\
		g_tetimes.tefpol_m * g_tetimes.so_tefpol_t +\
		(g_tetimes.tevert_s ? g_pinit->PolygonVertexBufferSize * g_tetimes.so_tevert_t : 0))
#else
#define TOTALMEMORY 0
#endif

/* teline_flags_t */
#define TEL_FORMS	(TEL_CIRCLE|(TEL_CIRCLE<<1)|(TEL_CIRCLE<<2)|(TEL_CIRCLE<<3)) /* bitmask for forms */

/* tefbox_flags_t */
#define TEF_CUSTOM	(1<<7) /* use color sequence for tefboxes */


/*-----------------------------------------------------------------------------
	function prototypes
-----------------------------------------------------------------------------*/
extern COLORREF ColorSequence(const struct color_sequence *cs, double time);
#if LANG==JP
extern struct gryph *newgryph(const char s[3], int size);
extern void drawgryph(teout_t *pwg, const struct gryph *g, int x0, int y0, COLORREF drawcolor);
extern void cleargryph(void);
#endif

#endif /* TENT_P_H */
