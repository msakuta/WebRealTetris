#include "tent_p.h"
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include "dprint.h"
#include "debug.h"
#include <clib/stats.h>
#include "../lib/color.h"
#include "../lib/cs2.h"
#include "../lib/timemeas.h"


/*#############################################################################
	macro definitions
#############################################################################*/
#define ENABLE_FOLLOW 0 /* whether activate the following feature for teline */
#define ENABLE_ROUND 0 /* rounding values to nearest integral value instead of flooring */
#define DIRECTDRAW 1 /* whether draw into the buffer directly */

#define TEF_BLENDMASK (TEF_ADDBLEND|(TEF_ADDBLEND<<1))


/*=============================================================================
	type definitions
=============================================================================*/
/* the needless-to-explain line temporaly entity */
typedef struct te_line{
	double x, y; /* center point, double because the velo may be so slow */
	double vx, vy; /* velocity. */
	double len; /* length of the beam */
	double ang; /* angle of the beam */
	double omg; /* angle velocity */
	double life; /* remaining rendering time */
	double grv; /* gravity effect factor */
	union{ /* color definition depends on flags & TEL_FORMS */
		COLORREF r; /* color of the beam, black means random per rendering */
		const struct color_sequence *cs; /* TEL_GRADCIRCLE */
		const struct cs2 *cs2; /* TEL_GRADCIRCLE2 */
	} col;
	DWORD flags; /* entity options */
	struct te_line *next; /* list next pointer */
} teline_t;

/* static auto-decaying filled box, now it's not necesarily filled though. */
typedef struct te_fillbox{
	/* do not insert members before the 'ltrb' */
	LONG l, t; /* dimensions, are type of LONG, which is choosen for compatibility */
	LONG r, b; /* with RECT, which sounds a bit silly. */
	double max; /* color velo */
	double life; /* remaining rendering frames */
	tefbox_flags_t flags; /* option bitfields */
	struct te_fillbox *next; /* list next pointer */
	union{ /* switched by flags & TEF_CUSTOM */
		struct{
			COLORREF col;
			COLORREF cd;  /* destination color */
		} n;
		struct{
			const struct color_sequence *cf, *cb;
		} c;
	} u;
} tefbox_t;

/* teline does the job */
typedef struct te_circle{
	double x, y; /* center */
	double vx, vy;
	double rad;
	double dr;
	double grv;
	COLORREF col;
	unsigned life;
} tecirc_t;

/* this is a pointer list pool, which does not use heap allocation or shifting
  during the simulation which can be serious overhead. */
struct te_line_list{
	unsigned m; /* cap for total allocations of elements */
	unsigned ml; /* allocated elements in l */
	unsigned mex; /* allocated elements in an extra list */
	teline_t *l;
	teline_t *lfree, *lactv, *last;
	struct te_line_extra_list{
		struct te_line_extra_list *next;
		teline_t l[1]; /* variable */
	} *ex; /* extra space is allocated when entities hits their max count */
	unsigned bs; /* verbose buffer size */
};

struct te_fillbox_list{
	unsigned m; /* cap for total allocations of elements */
	unsigned ml; /* allocated elements in l */
	unsigned mex; /* allocated elements in an extra list */
	tefbox_t *l;
	tefbox_t *lfree, *lfill, *lflast, *lborder, *lblast;
	struct te_fbox_extra_list{
		struct te_fbox_extra_list *next;
		tefbox_t l[1]; /* variable */
	} *ex;
	unsigned bs; /* verbose buffer size */
};

/* queue-like data structure. actually it is not assured to remove the oldest
  element first, so strictly it is not a queue. when a non-oldest element is to
  be removed, all older elements are forced to shift, so it can be overhead. */
struct te_circle_list{
	unsigned begin; /* starting index of the buffer */
	unsigned cur; /* current appending index */
	tecirc_t l[MAX_TENT];
};


/******************************************************************************
	global variables
******************************************************************************/
//static tell_t tell = {512, 256, 128, 0, 0, NULL};
//static tefl_t tefl = {128, 64, 32, 0, 0, NULL};
//tell_t *g_ptell = &tell;
//tefl_t *g_ptefl = &tefl;
#if LANG==JP
static const char *s_cook[] = {
	"Ä", "àu", "ö", "—g", "•¦",
};
static const char *s_meat[] = {
	"‹", "Œ{", "“Ø", "Œ~", "Œ¢",   "äY", "Ž­", "’–", "”n", "—r",
	"‘â", "ˆñ", "–Ž", "ø", "ŠI",   "éÍ",
};
static const char *s_twelve[] = {
	"Žq", "‰N", "“Ð", "‰K", "’C",   "–¤", "Œß", "–¢", "\", "“Ñ",
	"œú", "ˆå",
}, *s_ten[] = {
	"b", "‰³", "•¸", "’š", "•è",   "ŒÈ", "M", "h", "p", "á¡"
};
static const char *s_destroy[] = {
	"”š", "—ó", "â", "–Å", "‰ó",   "Š…", "‘s", "•ö", "Ÿr", "Œ",
	"—…", "”j", "’f"
};
static const char *s_script[] = {
	"F", "‘¦", "¥", "‹ó", "–³",   "‘z", "^", "Šä", "ŽÉ", "—˜",
	"Žq"
}, *s_fireworks[] = {
	"Žõ", "—Á", "Á", "°", "¯",   "´", "“V", "–Á", "‰Ø", "N",
	"^", "•Ÿ", "K", "—í", "Šð",   "Šˆ", "”¬", "à", "•ó", "•x",
	"’W", "Š…", "æÒ", "‰õ", "Šy",   "‰‹", "‰", "—d", "‰ƒ", "–G",
	"–G", "–G", "–G", "¼", "”ö",   "ò"
}, *s_mahjong[] = {
	"”’", "á¢", "’†", "äÝ",
};
#endif
#ifndef NDEBUG
struct tetimes g_tetimes/* = {sizeof teline_t, sizeof tefbox_t}*/;
#endif


/*-----------------------------------------------------------------------------
	function definitions
-----------------------------------------------------------------------------*/
#if 0
static int TentInit(void){
	int i;
	if(INIT_TELINE && NULL == (tell.l = malloc(INIT_TELINE * sizeof *tell.l))
	|| INIT_TEFBOX && NULL == (tefl.l = malloc(INIT_TEFBOX * sizeof *tefl.l))) return 0;
	tell.m = MAX_TELINE;
	tell.ml = tell.bs = INIT_TELINE;
	tell.mex = UNIT_TELINE;
	tell.lfree = tell.l;
	tell.lactv = tell.last = NULL;
	for(i = 0; i < (int)(INIT_TELINE - 1); i++)
		tell.l[i].next = &tell.l[i+1];
	if(INIT_TELINE) tell.l[INIT_TELINE-1].next = NULL;
	tefl.m = MAX_TEFBOX;
	tefl.ml = tefl.bs = INIT_TEFBOX;
	tefl.mex = UNIT_TEFBOX;
	tefl.lfree = tefl.l;
	tefl.lfill = tefl.lborder = NULL;
	for(i = 0; i < (int)(INIT_TEFBOX - 1); i++)
		tefl.l[i].next = &tefl.l[i+1];
	if(INIT_TEFBOX) tefl.l[INIT_TEFBOX-1].next = NULL;
#ifndef NDEBUG
	g_tetimes.tefbox_m = INIT_TEFBOX;
	g_tetimes.teline_m = INIT_TELINE;
	dprintf(
		"sizeof(teline_t) = %u, sizeof(tefbox_t) = %u\n"
		"line Count: line %u/%u, fbox %u/%u\n"
		"Memory Usage: line %u/%u, fbox %u/%u\n",
		sizeof(teline_t), sizeof(tefbox_t),
		INIT_TELINE, MAX_TELINE, INIT_TEFBOX, MAX_TEFBOX,
		INIT_TELINE * sizeof *tell.l, MAX_TELINE * sizeof *tell.l,
		INIT_TEFBOX * sizeof *tefl.l, MAX_TEFBOX * sizeof *tefl.l);
#endif
	return 1;
}
#endif

/* recursive functions to free extra allocations */
static void freeTelineExtra(struct te_line_extra_list *p){
	if(p->next) freeTelineExtra(p->next);
	free(p);
}

static void freeTefboxExtra(struct te_fbox_extra_list *p){
	if(p->next) freeTefboxExtra(p->next);
	free(p);
}

#if 0
static int TentExit(void){
	if(tell.l) free(tell.l), tell.l = NULL;
	if(tell.ex) freeTelineExtra(tell.ex), tell.bs = 0;
	if(tefl.l) free(tefl.l), tefl.l = NULL;
	if(tefl.ex) freeTefboxExtra(tefl.ex), tefl.bs = 0;
	return 1;
}
#endif

/* allocator and constructor */
tell_t *NewTeline(unsigned maxt, unsigned init, unsigned unit){
	/* it's silly that max is defined as a macro */
	int i;
	tell_t *ret;
	ret = malloc(sizeof *ret);
	if(!ret) return NULL;
	ret->lfree = ret->l = malloc(init * sizeof *ret->l);
	if(!ret->l) return NULL;
	ret->m = maxt;
	ret->ml = ret->bs = init;
	ret->mex = unit;
	ret->lactv = ret->last = NULL;
	ret->ex = NULL;
	for(i = 0; i < (int)(init - 1); i++)
		ret->l[i].next = &ret->l[i+1];
	if(init) ret->l[init-1].next = NULL;
#ifndef NDEBUG
	g_tetimes.so_teline_t = sizeof(teline_t);
	g_tetimes.teline_m = ret->bs;
	dprintf(
		"sizeof(teline_t) = %u\n"
		"line Count: %u/%u\n"
		"Memory Usage for Teline: %u/%u\n"
		"Total Memory Usage: %u\n",
		sizeof(teline_t),
		init, maxt,
		init * sizeof *ret->l, maxt * sizeof *ret->l,
		TOTALMEMORY);
#endif
	return ret;
}

/* destructor and deallocator */
void DelTeline(tell_t *p){
	if(p->l) free(p->l);
	if(p->ex) freeTelineExtra(p->ex);
	free(p);
}

void AddTeline(tell_t *p, double x, double y, double vx, double vy,
	double len, double ang, double omg, double grv,
	COLORREF col, teline_flags_t flags, double life)
{
	teline_t *pl;
	if(!p->m) return;
	pl = p->lfree;
	if(!pl){
		if(p->mex && p->bs + p->mex <= p->m){
			struct te_line_extra_list *ex;
			unsigned i;
			dprintf("teline.bs = %u + (%u)%u = %u; %u\n", p->bs * sizeof *p->l,
				offsetof(struct te_line_extra_list, l), offsetof(struct te_line_extra_list, l) + p->mex * sizeof *ex->l,
				(p->bs + p->mex) * sizeof *p->l, TOTALMEMORY);
			ex = malloc(offsetof(struct te_line_extra_list, l) + p->mex * sizeof *ex->l); /* struct hack alloc */
			ex->next = p->ex;
			p->ex = ex;
			p->lfree = ex->l;
			for(i = 0; i < p->mex-1; i++)
				ex->l[i].next = &ex->l[i+1];
			ex->l[p->mex-1].next = NULL;
			pl = p->lfree;
			p->bs += p->mex;
#ifndef NDEBUG
			g_tetimes.teline_m = p->bs;
#endif
		}
		else{
			pl = p->lactv; /* reuse the oldest active node */
			if(!pl) return;

			/* roll pointers */
			p->last->next = p->lactv;
			p->last = p->lactv;
			p->lactv = p->lactv->next;
			pl->next = NULL;
		}
	}
	else{ /* allocate from the free list. */
		p->lfree = pl->next;
		pl->next = p->lactv;
		p->lactv = pl;
		if(!pl->next) p->last = pl;
	}
	/* filter these bits a little. */
	if(flags & TEL_NOLINE) flags &= ~TEL_HEADFORWARD;
	if((flags & TEL_FORMS) == TEL_POINT) flags &= ~TEL_VELOLEN;

#define tel (*pl)
	tel.x = x;
	tel.y = y;
	tel.vx = vx;
	tel.vy = vy;
	tel.len = len;
	tel.ang = ang;
	tel.omg = omg;
	tel.grv = (float)grv;
	tel.col.r = col;
	tel.flags = flags;
	tel.life = life;
#undef tel

#ifndef NDEBUG
	g_tetimes.teline_s++;
#endif
}

/* animate every line */
void AnimTeline(tell_t *p, double dt){
	STARTTIMER(animteline, &g_tetimes.animteline)
//	unsigned i, j;
//	i = p->begin;
	teline_t *pl = p->lactv, **ppl = &p->lactv;
	while(pl){
		teline_t *pl2;
		double xlen, ylen;
//		teline_t *pl = &p->l[i];
		pl->life -= dt;

		/* delete condition */
		if(pl->life < 0 ||
			!(pl->flags & (TEL_NOREMOVE /*| TEL_RETURN | TEL_REFLECT*/)) &&
			((((pl->flags & TEL_NOLINE) && (pl->flags & TEL_FORMS) == TEL_POINT)) &&
			(pl->x < 0 || XSIZ < pl->x || pl->y < 0 || YSIZ < pl->y)) ||
			(xlen = ylen = pl->flags & TEL_VELOLEN ? (ABS(pl->vx) + ABS(pl->vy)) * pl->len/2 : pl->len/2,
			(pl->x + xlen < 0 || XSIZ < pl->x - xlen || pl->y + ylen < 0 || YSIZ < pl->y - ylen))){
			/* check if the bonding box hits the draw area. */

			/* roll pointers */
			if(pl == p->last && *ppl != p->lactv)
				p->last = (teline_t*)((byte*)ppl - offsetof(teline_t, next));
			pl2 = pl->next;
			*ppl = pl->next;
			pl->next = p->lfree;
			p->lfree = pl;
		}
		else{
			ppl = &pl->next;
			pl2 = pl->next;
		}
#if ENABLE_FOLLOW
		if(!pl->flags & TEL_FOLLOW){
#endif
		pl->vy += GRAVITY * pl->grv * dt; /* apply gravity */
		pl->x += pl->vx * dt;
		pl->y += pl->vy * dt;

		/* reappear from opposite edge. prior than reflect */
		if(pl->flags & TEL_RETURN){
			if(pl->x < 0)
				pl->x += XSIZ;
			else if(XSIZ < pl->x)
				pl->x -= XSIZ;
			if(pl->y < 0)
				pl->y += YSIZ;
			else if(YSIZ < pl->y)
				pl->y -= YSIZ;
		}
		/* rough approximation of reflection. rigid body dynamics are required for more reality. */
		else if(pl->flags & TEL_REFLECT){
			int j = 0; /* reflected at least once */
			if(pl->x + pl->vx * dt < 0 || XSIZ < pl->x + pl->vx * dt)
				pl->vx *= - REFLACTION, j = 1;
			if(pl->y + pl->vy * dt < 0 || YSIZ < pl->y + pl->vy * dt)
				pl->vy *= - REFLACTION, j = 1;
			if(j && !(pl->flags & (TEL_HEADFORWARD | TEL_STAR)))
				pl->omg *= - REFLACTION;
		}

		/* redirect its head to where the line going to go */
		if(pl->flags & TEL_HEADFORWARD)
			pl->ang = atan2(pl->vy, pl->vx) + pl->omg;
		else if(!(pl->flags & TEL_FORMS))
			pl->ang = fmod(pl->ang + pl->omg * dt, 2 * M_PI);
#if ENABLE_FOLLOW
		}
		else{
			/* redirect its head to where the target ent going to go */
			if(pl->flags & TEL_HEADFORWARD)
				pl->ang = atan2(((teline_t*)&pl->vx)->vy, ((teline_t*)&pl->vx)->vx) + pl->omg;
			else if(!(pl->flags & TEL_FORMS))
				pl->ang = fmod(pl->ang + pl->omg * dt, 2 * M_PI);
		}
#endif

		pl = pl2;
	}
	STOPTIMER(animteline);
#ifndef NDEBUG
	if(p->ml){
	unsigned int i = 0;
//	struct te_line_extra_list *ex = p->ex;
	teline_t *pl = p->lactv;
	while(pl) i++, pl = pl->next;
	g_tetimes.teline_c = i;
//	i = 1;
//	while(ex) i++, ex = ex->next;
//	g_tetimes.teline_m = i * p->m;
	}
#endif
}

#define MAX_FONTSIZE 26

/* this function keeps stock fonts internally */
static HFONT GetSizeFont(int size){
	static HFONT s_fonts[MAX_FONTSIZE] = {0};
	HFONT *const fonts = &s_fonts[-1], f; /* 0 is inaccessible */
	if(1 <= size && size < MAX_FONTSIZE+1 && fonts[size]) return fonts[size];
	else f = CreateFont(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
	if(1 <= size && size < MAX_FONTSIZE+1) fonts[size] = f;
	return f;
}

#if DIRECTDRAW
static int Line(const teout_t *to, const teline_t *pl, int x, int y, int ox, int oy, COLORREF cc){
	int i, xp, yp, delta = MAX(ABS(ox - x), ABS(oy - y));
	const RECT r = {MAX(to->clip.left, 0), MAX(to->clip.top, 0), MIN(to->clip.right, to->bm.bmWidth), MIN(to->clip.bottom, to->bm.bmHeight)};
	const RECT tr = {MIN(x, ox), MIN(y, oy), MAX(x, ox), MAX(y, oy)};

	/* since transformation matrix is introduced, there're big chances a line is not in the output screen. */
	if(RECTINTERSECT(r, tr)) for(i = delta; i; i--){
		xp = (ox * i + x * (delta - i)) / delta;
		yp = (oy * i + y * (delta - i)) / delta;
		if(r.left <= xp && xp < r.right && r.top <= yp && yp < r.bottom){
			COLORREF *const tc = (COLORREF*)&((BYTE*)to->bm.bmBits)[xp * 4 + yp * to->bm.bmWidthBytes];
#	if ENABLE_ADDBLEND
#	define GetAValue(col) ((col)>>24)
			switch(pl->flags & TEF_BLENDMASK){
			case TEF_ADDBLEND:
				*tc = RGB(
					MIN(255,GetRValue(*tc)+GetBValue(cc)),
					MIN(255,GetGValue(*tc)+GetGValue(cc)),
					MIN(255,GetBValue(*tc)+GetRValue(cc)));
				break;
			case TEF_ALPHABLEND:{
				int a = 255-GetAValue(cc);
				*tc = RGB(
					((255-a)*GetRValue(*tc)+a*GetBValue(cc))/256,
					((255-a)*GetGValue(*tc)+a*GetGValue(cc))/256,
					((255-a)*GetBValue(*tc)+a*GetRValue(cc))/256);
				break;
			}
			case TEF_SUBBLEND:
				*tc = RGB(
					MAX(0,GetRValue(*tc)-GetBValue(cc)),
					MAX(0,GetGValue(*tc)-GetGValue(cc)),
					MAX(0,GetBValue(*tc)-GetRValue(cc)));
				break;
			case TEF_SOLIDBLEND:
				*tc = RGB(GetBValue(cc),GetGValue(cc),GetRValue(cc));
				break;
			}
#	else
			*tc = RGB(GetBValue(cc),GetGValue(cc),GetRValue(cc));
#	endif
		}
	}
	return 0;
}
#else
static int Line(const teout_t *to, const teline_t *pl, int x1, int y1, int x2, int y2, COLORREF c){
	HPEN p, po;
	po = SelectObject(to->hdc, p = CreatePen(PS_SOLID, 0, c));
	MoveToEx(to->hdc, x1, y1, NULL);
	LineTo(to->hdc, x2, y2);
	SelectObject(to->hdc, po);
	DeleteObject(p);
	return 0;
}
#endif


/* draw lines */
void DrawTeline(teout_t *to, tell_t *p){
	timemeas_t tm;
	TimeMeasStart(&tm);
	{
	double br, lenb;
	unsigned char buf[3];
	teline_t *pl = p->lactv;
	while(pl){
		DWORD form = pl->flags & TEL_FORMS;

		/* gradiated fills are far heavier load than those simple lines, so always direct draw. */
		if(form == TEL_GRADCIRCLE){
			int x, y, xmax = (int)MIN(to->bm.bmWidth-1, pl->x + pl->len), ymax = (int)MIN(to->bm.bmHeight-1, pl->y + pl->len);
			double r2;
			for(x = (int)MAX(0, pl->x - pl->len); x <= xmax; x++) for(y = (int)MAX(0, pl->y - pl->len); y <= ymax; y++) if((r2 = (x - pl->x) * (x - pl->x) + (y - pl->y) * (y - pl->y)) < pl->len * pl->len){
				COLORREF col = ColorSequence(pl->col.cs, sqrt(r2) / pl->len * pl->col.cs->t);
				COLORREF *dst = BITMAPBITS(to->bm, x, y), dc = SWAPRB(*dst);
				if(pl->life < SHRINK_START) col = COLOR32SCALE(col, pl->life / SHRINK_START * 255);
				dc = pl->col.cs->alpha ? COLOR32AADD(dc, col) : COLOR32ADD(dc, col);
				*dst = SWAPRB32(dc);
			}
		}
		else if(form == TEL_GRADCIRCLE2){
			int x, y, xmax = (int)MIN(to->bm.bmWidth-1, pl->x + pl->len), ymax = (int)MIN(to->bm.bmHeight-1, pl->y + pl->len);
			double r2;
/*			for(x = (int)MAX(0, pl->x - pl->len); x <= xmax; x++) for(y = (int)MAX(0, pl->y - pl->len); y <= ymax; y++) if((r2 = (x - pl->x) * (x - pl->x) + (y - pl->y) * (y - pl->y)) < pl->len * pl->len){
				COLORREF col = ColorSequence2(pl->col.cs2, pl->life, sqrt(r2) / pl->len);
				COLORREF *dst = BITMAPBITS(to->bm, x, y), dc = SWAPRB(*dst);
				if(pl->life < SHRINK_START) col = COLOR32SCALE(col, pl->life / SHRINK_START * 255);
				dc = pl->col.cs->alpha ? COLOR32AADD(dc, col) : COLOR32ADD(dc, col);
				*dst = SWAPRB32(dc);
			}*/
		}
		else{
		COLORREF col;
		br = MIN(pl->life / FADE_START, 1.0);
		if(pl->col.r == WG_BLACK){
			buf[0] = (unsigned char)(br * (double)(rand() % 256));
			buf[1] = (unsigned char)(br * (double)(rand() % 256));
			buf[2] = (unsigned char)(br * (double)(rand() % 256));
		}
		else{
			buf[0] = (unsigned char)(br * GetRValue(pl->col.r));
			buf[1] = (unsigned char)(br * GetGValue(pl->col.r));
			buf[2] = (unsigned char)(br * GetBValue(pl->col.r));
		}
		col = RGB(buf[0], buf[1], buf[2]);

		/* shrink over its lifetime */
		if(pl->flags & TEL_SHRINK && pl->life < SHRINK_START)
			lenb = pl->len / SHRINK_START * pl->life;
		else
			lenb = pl->len;

		/* recalc length by its velocity. */
		if(pl->flags & TEL_VELOLEN)
			lenb *= sqrt(pl->vx * pl->vx + pl->vy * pl->vy);

#if ENABLE_ROUND
		if(g_pinit->RoundDouble)
#endif
		{
		double x, y;
#if ENABLE_FOLLOW
		if(pl->flags & TEL_FOLLOW){
			x = (((teline_t*)&pl->vx)->x + pl->x);
			y = (((teline_t*)&pl->vx)->y + pl->y);
		}
		else
#endif
		{
			x = fmod(to->trans[0] * pl->x + to->trans[2] * pl->y + to->trans[4], to->clip.right - to->clip.left);
			y = fmod(to->trans[1] * pl->y + to->trans[3] * pl->y + to->trans[5], to->clip.bottom - to->clip.top);
		}
		{
			teline_flags_t forms = pl->flags & TEL_FORMS;
			if(forms == TEL_CIRCLE){
				HPEN hp, ohp;
				ohp = SelectObject(to->hdc, CreatePen(0, PS_SOLID, col));
				Ellipse(to->hdc, (int)(x - lenb), (int)(y - lenb), (int)(x + lenb), (int)(y + lenb));
				DeleteObject(SelectObject(to->hdc, ohp));
/*				CircleWin(pwg, x, y, (int)(lenb + .5), col);*/
			}
			else if(forms == TEL_FILLCIRCLE){
#if LANG==EN
				HBRUSH hb, ohb;
				ohb = SelectObject(to->hdc, CreateSolidBrush(col));
				Ellipse(to->hdc, (int)(x - lenb), (int)(y - lenb), (int)(x + lenb), (int)(y + lenb));
				DeleteObject(SelectObject(to->hdc, ohb));
#else
				int i, j, n;
/*				HFONT hFont;
				hFont = CreateFont((int)(lenb + .5), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
				if(!hFont) break;
				SelectObject(pwg->hVDC, hFont);
				SetTextColor(pwg->hVDC, col);
				TextOut(pwg->hVDC, x-lenb/2, y-lenb/2, s_destroy[((int)pl>>3)%numof(s_destroy)], 2);
				DeleteObject(hFont);*/
				drawgryph(to,
					newgryph(s_fireworks[(int)pl%numof(s_fireworks)], lenb/*, RGB(0,0,0)*/),
					x-lenb/2, y-lenb/2, col);
#endif
			}
			else if(forms == TEL_HEXAGRAPH){
#if LANG==EN
#else
/*				HFONT hFont;
				hFont = CreateFont((int)(lenb + .5), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
				if(!hFont) break;
				SelectObject(pwg->hVDC, hFont);
				SetTextColor(pwg->hVDC, col);
				TextOut(pwg->hVDC, x-lenb/2, y-lenb/2, s_fireworks[(int)pl%numof(s_fireworks)], 2);
				DeleteObject(hFont);*/
				drawgryph(to,
					newgryph(s_fireworks[(int)pl%numof(s_fireworks)], lenb/*, RGB(0,0,0)*/),
					x-lenb/2, y-lenb/2, col);
#endif
			}
			else{ /* if(forms == TEL_POINT) */
				int ix = (int)x, iy = (int)y;
		/*		PutPointWin(pwg, x, y, col);*/
				if(RECTHIT(to->clip, ix, iy) && 0 < x && x < to->bm.bmWidth && 0 < y && y < to->bm.bmHeight)
					*BITMAPBITS(to->bm, ix, iy) = col;
			}
		}

		if(!(pl->flags & TEL_NOLINE)){
			double x1, y1, x2, y2;
			x1 = cos(pl->ang) * lenb;
			y1 = sin(pl->ang) * lenb;
			x2 = DTRANSX(to->trans, x1);
			y2 = DTRANSY(to->trans, y1);

			if(pl->flags & TEL_HALF)
				Line(to, pl, x + x2, y + y2, x, y, col);
			else
				Line(to, pl, x + x2, y + y2, x - x2, y - y2, col);

			if(pl->flags & TEL_STAR){ /* draw crossing line */
				/* rotate 90 degrees BEFORE transforming */
/*				x2 = (x - cos(pl->ang + M_PI / 2) * lenb +.5),
				y2 = (y - sin(pl->ang + M_PI / 2) * lenb +.5); */
				x2 = DTRANSX(to->trans, y1);
				y2 = DTRANSY(to->trans, -x1);

				Line(to, pl, x + x2, y + y2, x - x2, y - y2, col);
			}
		}
		}
#if ENABLE_ROUND
		else{
		{
			teline_flags_t forms = pl->flags & TEL_FORMS;
			if(forms == TEL_CIRCLE)
				CircleWin(pwg, (int)pl->x, (int)pl->y, (int)lenb, col);
			else if(forms == TEL_FILLCIRCLE){
#if LANG==EN
				FillCircleWin(pwg, (int)pl->x, (int)pl->y, (int)lenb, col);
#else
				HFONT hFont;
				char *s;
				hFont = CreateFont(lenb, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
				if(!hFont) break;
				SelectObject(pwg->hVDC, hFont);
				SetTextColor(pwg->hVDC, col);
				TextOut(pwg->hVDC, pl->x, pl->y, s_destroy[((int)pl>>3)%numof(s_destroy)], 2);
				DeleteObject(hFont);
#endif
			}
			else if(forms == TEL_HEXAGRAPH){
#if LANG==EN
#else
				HFONT hFont;
				hFont = CreateFont((int)lenb, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
				if(!hFont) break;
				SelectObject(pwg->hVDC, hFont);
				SetTextColor(pwg->hVDC, col);
				TextOut(pwg->hVDC, (int)pl->x, (int)pl->y, s_fireworks[((int)pl)%numof(s_fireworks)], 2);
				DeleteObject(hFont);
#endif
			}			else /* if(forms == TEL_POINT) */
				PutPointWin(pwg, (int)pl->x, (int)pl->y, col);
		}

		if(!(pl->flags & TEL_NOLINE)){
		if(pl->flags & TEL_HALF)
		LineWin(pwg,
		 (int)(pl->x + cos(pl->ang) * lenb),
		 (int)(pl->y + sin(pl->ang) * lenb),
		 (int)pl->x,
		 (int)pl->y,
		 col);
		else
		LineWin(pwg,
		 (int)(pl->x + cos(pl->ang) * lenb),
		 (int)(pl->y + sin(pl->ang) * lenb),
		 (int)(pl->x - cos(pl->ang) * lenb),
		 (int)(pl->y - sin(pl->ang) * lenb),
		 col);

		if(pl->flags & TEL_STAR) /* draw crossing line */
		LineWin(pwg,
		 (int)(pl->x + cos(pl->ang + M_PI / 2) * lenb),
		 (int)(pl->y + sin(pl->ang + M_PI / 2) * lenb),
		 (int)(pl->x - cos(pl->ang + M_PI / 2) * lenb),
		 (int)(pl->y - sin(pl->ang + M_PI / 2) * lenb),
		 col);
		}
		}
#endif
		}

		pl = pl->next;
	}
	}
#ifndef NDEBUG
	g_tetimes.drawteline = TimeMeasLap(&tm);
#endif
}

/* allocator and constructor */
tefl_t *NewTefbox(unsigned maxt, unsigned init, unsigned unit){
	/* it's silly that max is defined as a macro */
	int i;
	tefl_t *ret;
	ret = malloc(sizeof *ret);
	if(!ret) return NULL;
	ret->lfree = ret->l = malloc(init * sizeof *ret->l);
	if(!ret->l) return NULL;
	ret->m = maxt;
	ret->ml = ret->bs = init;
	ret->mex = unit;
	ret->lborder = ret->lblast = ret->lfill = ret->lflast = NULL;
	ret->ex = NULL;
	for(i = 0; i < (int)(init - 1); i++)
		ret->l[i].next = &ret->l[i+1];
	if(init) ret->l[init-1].next = NULL;
#ifndef NDEBUG
	g_tetimes.so_tefbox_t = sizeof(tefbox_t);
	g_tetimes.tefbox_m = ret->bs;
	dprintf(
		"sizeof(tefbox_t) = %u\n"
		"fbox Count: %u/%u\n"
		"Memory Usage for Tefbox: %u/%u\n"
		"Total Memory Usage: %u\n",
		sizeof(tefbox_t),
		init, maxt,
		init * sizeof *ret->l, maxt * sizeof *ret->l,
		TOTALMEMORY);
#endif
	return ret;
}

/* destructor and deallocator */
void DelTefbox(tefl_t *p){
	if(p->l) free(p->l);
	if(p->ex) freeTefboxExtra(p->ex);
	free(p);
}

/* common routine for AddTefbox and AddTefboxCustom */
static tefbox_t *allocTefbox(tefl_t *p, tefbox_flags_t flags){
	if(!p->lfree){ /* if free list is empty, deallocate one of the active lists. */
		if(p->mex && p->bs + p->mex <= p->m){
			struct te_fbox_extra_list *ex;
			unsigned i;
			dprintf("tefbox.bs = %u + (%u)%u = %u; %u\n", p->bs * sizeof *p->l,
				offsetof(struct te_fbox_extra_list, l[0]),
				offsetof(struct te_fbox_extra_list, l[0]) + p->mex * sizeof *ex->l,
				(p->bs + p->mex) * sizeof *p->l, TOTALMEMORY);
			ex = malloc(offsetof(struct te_fbox_extra_list, l) + p->mex * sizeof *ex->l);
			ex->next = p->ex;
			p->ex = ex;
			p->lfree = ex->l;
			for(i = 0; i < p->mex-1; i++)
				ex->l[i].next = &ex->l[i+1];
			ex->l[p->mex-1].next = NULL;
			p->bs += p->mex;
#ifndef NDEBUG
			g_tetimes.tefbox_m = p->bs;
#endif
		}
		else{
			/* try to free border list first, then free fill list if border list is empty as well.
			border is less significant when there're lots of effects. */
			tefbox_t **pp = p->lborder ? &p->lborder : &p->lfill, *pb = *pp;
			if(!pb) return NULL;

			/* reading through to end is not necessary because the head is the oldest. */
	//		while(pp->next) pp = pp->next;

			/* roll pointers */
			*pp = pb->next;
			pb->next = p->lfree;
			p->lfree = pb;
		}
	}
	p->lfree->flags = flags;
	{
	/* if BORDER flag is on, allocate in border list. */
	tefbox_t **pp = flags & TEF_BORDER ? &p->lborder : &p->lfill;
	while(*pp) pp = &(*pp)->next;

	/* roll pointers */
	*pp = p->lfree;
	p->lfree = p->lfree->next;
	(*pp)->next = NULL;
#ifndef NDEBUG
	g_tetimes.tefbox_s++;
#endif
	return *pp;
	}
}

/* easy use version; color sequence and bordered fillbox are not available. */
void AddTefbox(tefl_t *p, int l, int t, int r, int b, COLORREF col, COLORREF cd, double life, tefbox_flags_t flags){

	tefbox_t *pb = allocTefbox(p, flags);
	if(!pb) return;
	assert(pb && !(flags & TEF_CUSTOM));
	pb->l = l;
	pb->t = t;
	pb->r = r;
	pb->b = b;
	pb->u.n.col = col;
	pb->u.n.cd = cd;
	pb->max = pb->life = life;
}

/* advanced use version; color sequence and bordered fillbox are available. */
void AddTefboxCustom(tefl_t *p, int l, int t, int r, int b, tefbox_flags_t flags,
					 const struct color_sequence *cs1, const struct color_sequence *cs2){
	tefbox_t *pb = allocTefbox(p, flags | TEF_CUSTOM);
	if(!pb) return;
	pb->l = l;
	pb->t = t;
	pb->r = r;
	pb->b = b;
	pb->u.c.cf = cs1;
	pb->u.c.cb = cs2;

	/* calculate life to remove on appropriate time in AnimTefbox. */
	{
	double l1 = 0., l2 = 0.;
	unsigned i;
	if(cs1) for(i = 0; i < cs1->c; i++) l1 += cs1->p[i].t;
	if(cs2) for(i = 0; i < cs2->c; i++) l2 += cs2->p[i].t;
	pb->life = pb->max = MAX(l1, l2);
	}
}

void AnimTefbox(tefl_t *p, double dt){
	STARTTIMER(animtefbox, &g_tetimes.animtefbox)
	int run = 0;
	tefbox_t *pb, **ppb;
	pb = p->lfill;
	ppb = &p->lfill;
	do {
		while(pb){
			pb->life -= dt;

			/* delete condition */
			if(pb->life < 0){
				/* roll pointers */
				tefbox_t *pb2 = pb->next;
				*ppb = pb->next;
				pb->next = p->lfree;
				p->lfree = pb;
				pb = pb2;
			}
			else{
				ppb = &pb->next;
				pb = pb->next;
			}
		}
		if(run) break;
		pb = p->lborder;
		ppb = &p->lborder;
		run = 1;
	} while(1);
	STOPTIMER(animtefbox);
#ifndef NDEBUG
	{
	unsigned int i = 0;
//	struct te_fbox_extra_list *ex = p->ex;
	tefbox_t *pb = p->lfill;
	while(pb) i++, pb = pb->next;
	pb = p->lborder;
	while(pb) i++, pb = pb->next;
	g_tetimes.tefbox_c = i;
//	i = 1;
//	while(ex) i++, ex = ex->next;
//	g_tetimes.tefbox_m = i * p->m;
	}
#endif
}

/* retrieve color from a color sequence at a specified elapsed time. */
COLORREF ColorSequence(const struct color_sequence *cs, double time){
	unsigned j;
	COLORREF dstc;
	byte srcc[3];
	assert(cs->c != 0);
	for(j = 0; j < cs->c && time >= 0; j++) time -= cs->p[j].t;
	j--;
	/* by default, disappear into black. This behavior can be changed by appending 0
	  timed color node at the end of the sequence. */
	dstc = j == cs->c-1 ? 0xff000000 : cs->p[j+1].col;
	time = cs->p[j].t ? (time + cs->p[j].t) / cs->p[j].t : 0.5;
	if(time > 1.) time = 1.;
	if(cs->p[j].col == cs->colrand)
		srcc[0] = (byte)rand(), srcc[1] = (byte)rand() & 0xff, srcc[2] = (byte)rand() & 0xff;
	else
		srcc[0] = GetRValue(cs->p[j].col),
		srcc[1] = GetGValue(cs->p[j].col),
		srcc[2] = GetBValue(cs->p[j].col);
	return RGB((srcc[0] * (1.-time) + GetRValue(dstc) * time),
			(srcc[1] * (1.-time) + GetGValue(dstc) * time),
			(srcc[2] * (1.-time) + GetBValue(dstc) * time))
			| (cs->alpha ? (COLORREF)((cs->p[j].col>>24) * (1.-time) + (dstc>>24) * time) << 24 : 0);
}

static void outlineRect(teout_t *to, RECT *r, COLORREF c){
	int i;
	if(to->bm.bmBitsPixel != 32){
		assert(0);
		return;
	}
	for(i = r->left; i < r->right; i++){
		*(COLORREF*)&((char*)to->bm.bmBits)[i * sizeof(COLORREF) + r->top * to->bm.bmWidthBytes] = c;
		*(COLORREF*)&((char*)to->bm.bmBits)[i * sizeof(COLORREF) + (r->bottom-1) * to->bm.bmWidthBytes] = c;
	}
	for(i = r->top+1; i < r->bottom-1; i++){
		*(COLORREF*)&((char*)to->bm.bmBits)[r->left * sizeof(COLORREF) + i * to->bm.bmWidthBytes] = c;
		*(COLORREF*)&((char*)to->bm.bmBits)[(r->right-1) * sizeof(COLORREF) + i * to->bm.bmWidthBytes] = c;
	}
}

extern DWORD swaprgb(DWORD);
extern void AddConstBox(COLORREF *pDest, COLORREF value, int x, int y, int dx, int dy, int w);

void DrawTefbox(teout_t *to, tefl_t *p){
	timemeas_t tm;
	TimeMeasStart(&tm);
	{
	tefbox_t *pb = p->lfill;
#if DIRECTDRAW
	BITMAP *pbm;
	if(!pb) goto retur;
	pbm = &to->bm;
	if(pbm->bmBitsPixel != 32) goto retur;
#endif
	while(pb){
		if(pb->flags & TEF_CUSTOM){
			if(pb->u.c.cf){
#if DIRECTDRAW
				int x, y;
				COLORREF cf;
				cf = ColorSequence(pb->u.c.cf, pb->max - pb->life);
				//cf = RGB(GetBValue(cf),GetGValue(cf),GetRValue(cf)); /* swap bytes */
				cf = swaprgb(cf);
				if((pb->flags & TEF_BLENDMASK) == TEF_ADDBLEND){
					AddConstBox(pbm->bmBits, cf, pb->l, pb->t, pb->r - pb->l, pb->b - pb->t, pbm->bmWidthBytes);
				}
				for(x = pb->l; x < pb->r; x++) for(y = pb->t; y < pb->b; y++) if(BITMAPHIT(*pbm, x, y)){
					COLORREF *const tmp = (COLORREF*)&((BYTE*)pbm->bmBits)[x * sizeof(COLORREF) + y * pbm->bmWidthBytes];
					switch(pb->flags & TEF_BLENDMASK){
					case TEF_SOLIDBLEND:
						*tmp = cf;
						break;
/*					case TEF_ADDBLEND:
						*tmp = ADDBLEND(*tmp,cf);
						break;*/
					case TEF_SUBBLEND:
						*tmp = SUBBLEND(*tmp,cf);
						break;
					case TEF_ALPHABLEND:
						*tmp = ALPHABLEND(*tmp,cf,255-(cf>>24));
					}
				}
#else
				FillRectangleWin(pwg, pb->l, pb->t, pb->r, pb->b, ColorSequence(pb->u.c.cf, pb->max - pb->life));
#endif
			}
			if(pb->u.c.cb){
#if DIRECTDRAW
				int x, y;
				COLORREF cb;
				cb = ColorSequence(pb->u.c.cb, pb->max - pb->life);
//				cb = RGB(GetBValue(cb),GetGValue(cb),GetRValue(cb)); /* swap bytes */
				cb = swaprgb(cb);
				switch(pb->flags & TEF_BLENDMASK == TEF_ADDBLEND){
				case TEF_SOLIDBLEND:
					for(x = pb->l; x < pb->r; x++){
						COLORREF *tmp = BITMAPBITS(*pbm, x, pb->t);
						if(BITMAPHIT(*pbm, x, pb->t))
							*tmp = cb;
						tmp = BITMAPBITS(*pbm, x, pb->b);
						if(BITMAPHIT(*pbm, x, pb->b))
							*tmp = cb;
					}
					for(y = pb->t+1; y < pb->b-1; y++){
						COLORREF *tmp = BITMAPBITS(*pbm, pb->l, y);
						if(BITMAPHIT(*pbm, pb->l, y))
							*tmp = cb;
						tmp = BITMAPBITS(*pbm, pb->r, y);
						if(BITMAPHIT(*pbm, pb->b, y))
							*tmp = cb;
					}
					break;
				case TEF_ADDBLEND:
					for(x = pb->l; x < pb->r; x++){
						COLORREF *tmp = BITMAPBITS(*pbm, x, pb->t);
						if(BITMAPHIT(*pbm, x, pb->t))
							*tmp = ADDBLEND(*tmp, cb);
						tmp = BITMAPBITS(*pbm, x, pb->b);
						if(BITMAPHIT(*pbm, x, pb->b))
							*tmp = ADDBLEND(*tmp, cb);
					}
					for(y = pb->t+1; y < pb->b-1; y++){
						COLORREF *tmp = BITMAPHITBITS(*pbm, pb->l, y);
						if(tmp)
							*tmp = ADDBLEND(*tmp, cb);
						tmp = BITMAPHITBITS(*pbm, pb->r, y);
						if(tmp)
							*tmp = ADDBLEND(*tmp, cb);
					}
					break;
				case TEF_SUBBLEND:
					for(x = pb->l; x < pb->r; x++){
						COLORREF *tmp = BITMAPHITBITS(*pbm, x, pb->t);
						if(tmp) *tmp = SUBBLEND(*tmp, cb);
						tmp = BITMAPHITBITS(*pbm, x, pb->b);
						if(tmp) *tmp = SUBBLEND(*tmp, cb);
					}
					for(y = pb->t+1; y < pb->b-1; y++){
						COLORREF *tmp = BITMAPHITBITS(*pbm, pb->l, y);
						if(tmp) *tmp = SUBBLEND(*tmp, cb);
						tmp = BITMAPHITBITS(*pbm, pb->r, y);
						if(tmp) *tmp = SUBBLEND(*tmp, cb);
					}
					break;
				case TEF_ALPHABLEND:
					for(x = pb->l; x < pb->r; x++){
						COLORREF *tmp = BITMAPHITBITS(*pbm, x, pb->t);
						if(tmp) *tmp = ALPHABLEND(*tmp, cb, 255-(cb>>24));
						tmp = BITMAPHITBITS(*pbm, x, pb->b);
						if(tmp) *tmp = ALPHABLEND(*tmp, cb, 255-(cb>>24));
					}
					for(y = pb->t+1; y < pb->b-1; y++){
						COLORREF *tmp = BITMAPHITBITS(*pbm, pb->l, y);
						if(tmp) *tmp = ALPHABLEND(*tmp, cb, 255-(cb>>24));
						tmp = BITMAPHITBITS(*pbm, pb->r, y);
						if(tmp) *tmp = ALPHABLEND(*tmp, cb, 255-(cb>>24));
					}
					break;
				}
#else
				RectangleWin(pwg, pb->l, pb->t, pb->r, pb->b, ColorSequence(pb->u.c.cb, pb->max - pb->life));
#endif
			}
		}
		else{
#if DIRECTDRAW
				int x, y;
			double mx = 255 * pb->max;
			double br = MIN(pb->life, mx);
			COLORREF cf;
			cf = pb->u.n.col == WG_BLACK
			 ? RGB(((mx - br) * GetRValue(pb->u.n.cd) + br * (double)rand() / RAND_MAX) / pb->max,
				((mx - br) * GetGValue(pb->u.n.cd) + br * (double)rand() / RAND_MAX) / pb->max,
				((mx - br) * GetBValue(pb->u.n.cd) + br * (double)rand() / RAND_MAX) / pb->max)
			 : RGB(((mx - br) * GetRValue(pb->u.n.cd) + br * GetRValue(pb->u.n.col)) / pb->max,
				((mx - br) * GetGValue(pb->u.n.cd) + br * GetGValue(pb->u.n.col)) / pb->max,
				((mx - br) * GetBValue(pb->u.n.cd) + br * GetBValue(pb->u.n.col)) / pb->max);
			cf = RGB(GetBValue(cf),GetGValue(cf),GetRValue(cf)); /* swap bytes */
			if((pb->flags & TEF_BLENDMASK) == TEF_ADDBLEND){
				AddConstBox(pbm->bmBits, cf, pb->l, pb->t, pb->r - pb->l, pb->b - pb->t, pbm->bmWidthBytes);
			}
			else
			for(x = pb->l; x < pb->r; x++) for(y = pb->t; y < pb->b; y++) if(BITMAPHIT(*pbm, x, y)){
				COLORREF *const tmp = BITMAPBITS(*pbm, x, y);
				switch(pb->flags & TEF_BLENDMASK){
				case TEF_SOLIDBLEND:
					*tmp = cf;
					break;
			/*	case TEF_ADDBLEND:
					*tmp = ADDBLEND(*tmp,cf);
					break;*/
				case TEF_SUBBLEND:
					*tmp = SUBBLEND(*tmp,cf);
					break;
				case TEF_ALPHABLEND:
					*tmp = ALPHABLEND(*tmp,cf, 256 * pb->life / pb->max);
					break;
				}
			}
#else
			double br;
			br = pb->life / pb->max;
			br = MIN(br, 1.0);
			FillRectangleWin(pwg, pb->l, pb->t, pb->r, pb->b,
			 pb->u.n.col == WG_BLACK
			 ? RGB((unsigned char)((1 - br) * GetRValue(pb->u.n.cd) + br * (double)(rand() % 128)),
				(unsigned char)((1 - br) * GetGValue(pb->u.n.cd) + br * (double)(rand() % 128)),
				(unsigned char)((1 - br) * GetBValue(pb->u.n.cd) + br * (double)(rand() % 128)))
			 : RGB((unsigned char)((1 - br) * GetRValue(pb->u.n.cd) + br * GetRValue(pb->u.n.col)),
				(unsigned char)((1 - br) * GetGValue(pb->u.n.cd) + br * GetGValue(pb->u.n.col)),
				(unsigned char)((1 - br) * GetBValue(pb->u.n.cd) + br * GetBValue(pb->u.n.col))));
#endif
		}
		pb = pb->next;
	}
	pb = p->lborder;
	while(pb){
		if(pb->flags & TEF_CUSTOM){
			if(pb->u.c.cf){
				double d = pb->max - pb->life;
/*				FillRectangleWin(pwg, pb->l, pb->t, pb->r, pb->b, ColorSequence(pb->u.c.cf, d));*/
				SetBkColor(to->hdc, ColorSequence(pb->u.c.cf, d));
				ExtTextOut(to->hdc, pb->l, pb->t, ETO_OPAQUE, (RECT*)pb, NULL, 0, NULL);
			}
			if(pb->u.c.cb){
/*				RectangleWin(pwg, pb->l, pb->t, pb->r, pb->b, ColorSequence(pb->u.c.cb, pb->max - pb->life));*/
				outlineRect(to, (RECT*)pb, ColorSequence(pb->u.c.cb, pb->max - pb->life));
			}
		}
		else{
			unsigned char buf[3];
			double br;
			br = pb->life / pb->max;
			br = MIN(br, 1.0);
			if(pb->u.n.col == WG_BLACK){
				buf[0] = (unsigned char)((1 - br) * GetRValue(pb->u.n.cd) + br * (double)(rand() % 128));
				buf[1] = (unsigned char)((1 - br) * GetGValue(pb->u.n.cd) + br * (double)(rand() % 128));
				buf[2] = (unsigned char)((1 - br) * GetBValue(pb->u.n.cd) + br * (double)(rand() % 128));
			}
			else{
				buf[0] = (unsigned char)((1 - br) * GetRValue(pb->u.n.cd) + br * GetRValue(pb->u.n.col));
				buf[1] = (unsigned char)((1 - br) * GetGValue(pb->u.n.cd) + br * GetGValue(pb->u.n.col));
				buf[2] = (unsigned char)((1 - br) * GetBValue(pb->u.n.cd) + br * GetBValue(pb->u.n.col));
			}

/*			RectangleWin(pwg, pb->l, pb->t, pb->r, pb->b, RGB(buf[0], buf[1], buf[2]));*/
			outlineRect(to, (RECT*)pb, RGB(buf[0], buf[1], buf[2]));
		}
		pb = pb->next;
	}
retur:;
	}
#ifndef NDEBUG
	g_tetimes.drawtefbox = TimeMeasLap(&tm);
#endif
}
