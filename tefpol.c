#include "tent_p.h"
#include <math.h>
#include <assert.h>
#include <stddef.h>
#include "dprint.h"
#include "debug.h"

/*#############################################################################
	macro definitions
#############################################################################*/
#define WINTEFPOL 0 /* whether use WinAPI to draw polylines */
#define DIRECTDRAW 1 /* whether draw into the buffer directly, experiments show this is the best */
#define ENABLE_ADDBLEND 1 /* perform per-pixel additive color merger method */
#define DRAWORDER 0 /* whether draw from the head to the tail, otherwise reverse */
#define ENABLE_FORMS 0 /* head forms */
#define ENABLE_FINE 0 /* granularity control for line graduation */
#define ENABLE_ROUGH 0 /* less fine */
#define ENABLE_RETURN 0 /* return from opposite edge, doesn't make sense for polylines */
#define ENABLE_THICK 1 /* thickness control in conjunction with WinAPI drawing */
#define ENABLE_HEAD 0 /* head sprite */
#define INTERP_CS 1 /* Liner interpolation rather than calling ColorSequence() everytime */

#if !WINTEFPOL && !DIRECTDRAW
#undef ENABLE_THICK
#define ENABLE_THICK 0
#endif

#define TEP_THICKNESS (TEP_THICK|(TEP_THICK<<1))
#define TEP_BLENDMASK (TEP_SOLIDBLEND|(TEP_SOLIDBLEND<<1))

/*=============================================================================
	type definitions
=============================================================================*/
/* a set of auto-decaying line segments */
typedef struct te_followpolyline{
	double x, y;
	double vx, vy;
	double life;
	float grv;
	const colseq_t *cs;
	DWORD flags; /* entity options */
	unsigned cnt; /* generated trail node count */
	struct te_vertex *head, *tail; /* polyline's */
	struct te_followpolyline *next;
} tefpol_t;

/* a tefpol can have arbitrary number of vertices in the vertices buffer. */
typedef struct te_vertex{
	float dt;
	int x, y;
	struct te_vertex *next;
} tevert_t;

struct te_followpolyline_list{
	unsigned m;
	unsigned ml;
	unsigned mex;
	tefpol_t *l;
	tefpol_t *lfree, *lactv, *last;
	struct te_fpol_extra_list{
		struct te_fpol_extra_list *next;
		tefpol_t l[1]; /* variable */
	} *ex;
	unsigned bs; /* verbose buffer size */
};

/* global vertice pool, used by multiple entities */
static struct te_vertice_list{
	unsigned m;
	tevert_t *l;
	tevert_t *lfree, *lfst;
} svl = {0, NULL};

/******************************************************************************
	file local variables
******************************************************************************/
static const char *s_electric[] = {
	"“d", "—‹", "Œ‚", "“{", "•®", "”±", "Š…",
};

/*-----------------------------------------------------------------------------
	function definitions
-----------------------------------------------------------------------------*/

#if 1
#define checkVertices(a)
#else
static void checkVertices(tepl_t *p){
	tefpol_t *pl = p->lactv;
	static int invokes = 0;
	char *ok;
	ok = calloc(sizeof*ok, svl.m);
	while(pl){ /* check all tefpols */
		tevert_t *pv = pl->head;
		while(pv){
			ok[pv - svl.l] = 1;
			if(!pv->next) assert(pl->tail == pv);
			pv = pv->next;
		}
		pl = pl->next;
	}
	{/* check free list */
		tevert_t *pv = svl.lfree;
		while(pv){
			ok[pv - svl.l] = 1;
			pv = pv->next;
		}
	}
	{
		int i;
		for(i = 0; i < svl.m; i++)
			assert(ok[i]);
	}
	free(ok);
	invokes++;
}
#endif

/* free vertex nodes between *pp and plast */
static void freeVertex(tevert_t **pp, tevert_t *plast){
/*	if((*pp)->next) freeVertex(&(*pp)->next);
	(*pp)->next = svl.lfree;
	svl.lfree = *pp;
	if(!svl.lfree->next) svl.lfst = *pp;
	*pp = NULL;
*/
#if 1 /* put back to head */
	if(*pp){
		tevert_t *pfirst = *pp;
		assert(!svl.lfree == !svl.lfst);
		assert(!svl.lfst || !svl.lfst->next);
		assert(!plast || !plast->next);
		if(plast)
			*pp = plast->next, plast->next = svl.lfree;
		else{
			tevert_t *p = pfirst;
			while(p->next) p = p->next;
			(plast = p)->next = svl.lfree;
		}
		if(!svl.lfree) svl.lfst = plast;
		svl.lfree = pfirst;
	}
	assert(!svl.lfree == !svl.lfst);
	assert(!svl.lfst || !svl.lfst->next);
#else /* push back from back */
	if(*pp){
		assert(!svl.lfree == !svl.lfst);
		assert(!svl.lfst || !svl.lfst->next);
		assert(!plast || !plast->next);
		if(svl.lfree){
			svl.lfst->next = *pp;
		}
		else{
			svl.lfree = *pp;
		}
		if(plast) svl.lfst = plast;
		else{
			tevert_t *p = *pp;
			while(p->next) p = p->next;
			svl.lfst = p;
		}
		*pp = NULL;
	}
#endif
}

static void freeTefpolExtra(struct te_fpol_extra_list *p){
	if(p->next) freeTefpolExtra(p->next);
	free(p);
}

tepl_t *NewTefpol(unsigned maxt, unsigned init, unsigned unit){
	int i;
	tepl_t *ret;
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
	g_tetimes.so_tefpol_t = sizeof(tefpol_t);
	g_tetimes.so_tevert_t = sizeof(tevert_t);
	g_tetimes.tefpol_m = ret->bs;
	dprintf(
		"sizeof(tefpol_t) = %u\n"
		"fpol Count: %u/%u\n"
		"Memory Usage for Tefpol: %u/%u\n"
		"Total Memory Usage: %u\n",
		sizeof(tefpol_t),
		init, maxt,
		init * sizeof *ret->l, maxt * sizeof *ret->l,
		TOTALMEMORY);
#endif
	return ret;
}

/* destructor and deallocator */
void DelTefpol(tepl_t *p){
	if(p->l){
		tefpol_t *pl = p->lactv;
		while(pl){
			freeVertex(&pl->head, pl->tail);
			pl = pl->next;
		}
		free(p->l);
	}
	if(p->ex) freeTefpolExtra(p->ex);
	free(p);
}

void AddTefpol(tepl_t *p, double x, double y, double vx, double vy,
			   float grv, const colseq_t *col, tefpol_flags_t f, double life)
{
	tefpol_t *pl;
	if(!p->m) return;
	pl = p->lfree;
	if(!pl){
		if(p->mex && p->bs + p->mex <= p->m){
			struct te_fpol_extra_list *ex;
			unsigned i;
			dprintf("tefpol.bs = %u + (%u)%u = %u; %u\n", p->bs * sizeof *p->l,
				offsetof(struct te_fpol_extra_list, l), offsetof(struct te_fpol_extra_list, l) + p->mex * sizeof *ex->l,
				(p->bs + p->mex) * sizeof *p->l, TOTALMEMORY);
			ex = malloc(offsetof(struct te_fpol_extra_list, l) + p->mex * sizeof *ex->l); /* struct hack alloc */
			ex->next = p->ex;
			p->ex = ex;
			p->lfree = ex->l;
			for(i = 0; i < p->mex-1; i++)
				ex->l[i].next = &ex->l[i+1];
			ex->l[p->mex-1].next = NULL;
			pl = p->lfree;
			p->bs += p->mex;
#ifndef NDEBUG
			g_tetimes.tefpol_m = p->bs;
#endif
		}
		else{
			pl = p->lactv; /* reuse the oldest active node */
			if(!pl) return;
			assert(!pl->tail || !pl->tail->next);

			/* free vertices */
			freeVertex(&pl->head, pl->tail);
/*			if(pl->head){
				if(svl.lfree){
					svl.lfst->next = pl->head;
					svl.lfst = pl->tail;
				}
				else{
					svl.lfree = pl->head;
					svl.lfst = pl->tail;
				}
			}*/

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
//	if(flags & TEL_NOLINE) flags &= ~TEL_HEADFORWARD;
//	if((flags & TEL_FORMS) == TEL_POINT) flags &= ~TEL_VELOLEN;

	pl->x = x;
	pl->y = y;
	pl->vx = vx;
	pl->vy = vy;
	pl->grv = grv;
	pl->cs = col;
	pl->flags = f;
	pl->cnt = 0;
	pl->life = life;
	pl->head = pl->tail = NULL;

#ifndef NDEBUG
	g_tetimes.tefpol_s++;
#endif
}

void AnimTefpol(tepl_t *p, double dt){
	STARTTIMER(animtefpol, &g_tetimes.animtefpol)
	tefpol_t *pl = p->lactv, **ppl = &p->lactv;
	while(pl){
		tefpol_t *pl2;

		/* delete condition */
		/* if no border behavior is specified, it should be deleted */
		if(pl->life < -pl->cs->t 
			|| (!(pl->flags & (TEP_RETURN | TEP_REFLECT)) && !pl->head && (pl->x < 0 || XSIZ < pl->x || pl->y < 0 || YSIZ < pl->y))){
			/* check if the bounding box hits the draw area. */

			/* free occupied vertices */
			freeVertex(&pl->head, pl->tail);
			pl->tail = NULL;

			/* roll pointers */
			if(pl == p->last && *ppl != p->lactv)
				p->last = (tefpol_t*)((byte*)ppl - offsetof(tefpol_t, next));
			pl2 = pl->next;
			*ppl = pl->next;
			pl->next = p->lfree;
			p->lfree = pl;
			checkVertices(p);
			pl = pl2;
			continue;
		}
		else{
			ppl = &pl->next;
			pl2 = pl->next;
		}
		if(!svl.l){ /* allocate the vertex buffer */
			int i;
			svl.l = svl.lfree = malloc((svl.m = g_pinit->PolygonVertexBufferSize) * sizeof *svl.l);
			for(i = 0; i < svl.m-1; i++)
				svl.l[i].next = &svl.l[i+1];
			if(svl.m){
				svl.l[i].next = NULL;
				svl.lfst = &svl.l[i];
			}
			dprintf(
				"sizeof(tevert_t) = %u\n"
				"vertex Count: %u\n"
				"Memory Usage for vertices: %u\n",
				sizeof(tevert_t),
				g_pinit->PolygonVertexBufferSize,
				g_pinit->PolygonVertexBufferSize * sizeof *svl.l);
		}
		{ /* free outdated vertices */
#if DRAWORDER
		tevert_t **ppv = &pl->head;
		double t = 0;
		while(*ppv){
			if(pl->cs->t < (t += (*ppv)->dt)){
				freeVertex(ppv, pl->tail);
				if(ppv != &pl->head)
					pl->tail = (tevert_t*)((byte*)ppv - offsetof(struct te_vertex, next));
				else
					pl->tail = NULL;
				break;
			}
			ppv = &(*ppv)->next;
		}
#else
		tevert_t *pv = pl->head;
		double t = 0;
		for(; pv; pv = pv->next) t += pv->dt;
		while(pl->cs->t < t){ /* delete from head */
			pv = pl->head;
			pl->head = pl->head->next;
			pv->next = svl.lfree;
			svl.lfree = pv;
			if(!pl->head) pl->tail = NULL;
			if(!svl.lfree->next) svl.lfst = svl.lfree;
			t -= pv->dt;
		}
#endif
		checkVertices(p);
		}
		/* AddVertex: add a vertex for the polyline */
		if(pl->life > 0
			&& (pl->flags & (TEP_RETURN | TEP_REFLECT) || 0 < pl->x && pl->x < XSIZ && 0 < pl->y && pl->y < YSIZ) /* if neither return or reflect on the border, shouldn't add vertices */
			&& (!pl->head || (int)pl->x != pl->head->x || (int)pl->y != pl->head->y)){
		pl->cnt++;
		if(!(pl->flags & TEP_ROUGH) || pl->cnt % 2){
		tevert_t *pv = svl.lfree;
		if(!pv){
			pv = pl->head; /* RerollVertex: reuse the oldest active node */
			if(!pv) goto novertice;

			/* roll pointers */
#if /*DRAWORDER*/0
			pl->tail->next = pl->head;
			pl->head = pl->tail;
			for(pv = pl->head; pv->next != pl->tail; pv = pv->next);
			pl->tail = pv;
			pv->next = NULL;
#else
			pl->tail->next = pl->head;
			pl->tail = pl->head;
			pl->head = pl->head->next;
			pv->next = NULL;
#endif
			checkVertices(p);
		}
		else{ /* AllocVertex: allocate from the pool */
#if DRAWORDER
			svl.lfree = pv->next;
			if(!svl.lfree) svl.lfst = NULL;
			pv->next = pl->head;
			pl->head = pv;
			if(!pv->next) pl->tail = pv;
#else
			svl.lfree = pv->next;
			if(!svl.lfree) svl.lfst = NULL;
			pv->next = NULL;
			if(pl->tail) pl->tail->next = pv;
			else pl->head = pv;
			pl->tail = pv;
#endif
			checkVertices(p);
		}
		if(pl->flags & TEP_DRUNK){
			pv->x = (int)pl->x + rand() % 8 - 4;
			pv->y = (int)pl->y + rand() % 8 - 4;
		}
		else if(pl->flags & TEP_WIND){
			pv->x = (int)pl->x + rand() % 4 - 2;
			pv->y = (int)pl->y + rand() % 4 - 2;
		}
		else{
			pv->x = (int)pl->x;
			pv->y = (int)pl->y;
		}
		pv->dt = dt;
	//	pl->cnt++;
#ifndef NDEBUG
		g_tetimes.tevert_s++;
#endif
		}}
novertice:
		pl->life -= dt;
		/* if out, don't think of it anymore, with exceptions on return and reflect,
		  which is expceted to be always inside the region. */
		if(pl->life > 0 && (pl->flags & (TEP_RETURN | TEP_REFLECT) || !(pl->x < 0 || XSIZ < pl->x || pl->y < 0 || YSIZ < pl->y))){
			pl->vy += GRAVITY * pl->grv * dt; /* apply gravity */
			if(pl->flags & TEP_DRUNK){
				pl->vx += dt * ((drand() - .5) * 9000 - (pl->vx / 3));
				pl->vy += dt * ((drand() - .5) * 9000 - (pl->vy / 3));
			}
			pl->x += pl->vx * dt;
			pl->y += pl->vy * dt;
		}
#if DRAWORDER
		else if(pl->head)
			pl->head->dt += dt;
#else
		else if(pl->tail)
			pl->tail->dt += dt;
#endif

#if ENABLE_RETURN
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
		else
#endif
		/* rough approximation of reflection. rigid body dynamics are required for more reality. */
		if(pl->flags & TEP_REFLECT){
			int j = 0; /* reflected at least once */
			if(pl->x + pl->vx * dt < 0 || XSIZ < pl->x + pl->vx * dt)
				pl->vx *= - REFLACTION, j = 1;
			if(pl->y + pl->vy * dt < 0 || YSIZ < pl->y + pl->vy * dt)
				pl->vy *= - REFLACTION, j = 1;
		}

		pl = pl2;
	}
	checkVertices(p);
	STOPTIMER(animtefpol);
#ifndef NDEBUG
	if(p->ml){
	unsigned int i = 0;
	tefpol_t *pl = p->lactv;
	while(pl) i++, pl = pl->next;
	g_tetimes.tefpol_c = i;
	}
	if(svl.m){
	unsigned int i = svl.m;
	tevert_t *pv = svl.lfree;
	while(pv) i--, pv = pv->next;
	g_tetimes.tevert_c = i;
	}
#endif
}

void DrawTefpol(teout_t *to, tepl_t *p){
	STARTTIMER(drawtefpol, &g_tetimes.drawtefpol)
	tefpol_t *pl = p->lactv;
#if DIRECTDRAW
	BITMAP *pbm;
	if(!pl) goto retur;
	pbm = &to->bm;
	if(pbm->bmBitsPixel != 32) goto retur;
#endif
#if DRAWORDER
	for(; pl; pl = pl->next){
		int ox = pl->x, oy = pl->y, c = pl->cnt;
#else
	for(; pl; pl = pl->next) if(pl->head){
		int ox = pl->head->x, oy = pl->head->y, c = pl->cnt;
#endif
#if INTERP_CS
		COLORREF ocol;
#endif
		tevert_t *pv = pl->head;
		double t = 0;
#if ENABLE_THICK
		int thick = ((pl->flags & TEP_THICKNESS) >> 7) & 0x3;
		if(thick) thick++;
#else
		const int thick = 0;
#endif
#if !DRAWORDER
		for(pv = pl->head; pv; pv = pv->next) t += pv->dt;
#endif
#if INTERP_CS
		ocol = ColorSequence(pl->cs, t);
/*		ocol = SWAPRB(ocol);*/
#endif
/*		int i;
		for(i = 0; i < 2; i++){
			PutPointWin(pwg, ox + rand() % 10 - 5, oy + rand() % 10 - 5, ColorSequence(pl->cs, 0));
		}*/
#if WINTEFPOL
		MoveToEx(pwg->hVDC, ox, oy, NULL);
#endif
		for(pv = pl->head; pv; c--){
#if INTERP_CS
			COLORREF ncol;
#endif
#if ENABLE_FINE
			int x, y, drawn;
			for(drawn = 0; !drawn || (pl->flags & TEP_FINE && drawn == 1); drawn++){
			if(pl->flags & TEP_FINE){
				x = drawn ? pv->x : (ox + pv->x) / 2;
				y = drawn ? pv->y : (oy + pv->y) / 2;
			}
			else
				x = pv->x, y = pv->y;
			if(pl->flags & TEP_SHAKE){
				x += rand() % 8 - 4;
				y += rand() % 8 - 4;
			}
#else
			const int x = pl->flags & TEP_SHAKE ? pv->x + rand() % 6 - 3 : pv->x,
				y = pl->flags & TEP_SHAKE ? pv->y + rand() % 6 - 3 : pv->y;
#endif
#if INTERP_CS
			{
#	if ENABLE_FINE
			float dt = pl->flags & TEP_FINE ? pv->dt / 2 : pv->dt;
#	else
			float dt = pv->dt;
#	endif
#	if DRAWORDER
			ncol = ColorSequence(pl->cs, t + dt);
#	else
			ncol = ColorSequence(pl->cs, t);
#	endif
/*			ncol = SWAPRB(ncol);*/
			}
#endif
#if ENABLE_FORMS
			switch(pl->flags & TEL_FORMS){
			case 0:
#endif
#if WINTEFPOL
				{
				HPEN hPen;
				hPen = CreatePen(PS_SOLID, thick, ColorSequence(pl->cs, t));
				if(!hPen) break;
				SelectObject(pwg->hVDC, hPen);
				LineTo(pwg->hVDC, x, y);
				DeleteObject(hPen);
				}
#elif DIRECTDRAW
				{
				if(pbm->bmBits && pbm->bmBitsPixel == 32 && 0 <= x && x < pbm->bmWidth && 0 <= y && y < pbm->bmHeight){
					int i, xp, yp, delta = MAX(ABS(ox - x), ABS(oy - y));
					for(i = delta; i; i--){
#if ENABLE_THICK
						int j;
#endif
						xp = (ox * i + x * (delta - i)) / delta;
						yp = (oy * i + y * (delta - i)) / delta;
#if ENABLE_THICK
						for(j = 0; pl->flags & TEP_THICK ? j < 2 : j < 1; j++, ABS(ox-x)<ABS(oy-y) ? xp++ : yp++)
#endif
						if(0 <= xp && xp < pbm->bmWidth && 0 <= yp && yp < pbm->bmHeight){
							COLORREF cc, *const tc = (COLORREF*)&((BYTE*)pbm->bmBits)[xp * 4 + yp * pbm->bmWidthBytes];
#if ENABLE_FINE
							float dt = pl->flags & TEP_FINE ? pv->dt / 2 : pv->dt;
#else
							float dt = pv->dt;
#endif
#if INTERP_CS
#	if DRAWORDER
							cc = RATEBLEND(ocol, ncol, i, delta);
#	else
							cc = RATEBLEND(ncol, ocol, i, delta);
#	endif
#else
#	if DRAWORDER
							cc = ColorSequence(pl->cs, (t * i + (t + dt) * (delta - i)) / delta);
#	else
							cc = ColorSequence(pl->cs, ((t + dt) * i + t * (delta - i)) / delta);
#	endif
#endif
#if ENABLE_ADDBLEND
#define GetAValue(col) ((col)>>24)
							switch(pl->flags & TEP_BLENDMASK){
							case TEP_ADDBLEND:
								*tc = RGB(
									MIN(255,GetRValue(*tc)+GetBValue(cc)),
									MIN(255,GetGValue(*tc)+GetGValue(cc)),
									MIN(255,GetBValue(*tc)+GetRValue(cc)));
								break;
							case TEP_ALPHABLEND:{
								int a = 255-GetAValue(cc);
								*tc = RGB(
									((255-a)*GetRValue(*tc)+a*GetBValue(cc))/256,
									((255-a)*GetGValue(*tc)+a*GetGValue(cc))/256,
									((255-a)*GetBValue(*tc)+a*GetRValue(cc))/256);
								break;
							}
							case TEP_SUBBLEND:
								*tc = RGB(
									MAX(0,GetRValue(*tc)-GetBValue(cc)),
									MAX(0,GetGValue(*tc)-GetGValue(cc)),
									MAX(0,GetBValue(*tc)-GetRValue(cc)));
								break;
							case TEP_SOLIDBLEND:
								*tc = RGB(GetBValue(cc),GetGValue(cc),GetRValue(cc));
								break;
							}
#else
							*tc = RGB(GetBValue(cc),GetGValue(cc),GetRValue(cc));
#endif
						}
					}
				}
				}
#else
				LineWin(pwg, ox, oy, x, y, ColorSequence(pl->cs, t));
#endif
#if ENABLE_FORMS
				break;
			case TEL_POINT:
				PutPointWin(pwg, pv->x, pv->y, ColorSequence(pl->cs, t));
				break;
			case TEL_CIRCLE:
				CircleWin(pwg, pv->x, pv->y, (ABS(ox - pv->x) + ABS(oy - pv->y)) / 2, ColorSequence(pl->cs, t));
				break;
			case TEL_FILLCIRCLE:
				FillCircleWin(pwg, pv->x, pv->y, (ABS(ox - pv->x) + ABS(oy - pv->y)) / 2, ColorSequence(pl->cs, t));
				break;
			case TEL_RECTANGLE:{
				int rad = (ABS(ox - pv->x) + ABS(oy - pv->y)) / 2;
				RectangleWin(pwg, pv->x - rad, pv->y - rad, pv->x + rad, pv->y + rad, ColorSequence(pl->cs, t));
				break;}
			case TEL_FILLRECT:{
				int rad = (ABS(ox - pv->x) + ABS(oy - pv->y)) / 2;
				FillRectangleWin(pwg, pv->x - rad, pv->y - rad, pv->x + rad, pv->y + rad, ColorSequence(pl->cs, t));
				break;}
			case TEL_PENTAGRAPH:{
				double rad = (ABS(ox - pv->x) + ABS(oy - pv->y)) / 2, ang = 9/*drand() * 2 * M_PI*/;
				int i;
				HPEN hPen;
				hPen = CreatePen(PS_SOLID, 0, ColorSequence(pl->cs, t));
				if(!hPen) break;
				SelectObject(pwg->hVDC, hPen);
				MoveToEx(pwg->hVDC, pv->x + cos(ang) * rad, pv->y + sin(ang) * rad, NULL);
				for(i = 0 ; i < 5; i++){
					ang += 2 * M_PI * 2 / 5.;
					LineTo(pwg->hVDC, pv->x + cos(ang) * rad, pv->y + sin(ang) * rad);
				}
				DeleteObject(hPen);
				break;}
			case TEL_HEXAGRAPH:{
				HFONT hFont;
				char *s;
				hFont = CreateFont(MIN((ABS(ox - pv->x) + ABS(oy - pv->y)), 20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, NULL);
				if(!hFont) break;
				SelectObject(pwg->hVDC, hFont);
				SetTextColor(pwg->hVDC, ColorSequence(pl->cs, t));
				switch(c % 7){
					case 6:
					case 0: s = "”š"; break;
					case 1: s = "—ó"; break;
					case 2: s = "â"; break;
					case 3: s = "–Å"; break;
					case 4: s = "‰ó"; break;
					case 5: s = "Š…"; break;
				}
				TextOut(pwg->hVDC, pv->x, pv->y, s, 2);
				DeleteObject(hFont);
				break;}
			default: assert(0);
			}
#endif
#if ENABLE_FINE
			if(pl->flags & TEP_FINE)
#if DRAWORDER
			t += pv->dt / 2;
#else
			t -= pv->dt / 2;
#endif
			else
#endif
#if DRAWORDER
			t += pv->dt;
#else
			t -= pv->dt;
#endif
#if ENABLE_FINE
			if(pl->flags & TEP_FINE && !drawn)
				ox = (ox + pv->x) / 2, oy = (oy + pv->y) / 2;
			}
#endif
#if INTERP_CS
			ocol = ncol;
#endif
			ox = x;
			oy = y;
			pv = pv->next;
		}
#if ENABLE_HEAD
		if(pl->flags & TEP_HEAD){
#if LANG==JP
			HFONT hFont;
			hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, NULL);
			if(!hFont) break;
			SelectObject(pwg->hVDC, hFont);
			SetTextColor(pwg->hVDC, ColorSequence(pl->cs, 0));
			TextOut(pwg->hVDC, pl->x - 10, pl->y - 10, s_electric[(int)pl%numof(s_electric)], 2);
			DeleteObject(hFont);
#else
		FillCircleWin(pwg, pl->x, pl->y, 5, ColorSequence(pl->cs, 0));
#endif
		}
#endif
	}
retur:;
	STOPTIMER(drawtefpol);
}

#ifndef NDEBUG
void VertexUsage(struct VertexUsageRet *ret){
	int i;
	unsigned c = 0;
	ret->maxcount = svl.m;
	ret->bytes = sizeof svl + svl.m * sizeof*svl.l;
	{
		tevert_t *pv;
		for(pv = svl.lfree; pv; pv = pv->next){
			c++;
		}
		ret->count = svl.m - c;
	}
}
#endif
