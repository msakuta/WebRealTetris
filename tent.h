#ifndef TENT_H
#define TENT_H
/* deals with temporaly entities. */

#include "realt.h"
#define VC_EXTRALEAN
#include <windows.h>
#include "../WinGraph.h"
#include "../lib/colseq.h"

/*#############################################################################
	macro definitions
#############################################################################*/

/* teline_flags_t */
#define TEL_NOREMOVE    (1<<0) /* if set, won't be deleted when it gets out of border. */
#define TEL_HEADFORWARD (1<<1) /* the line will head towards its velocity. omg now means angle offset. */
#define TEL_VELOLEN     (1<<2) /* set length by its velocity. len now means factor. */
#define TEL_RETURN      (1<<3) /* lines gone beyond border will appear at opposide edge */
#define TEL_SHRINK      (1<<4) /* shrink length as its life decreases */
#define TEL_REFLECT     (1<<5) /* whether reflect on border, ignored when RETURN is set */
#define TEL_STAR        (1<<6) /* whether draw perpendicular line as well */
#define TEL_HALF        (1<<7) /* center point means one of the edges of the line */
#define TEL_NOLINE		(1<<8) /* disable draw of lines */
#define TEL_TRAIL		(1<<9) /* append trailing tracer */
#define TEL_FOLLOW      (1<<10) /* follow some other tent, assume vx as the pointer to target */
#define TEL_CIRCLE		(1<<11) /* line form; uses 3 bits up to 12th bit */
#define TEL_FILLCIRCLE	(2<<11)
#define TEL_POINT		(3<<11)
#define TEL_RECTANGLE	(4<<11)
#define TEL_FILLRECT	(5<<11)
#define TEL_PENTAGRAPH	(6<<11)
#define TEL_HEXAGRAPH	(7<<11)
#define TEL_KANJI       (8<<11)
#define TEL_KANJI2      (9<<11)
#define TEL_GRADCIRCLE  (10<<11) /* gradiated circle with transparency, assume col as pointer to CS */
#define TEL_GRADCIRCLE2 (11<<11) /* time-dependent version, assume col as pointer to CS2 */

/* tefbox_flags_t */
#define TEF_BORDER		(1<<0) /* whether draw the border line */
#define TEF_NOFILL		(1<<1) /* do not fill inside (combine with border!) */
#define TEF_ADDBLEND    TEP_ADDBLEND /* common */
#define TEF_SOLIDBLEND  TEP_SOLIDBLEND
#define TEF_ALPHABLEND  TEP_ALPHABLEND
#define TEF_SUBBLEND    TEP_SUBBLEND

/* tefpol_flags_t */
#define TEP_FINE        (1<<0) /* finer granularity for graduation */
#define TEP_SHAKE       (1<<1) /* randomly shift vertices per rendering */
#define TEP_WIND        (1<<2) /* wake of polyline will wind */
#define TEP_RETURN      (1<<3) /* lines gone beyond border will appear at opposide edge */
#define TEP_DRUNK       (1<<4) /* add random velocity to the head per step */
#define TEP_REFLECT     (1<<5) /* whether reflect on border */
#define TEP_ROUGH		(1<<6) /* use less vertices */
#define TEP_THICK       (1<<7) /* enthicken polyline to 2 pixels wide */
#define TEP_THICKER     (2<<7) /* enthicken polyline to 3 pixels wide */
#define TEP_THICKEST    (3<<7) /* enthicken polyline to 4 pixels wide */
#define TEP_HEAD        (1<<9) /* append head graphic */
#define TEP_ADDBLEND    (0<<16) /* additive; default */
#define TEP_SOLIDBLEND  (1<<16) /* transparency mode */
#define TEP_ALPHABLEND  (2<<16) /* alpha indexed */
#define TEP_SUBBLEND    (3<<16) /* subtractive */
/* teline's forms can be used as well. */

/*=============================================================================
	type definitions
=============================================================================*/
typedef struct te_line_list tell_t;
typedef DWORD teline_flags_t, tefbox_flags_t, tefpol_flags_t;
typedef struct te_fillbox_list tefl_t;
typedef struct te_followpolyline_list tepl_t;
typedef struct te_circle_list tecl_t;

/* temporary entities output device info */
typedef struct te_output{
	HDC hdc;
	HBITMAP hbm;
	RECT clip;
	double trans[6]; /* affine transformation matrix applied on drawing */
	BITMAP bm;
	BITMAPINFO bmi;
} teout_t;


/******************************************************************************
	global variables
******************************************************************************/
//extern tell_t *g_ptell;
//extern tefl_t *g_ptefl;
#ifndef NDEBUG
/* debug information */
extern struct tetimes{
	size_t so_teline_t, so_tefbox_t, so_tefpol_t, so_tevert_t; /* sizeof's */
	double drawteline;
	double animteline;
	double drawtefbox;
	double animtefbox;
	double drawtefpol;
	double animtefpol;
	unsigned teline_c;
	unsigned teline_m;
	unsigned teline_s;
	unsigned tefbox_c;
	unsigned tefbox_m;
	unsigned tefbox_s;
	unsigned tefpol_c;
	unsigned tefpol_m;
	unsigned tefpol_s;
	unsigned tevert_c;
	unsigned tevert_s;
} g_tetimes;
#endif


/*-----------------------------------------------------------------------------
	function prototypes
-----------------------------------------------------------------------------*/
//int TentInit(void); /* return 0 if failed */
//int TentExit(void);
#if LANG==JP && !defined NDEBUG
struct GryphCacheUsageRet{
	unsigned long count;
	unsigned long bytes;
};
void GryphCacheUsage(struct GryphCacheUsageRet *);
#endif
#ifndef NDEBUG
struct VertexUsageRet{
	unsigned long count;
	unsigned long bytes;
	unsigned long maxcount;
};
void VertexUsage(struct VertexUsageRet *);
#endif

tell_t *NewTeline(unsigned maxt, unsigned init, unsigned unit); /* constructor */
void DelTeline(tell_t *); /* destructor */
void AddTeline(tell_t *tell, double x, double y, double vx, double vy, double len, double ang, double omg, double grv, COLORREF col, teline_flags_t flags, double life);
void AnimTeline(tell_t *tell, double dt);
void DrawTeline(teout_t *to, tell_t *tell);

tefl_t *NewTefbox(unsigned maxt, unsigned init, unsigned unit); /* allocator */
void DelTefbox(tefl_t *); /* deallocator */
void AddTefbox(tefl_t *p, int l, int t, int r, int b, COLORREF col, COLORREF destcol, double life, tefbox_flags_t flags);
void AddTefboxCustom(tefl_t *p, int l, int t, int r, int b, tefbox_flags_t flags, const struct color_sequence *inbox_color, const struct color_sequence *frame_color);
void AnimTefbox(tefl_t *p, double dt);
void DrawTefbox(teout_t *to, tefl_t *p);

tepl_t *NewTefpol(unsigned maxt, unsigned init, unsigned unit);
void DelTefpol(tepl_t *);
void AddTefpol(tepl_t *p, double x, double y, double vx, double vy, float grv, const colseq_t *col, tefpol_flags_t flags, double life);
void AnimTefpol(tepl_t *p, double dt);
void DrawTefpol(teout_t *to, tepl_t *p);

#endif /* TENT_H */

