#include "tent_p.h"
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

/* gryph caching */
#define GBUCKET 31
struct gryph{
	struct gryph *next;
	char s[3];
	int size;
	long b;
	BYTE v[1];
};

// hash table
static struct gryph *gryphpool[GBUCKET] = {NULL};

static int hashgryph(const struct gryph *g){
	int i, ret = g->size;
	for(i = 0; i < sizeof g->s; i++)
		ret += (unsigned char)g->s[i];
	return ret;
}

struct gryph *newgryph(const char s[3], int size){
	static int init = 0;
	int h;
	long b;
	struct gryph *pg, *g1;
	if(!size) return NULL;
	if(!init){
		init = 1;
		atexit(cleargryph);
	}

	/* obtain hash code. TODO: better spreading algorithm? */
	{
		int i;
		h = size;
		for(i = 0; i < sizeof pg->s; i++)
			h += (unsigned char)s[i];
		h %= GBUCKET;
	}

	for(g1 = gryphpool[h]; g1; g1 = g1->next)
		if(!memcmp(s, g1->s, sizeof g1->s) && size == g1->size)
			return g1;
	b = offsetof(struct gryph, v) + (size * size + 7) / 8;
	pg = malloc(b);
	pg->next = gryphpool[h];
	*(long*)pg->s = *(long*)s;
	pg->size = size;
	pg->b = b;
	{
		BYTE *pv;
		long wb;
		HBITMAP hb;
		HDC hdc;
		HFONT hFont;
		int inv;

		/* create a monochrome bitmap to write gryph bitputterns into. */
		{
			struct bitmapmono {
				BITMAPINFOHEADER bmiHeader;
				RGBQUAD bmiColors[2];
			} bi = {
				{
					sizeof(struct bitmapmono),
					size,
					size,
					1,
					1,
					BI_RGB, 0, 100, 100, 0, 0},
					{{255,255,255,0}, {0,0,0,0}}};
			hb = CreateDIBSection(NULL, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pv, NULL, 0);
		}
		if(!hb) return NULL;
#if 0 /* there's silly bug in GetObject that won't return correct WidthBytes. */
		{
			BITMAP bm;
			GetObject(hb, sizeof bm, &bm);
			wb = bm.bmWidthBytes;
		}
#else /* unreliable way to get word alignment */
		wb = (size + (8 * sizeof(int) - 1)) / (8 * sizeof(int)) * sizeof(int);
#endif
		/* there's un-understandable issue that output of textout sometimes inverts */
		inv = pv[0] & 1;

		/* write it with TextOut */
		hdc = CreateCompatibleDC(GetDC(NULL));
		if(!hdc) return NULL;
		SelectObject(hdc, hb);
		hFont = CreateFont((int)(size + .5), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, NULL);
		if(!hFont) return NULL;
		SelectObject(hdc, hFont);
		SetTextColor(hdc, RGB(0,0,0));
		TextOut(hdc, 0, 0, s, 2);

		/* copy the contents into malloc'ed buffer in packed form. */
		{
			int x, y;
			for(y = 0; y < pg->size; y++)
				for(x = 0; x < pg->size; x++){
					int dst = x + pg->size * y;
					assert((&pg->v[dst / 8] - (BYTE*)pg) < pg->b);
					if(pv[x / 8 + wb * y] & (1 << (7 - x % 8)) ? !inv : inv)
						pg->v[dst / 8] |= 1 << (dst % 8);
					else
						pg->v[dst / 8] &= ~(1 << (dst % 8));
				}
		}

		/* free resources */
		DeleteObject(hFont);
		DeleteObject(hdc);
		DeleteObject(hb);
	}
	return gryphpool[h] = pg;
}

void drawgryph(teout_t *to, const struct gryph *g, int x0, int y0, COLORREF c){
	int i, j, x, y;
	if(g)
	for(j = 0, y = MIN(y0, to->bm.bmHeight); 0 <= y && j < g->size; y--, j++)
//	for(j = 0, y = MAX(y0, 0); y < pwg->nHeight && j < g->size; y++, j++)
//		for(i = 0, x = MIN(x0, pwg->nWidth); 0 < x && i < g->size; x--, i++)
		for(i = 0, x = MAX(x0, 0); x < to->bm.bmWidth && i < g->size; x++, i++){
			int src = i + g->size * j;
//			if( (((BYTE*)g->v)[(g->size - i-1) / 8 + j * g->wb] & (1 << ((g->size - i-1) % 8))) )
//			if( (((BYTE*)g->v)[i / 8 + j * g->wb] & (1 << (7 - i % 8))) )
			if( (((BYTE*)g->v)[src / 8] & (1 << (src % 8))) )
				*BITMAPBITS(to->bm, x, y) = c;
//			pwg->pVWBuf[x + pwg->nWidth * y] = ((COLORREF*)g->v)[(g->size - i-1) + j * g->size];
		}
}

void cleargryph(void){
	int i;
	for(i = 0; i < GBUCKET; i++){
		struct gryph *pg = gryphpool[i];
		while(pg){
			struct gryph *pgkill = pg;
			pg = pg->next;
			free(pgkill);
		}
	}
}

#if LANG==JP && !defined NDEBUG

void GryphCacheUsage(struct GryphCacheUsageRet *ret){
	int i;
	ret->count = 0;
	ret->bytes = sizeof gryphpool;
	for(i = 0; i < GBUCKET; i++){
		struct gryph *pg;
		for(pg = gryphpool[i]; pg; pg = pg->next){
			ret->count++;
			ret->bytes += offsetof(struct gryph, v) + (pg->size * pg->size + 7) / 8;
		}
	}
}

#endif
