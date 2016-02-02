/* here lie are codes that obsolete but might be referred in future */

static void Draw(unsigned t){
#ifndef NDEBUG
	static clock_t foffset;
	static double drawtime = 0.;
	clock_t offset;
	LARGE_INTEGER li1, li2;
	if(gs.hrt)
		QueryPerformanceCounter(&li1);
	else
		offset = clock();

#endif

//	ClearScreenWin(&wg,WG_BLACK);
	FillRectangleWin(&wg, 0, 0, XSIZ, YSIZ, WG_BLACK);

#if !defined NDEBUG && FTIMEGRAPH
	{ /* frametime log draw */
	int i = s_curftl, j = 0;
//	for(i = 0; i < ARRAYSIZE(s_ftlog); i++){
	do{
		double d = 200 * s_ftlog[i] * MAX_BLOCK_HEIGHT;
		LineWin(&wg, j, MAX_BLOCK_HEIGHT, j, MIN(YSIZ, MAX_BLOCK_HEIGHT + (int)d), RGB(0, 64, 128));
		j++;
	} while(ADV(i, ARRAYSIZE(s_ftlog)) != s_curftl);
	}
#endif

	if(pimg && !FADESCREEN)
		PutPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, pimg);
	DrawTefbox(&wg, g_ptefl);
	/* game over line */
#if MEMORYGRAPH
	{
	int x = 0;
	int y = g_tetimes.teline_m * g_tetimes.so_teline_t / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 80, 0));
	x += DrawFontWin(&wg, "teline", x, y, RGB(80, 80, 0), NULL) * g_fontw;
	y = g_tetimes.tefbox_m * g_tetimes.so_tefbox_t / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 0, 0));
	x += DrawFontWin(&wg, "tefbox", x, y, RGB(80, 0, 0), NULL) * g_fontw;
	y = g_tetimes.tefpol_m * g_tetimes.so_tefpol_t / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(0, 80, 80));
	x += DrawFontWin(&wg, "tefpol", x, y, RGB(0, 80, 80), NULL) * g_fontw;
	y = g_pinit->PolygonVertexBufferSize * g_tetimes.so_tevert_t / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(0, 80, 0));
	x += DrawFontWin(&wg, "tevert", x, y, RGB(0, 80, 0), NULL) * g_fontw;
	y = (g_tetimes.teline_m * g_tetimes.so_teline_t +
		g_tetimes.tefbox_m * g_tetimes.so_tefbox_t +
		g_tetimes.tefpol_m * g_tetimes.so_tefpol_t +
		(g_tetimes.tevert_s ? g_pinit->PolygonVertexBufferSize * g_tetimes.so_tevert_t : 0)) / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 80, 80));
	DrawFontWin(&wg, "total", x, y, RGB(80, 80, 80), NULL);
	}
#endif
	if(!FADESCREEN){
		LineWin(&wg, 0, MAX_BLOCK_HEIGHT, XSIZ, MAX_BLOCK_HEIGHT, RGB(0, 0, 80));
	}
	DrawTefpol(&wg, g_ptepl);
	DrawTeline(&wg, g_ptell);

	if(FADESCREEN){
#ifndef NDEBUG
	char strbuf[64];
#endif
	int rt = 0, gt = 0, bt = 0;
//	smoothing(ptb, peb, 1, XSIZ, YSIZ);
//	memcpy(peb, ptb, XSIZ * YSIZ * sizeof *peb);
#define ADDCR(a,b) RGB(MIN(255,(GetRValue(a)+GetRValue(b))/2),MIN(255,(GetGValue(a)+GetGValue(b))/2),MIN(255,(GetBValue(a)+GetBValue(b))/2))
#define ADDCR2(a,b,c) RGB(MIN(255,(2*GetRValue(a)+GetRValue(b)+GetRValue(c))/4),MIN(255,(2*GetGValue(a)+GetGValue(b)+GetGValue(c))/4),MIN(255,(2*GetBValue(a)+GetBValue(b)+GetBValue(c))/4))
#define ADDCR3(a,b,c,d) RGB((3*GetRValue(a)+GetRValue(b)+GetRValue(c)+GetRValue(d))/6,(3*GetGValue(a)+GetGValue(b)+GetGValue(c)+GetGValue(d))/6,(3*GetBValue(a)+GetBValue(b)+GetBValue(c)+GetBValue(d))/6)
#define TC4(a) ((a)<0?~(-(a)-1):(a))
#define DTC4(a) ((a)&0x8?~(a)+1:(a))
#if TRANSBUFFER == 1
#define TTT
#else
#define TTT TC4
#endif
#define TFACTOR 2
#define ADDTR2(a,b,c) ((2*TTT((a)&0xf)+TTT((b)&0xf)+TTT((c)&0xf))/4|((2*((a)>>4)+((b)>>4)+((c)>>4))/4)<<4)
#define ADDTR3(a,b,c,d) ((3*TTT((a)&0xf)+TTT((b)&0xf)+TTT((c)&0xf)+TTT((d)&0xf)+TFACTOR)/6|((3*((a)>>4)+((b)>>4)+((c)>>4)+((d)>>4)+TFACTOR)/6)<<4)
	/*if(gs.fc % 2)*/{
		int i, j, k;
		for(i = 0; i < YSIZ; i++){
			peb[i*XSIZ] = ADDCR2(peb[i*XSIZ], peb[(i+1)*XSIZ], peb[(i+1)*XSIZ+1]);
			for(j = 1; j < XSIZ-1; j++){
				COLORREF *pc = &peb[i*XSIZ+j];
#if (TURBULSCREEN || BURNSCREEN)
				int r;
				r = rand();
#endif
#ifndef NDEBUG
				rt += GetRValue(*pc);
				gt += GetGValue(*pc);
				bt += GetBValue(*pc);
#endif
#if TURBULSCREEN
                if(j != 1 && j != XSIZ-2) switch(r % 4){
#	if TURBULSCREEN == 1
					case 0: *pc = ADDCR2(*pc, pc[XSIZ-1], pc[XSIZ]); goto turbulexit;
					case 1: *pc = ADDCR2(*pc, pc[XSIZ], pc[XSIZ+1]); goto turbulexit;
#	else
					case 0: *pc = ADDCR3(*pc, pc[XSIZ-2], pc[XSIZ-1], pc[XSIZ]); goto turbulexit;
					case 1: *pc = ADDCR3(*pc, pc[XSIZ], pc[XSIZ+1], pc[XSIZ+2]); goto turbulexit;
#	endif
				}
#endif
				*pc = ADDCR3(*pc, pc[XSIZ-1], pc[XSIZ], pc[XSIZ+1]);
turbulexit:;
#if BURNSCREEN
				if((r / 4) % 4096 == 0) for(k = 0; k < 24; k += 8)
				if(64 < (0xff & (((*pc)>>k))))
					*pc |= (0xff << k);
#endif
			}
			peb[i*XSIZ+XSIZ-1] = ADDCR2(peb[i*XSIZ+XSIZ-1], peb[(i+1)*XSIZ+XSIZ-2], peb[(i+1)*XSIZ+XSIZ-1]);
		}
		peb[(YSIZ-1)*XSIZ] = ADDCR2(peb[(YSIZ-1)*XSIZ], peb[0], peb[1]);
		for(j = 1; j < XSIZ-1; j++){
			COLORREF *pc = &peb[(YSIZ-1)*XSIZ+j];
			*pc = ADDCR3(*pc, peb[j-1], peb[j], peb[j+1]);
		}
		peb[(YSIZ-1)*XSIZ+XSIZ-1] = ADDCR2(peb[(YSIZ-1)*XSIZ+XSIZ-1], peb[XSIZ-2], peb[XSIZ-1]);
//		subconstant(&peb[(YSIZ-1)*XSIZ], RGB(8,8,8), XSIZ * sizeof *peb, bMMX);
	}
#if TRANSBUFFER
	/*if(t % 512 < 256 ? t % ((t % 512) / 64 + 1) == 0 : t % (4 - (t % 256) / 64) == 0)*/{
		int i, j;
		for(i = 0; i < YSIZ-1; i++){
			pbb[i*XSIZ] = ADDTR2(pbb[i*XSIZ], pbb[(i+1)*XSIZ], pbb[(i+1)*XSIZ+1]);
			for(j = 1; j < XSIZ-1; j++){
				byte *pb = &pbb[i*XSIZ+j];
#if TURBULTRANS & 2
                if(j != 1 && j != XSIZ-2) switch(rand() % 4){
#	if TURBULTRANS & 4
					case 0: *pb = ADDTR2(*pb, pb[XSIZ-1], pb[XSIZ]); goto transexit;
					case 1: *pb = ADDTR2(*pb, pb[XSIZ], pb[XSIZ+1]); goto transexit;
#	else
					case 0: *pb = ADDTR3(*pb, pb[XSIZ-2], pb[XSIZ-1], pb[XSIZ]); goto transexit;
					case 1: *pb = ADDTR3(*pb, pb[XSIZ], pb[XSIZ+1], pb[XSIZ+2]); goto transexit;
#	endif
				}
#endif
				*pb = ADDTR3(*pb, pb[XSIZ-1], pb[XSIZ], pb[XSIZ+1]);
transexit:;
				if(*pb){
					register int k, l;
					byte r;
					r = rand() & 0xfff;
					k = *pb & 0xf, l = *pb >> 4;
#if TURBULTRANS & 1
					if(!(r & 0x3)){/* randomly turbulate */
						if(r & 0x4){
							if(k != 0xf) k ++;
						}
						else if(k != 0) k --;
					}
					if(!(r & 0x18)){
						if(r & 0x20){
							if(l != 0xf) l ++;
						}
						else if(l != 0) l --;
					}
#endif
					*pb = k | (l << 4);
				}
/*				if(rand() % 4096 == 0){
					if(4 < (0xf & *pb))
						*pb |= 0xf;
					if(4 < (*pb >> 4))
						*pb |= 0xf0;
				}
*/			}
			pbb[i*XSIZ+XSIZ-1] = ADDTR2(pbb[i*XSIZ+XSIZ-1], pbb[(i+1)*XSIZ+XSIZ-2], pbb[(i+1)*XSIZ+XSIZ-1]);
		}
		pbb[(YSIZ-1)*XSIZ] = ADDTR2(pbb[(YSIZ-1)*XSIZ], pbb[0], pbb[1]);
		for(j = 1; j < XSIZ-1; j++){
			byte *pb = &pbb[(YSIZ-1)*XSIZ+j];
			*pb = ADDTR3(*pb, pbb[j-1], pbb[j], pbb[j+1]);
		}
		pbb[(YSIZ-1)*XSIZ+XSIZ-1] = ADDTR2(pbb[(YSIZ-1)*XSIZ+XSIZ-1], pbb[XSIZ-2], pbb[XSIZ-1]);
	}
#endif
	GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);
	addblend(peb, ptb, 15, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
	if(t % 2){
		unsigned t2 = t/2;
		int r = (t2 % 512 < 256 ? t2 : (255 - t2 % 256)) % 256 / 16;
		COLORREF c = RGB(r, (t2 % 0x300 < 0x180 ? t2 : (0x17f - t2 % 0x180)) % 0x180 / 0x18, 16 - r /* (255 - (t2 % 512 < 256 ? t2 : 255 - (t2 % 256))) / 16*/);
		subconstant(peb, c, XSIZ * YSIZ * sizeof *peb, bMMX);
#ifndef NDEBUG
		{
		int i;
		const int thickness = 3;
		for(i = 0; i < 3; i++){
		FillRectangleWin(&wg, 10+2*(0xff&(c>>(i*8))), i*thickness+YRES - 2 * g_fonth, 10+2*16, (i+1)*thickness+YRES - 2 * g_fonth, 0x80<<(i*8));
		FillRectangleWin(&wg, 10, i*thickness+YRES - 2 * g_fonth, 10+2*(0xff&(c>>(i*8))), (i+1)*thickness+YRES - 2 * g_fonth, 0x7f7f7f|(0xff<<(i*8)));
/*		HPEN hPen;
		hPen = CreatePen(PS_SOLID, thickness, 0x80<<(i*8));
		if(!hPen) break;
		SelectObject(wg.hVDC, hPen);
		MoveToEx(wg.hVDC, 10+2*16, i*thickness+YRES - 2 * g_fonth, NULL);
		LineTo(wg.hVDC, 10+2*(0xff&(c>>(i*8))), i*thickness+YRES - 2 * g_fonth);
		hPen = CreatePen(PS_SOLID, thickness, 0x7f7f7f|(0xff<<(i*8)));
		if(!hPen) break;
		SelectObject(wg.hVDC, hPen);
		LineTo(wg.hVDC, 10, i*thickness+YRES - 2 * g_fonth);
		DeleteObject(hPen);
*/		}}
/*		LineWin(&wg, 10, YRES - 2 * g_fonth, 10+2*r, YRES - 2 * g_fonth, RGB(255,0,0));
		LineWin(&wg, 10, YRES - 2 * g_fonth+1, 10+2*GetGValue(c), YRES - 2 * g_fonth+1, RGB(0,255,0));
		LineWin(&wg, 10, YRES - 2 * g_fonth+2, 10+2*GetBValue(c), YRES - 2 * g_fonth+2, RGB(0,0,255));
*/
		sprintf(strbuf, "(%i,%i,%i)", r, GetGValue(c), GetBValue(c));
		DrawFontWin(&wg, strbuf, 10, YRES - 3 * g_fonth, WG_WHITE, NULL);
	}
	sprintf(strbuf, "(%8.5g,%8.5g,%8.5g)", (double)rt / ((XSIZ-1)*YSIZ), (double)bt / ((XSIZ-1)*YSIZ), (double)gt / ((XSIZ-1)*YSIZ));
	DrawFontWin(&wg, strbuf, 10, YRES - g_fonth, WG_WHITE, NULL);
#else
	}
#endif
	GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);
#if TRANSBUFFER
#	if 1
	{/* transbuffer add */
		int i, j;
		for(i = 0; i < YSIZ; i++) for(j = 1; j < XSIZ-1; j++) if(ptb[i*XSIZ+j]){
			unsigned k;
			k = GetRValue(ptb[i*XSIZ+j]) / 128;
#		if TRANSBUFFER > 1
			if(7 < k) k = 7;
			if(rand() % 2) k = ~(k-1);
#		endif
			pbb[i*XSIZ+j] += k /*MIN(7, (GetRValue(ptb[i*XSIZ+j])*//*+GetGValue(ptb[i*XSIZ+j])+GetBValue(ptb[i*XSIZ+j])*//*) / 128))*/ << 4 |
				MIN(16, GetGValue(ptb[i*XSIZ+j]) / 128);
		}
	}
#	endif
#endif
	addblend(ptb, peb, 100, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
	if(pimg)
		alphablend(ptb, pimg, g_pinit->BackgroundBrightness, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
#if TRANSBUFFER
	{/* transcoloring */
		int i, j;
		COLORREF *pc;
		for(i = 0; i < YSIZ; i++){
			for(j = 1; j < XSIZ-1; j++){
				byte pb;
/*				pbb[i*XSIZ+j] += MIN(16, (GetRValue(ptb[i*XSIZ+j])) / 128) << 4 |
					MIN(16, GetGValue(ptb[i*XSIZ+j]) / 128);*/
				pb = pbb[i*XSIZ+j];
				if(pb){
#if 0
					if(gs.fc % 2 == 0)
						ptb[i*XSIZ+j] = RGB((pb&0xf) << 4, pb & 0xf0, 0);
					else
#endif
					{
						int k;
#if TRANSBUFFER == 1
						k = i*XSIZ+j + (pb & 0xf) + (pb >> 4) * XSIZ;
#else
						k = i*XSIZ+j + (pb & 0x8 ? ~(pb & 0x7)+1 : (pb & 0x7)) + (pb >> 4) * XSIZ;
#endif
						if(0 < k && k < XSIZ*YSIZ)
							ptb[i*XSIZ+j] = ptb[k];
					}
				}
			}
		}
	}
#endif
	PutPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);
	LineWin(&wg, 0, MAX_BLOCK_HEIGHT, XSIZ, MAX_BLOCK_HEIGHT, RGB(0, 0, 80));
	}

#if 0
	if(t % 23 == 0){
		block_t temp;
		i = rand() % bl.cur;
		BlockBreak(&bl.l[i]);
		KillBlock(&bl, i);
		do{
			temp.r = rand() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH + (temp.l = rand() % (XSIZ - MIN_BLOCK_WIDTH));
			if(XSIZ < temp.r) temp.r = XSIZ;
			temp.b = rand() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT + (temp.t = rand() % (YSIZ / 4 - MIN_BLOCK_HEIGHT));
			if(YSIZ < temp.b) temp.r = YSIZ;
		} while(GetAt(bl.l, bl.cur, &temp, bl.cur) != bl.cur);
		AddBlock(&bl, temp.l, temp.t, temp.r, temp.b);
		AddTefbox(g_ptefl, temp.l, temp.t, temp.r, temp.b, RGB(0, 0, 128), WG_BLACK, 32);
	}
#endif

//	if(!gs.pause)
//		AnimBlock(&bl, t);
	DrawBlock(&wg, &bl, gs.fc);

	/* game over message */
	if(gs.go){
#define PRESSPACE BYLANG("PRESS SPACE BAR TO RESTART", "スペースバーで開始")
		DrawFontWin(&wg, "GAME OVER", XSIZ / 2 - g_fontw * sizeof "GAME OVER" / 2 , YSIZ / 2 - g_fonth * 2, WG_WHITE,NULL);
		DrawFontWin(&wg, PRESSPACE, XSIZ / 2 - g_fontw * sizeof PRESSPACE / 2 , YSIZ / 2 - g_fonth, WG_WHITE,NULL);
	}
	/* game over is prior */
	if(gs.pause && g_pinit->DrawPauseString){
		DrawFontWin(&wg, "PAUSED", XSIZ / 2 - g_fontw * sizeof "PAUSED" / 2, YSIZ / 2, WG_WHITE,NULL);
	}

	/* side panel draw */
	gs.lastbar -= gs.ft;
	if(g_pinit->SidePanelRefreshRate == 0. || gs.lastbar < 0){
	if(g_pinit->SidePanelRefreshRate) while(gs.lastbar < 0) gs.lastbar += g_pinit->SidePanelRefreshRate;

	/* Clear Rect area */
	FillRectangleWin(&wg, XSIZ+1, 0, XRES, YRES, WG_BLACK);

	/* game zone frame */
	LineWin(&wg, XSIZ, 0, XSIZ, YSIZ,
		RGB(0xff - (gs.fc % 0x200 < 0x100 ? gs.fc : 0xff - (gs.fc&0xff)), 255, gs.fc % 0x200 < 0x100 ? gs.fc : 0xff - (gs.fc&0xff)) /* WG_YELLOW */);

	/* draw Next block */
	{
	COLORREF col = RGB(t % 512 < 256 ? t : 255 - (t % 256), 255, 255 - (t % 512 < 256 ? t : 255 - (t % 256)));
	DrawFontWin(&wg, "Next Block:", (XSIZ + XRES)/2 - g_fontw * sizeof "Next Block:" / 2, 300 - MAX_BLOCK_HEIGHT/2 - g_fonth, col, NULL);
	FillRectangleWin(&wg, (XSIZ + XRES)/2 - MAX_BLOCK_WIDTH/2, 300 - MAX_BLOCK_HEIGHT/2, (XSIZ + XRES)/2 + MAX_BLOCK_WIDTH/2, 300 + MAX_BLOCK_HEIGHT/2, RGB(GetBValue(col)/4,GetGValue(col)/4,GetRValue(col)/4));
	FillRectangleWin(&wg, (XSIZ + XRES)/2 - MIN_BLOCK_WIDTH/2, 300 - MIN_BLOCK_HEIGHT/2, (XSIZ + XRES)/2 + MIN_BLOCK_WIDTH/2, 300 + MIN_BLOCK_HEIGHT/2, WG_BLACK);
	RectangleWin(&wg, (XSIZ + XRES)/2 - gs.nextx / 2, 300 - gs.nexty / 2, (XSIZ + XRES)/2 + gs.nextx / 2, 300 + gs.nexty / 2,
	 col);
	}

	/* draw info strings. */
	{
    static char strbuf[64];
#ifndef NDEBUG
	/* print frame time */
	if(gs.hrt){
		sprintf(strbuf, "fps: %-8g (%-8g)", 1. / gs.ft, gs.avgft ? 1. / gs.fft : 1.);
	}
	else{
		sprintf(strbuf, "MSPF: %-8d", (unsigned)(clock() - foffset));
		foffset = clock();
	}
	{
	int y = 0;
	DrawFontWin(&wg, strbuf, XSIZ + 10, 0, WG_WHITE,NULL);
	sprintf(strbuf, "time: %8f", gs.t);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "frametime: %8g", gs.ft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "averageft: %8g", gs.avgft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "points: %u", gs.points);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "area: %u", gs.area);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "blocks: %-3u/%-3u", bl.cur, bl.size);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "p/g:b/g: %-5g:%-5g", (double)gs.fc / gs.c, (double)g_pinit->SidePanelRefreshRate / gs.ft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "vx: %i/%i(%u)", gs.vx, g_pinit->max_vx, gs.xc);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "Tefboxes: %-4u/%-4u|%u", g_tetimes.tefbox_c, g_tetimes.tefbox_m, g_tetimes.tefbox_s);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "Telines: %-4u/%-4u|%u", g_tetimes.teline_c, g_tetimes.teline_m, g_tetimes.teline_s);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "Tefpol: %-4u/%-4u|%u", g_tetimes.tefpol_c, g_tetimes.tefpol_m, g_tetimes.tefpol_s);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "Vtx: %-4u/%-4u|%u", g_tetimes.tevert_c, g_pinit->PolygonVertexBufferSize, g_tetimes.tevert_s);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, gs.slept == INFINITE ? "slept not" : "slept: %-8u ms", gs.slept);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "Draw: %8g", drawtime);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y = YSIZ - 3 * g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "Animate: %8g", animtime);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "AnimTefbox: %8g", g_tetimes.animtefbox);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "AnimTeline: %8g", g_tetimes.animteline);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "DrawTefbox: %8g", g_tetimes.drawtefbox);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "DrawTeline: %8g", g_tetimes.drawteline);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	}
#else
	sprintf(strbuf, "Frame Rate: %-8g", 1. / gs.ft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 0, WG_WHITE,NULL);
	sprintf(strbuf, "points: %u", gs.points);
	DrawFontWin(&wg, strbuf, XSIZ + 10, g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "erased: %u", gs.area);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 2*g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "blocks: %-3u", bl.cur);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 3*g_fonth, WG_WHITE,NULL);
#endif
	{
		int x = XSIZ + 10;
		DrawFontWin(&wg, "BGBr", x, YRES - g_fonth, WG_WHITE,NULL);
		FillRectangleWin(&wg, s_xbgbr, YRES - g_fonth, s_xbgbr + 100, YRES - 0, RGB(40, 10, 5));
		sprintf(strbuf, "%i", g_pinit->BackgroundBrightness);
		DrawFontWin(&wg, strbuf, s_xbgbr + 4 + g_pinit->BackgroundBrightness, YRES - g_fonth, RGB(255, 127, 127),NULL);
		x = s_xbgbr + g_pinit->BackgroundBrightness;
		LineWin(&wg, x, YRES - g_fonth, x, YRES, RGB(255, 127, 127));
		FillRectangleWin(&wg, s_rSet.left, s_rSet.top, s_rSet.right, s_rSet.bottom, RGB(5, 10, 40));
		DrawFontWin(&wg, pszSettings, s_rSet.left + g_fontw / 2, s_rSet.top, RGB(0, 255, 255),NULL);
		FillRectangleWin(&wg, s_rExit.left, s_rExit.top, s_rExit.right, s_rExit.bottom, RGB(40, 0, 40));
		DrawFontWin(&wg, pszExit, s_rExit.left + g_fontw / 2, s_rExit.top, RGB(255, 128, 255),NULL);
	}
	}
	/* update bar area */
	UpdateDrawRect(&wg, XSIZ, 0, XRES, YRES);
	}

	/* update game area */
	UpdateDrawRect(&wg, 0, 0, XSIZ-1, YSIZ);

#ifndef NDEBUG
	if(gs.hrt){
		QueryPerformanceCounter(&li2);
		drawtime = (double)(li2.QuadPart - li1.QuadPart) / gs.cps.QuadPart;
	}
	else{
		drawtime = (double)(clock() - offset) / CLOCKS_PER_SEC;
	}
#endif
}





static BOOL MsgProc(LPWG_APPPROC p){
/*	char strbuf[64];
	static clock_t offset, foffset;
	static unsigned t = 0x1000000, i = 0;
	static UINT_PTR tid;
	block_t temp;*/
	static int iset = 0;

	switch(p->Message){
		case WM_SYSCOMMAND:
			switch(p->wParam){
				case SC_CLOSE:
				InterlockedIncrement(&g_close);
				p->lRet = 0;
				return TRUE;
				case SC_RESTORE:{
				LONG_PTR lp;
				lp = GetWindowLongPtr(wg.hWnd, GWL_STYLE);
				if(lp)
					SetWindowLongPtr(wg.hWnd, GWL_STYLE, lp &~(WS_HSCROLL|WS_VSCROLL));
				}
				return FALSE;
			}
			break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
			if(p->wParam & MK_LBUTTON)
			{
			int x, y;
			x = (WORD)(p->lParam & 0xffff), y = (WORD)(p->lParam >> 16);
			if(s_xbgbr < x && x < s_xbgbr + 100 && YRES - g_fonth < y && y < YRES)
				s_init.BackgroundBrightness = x - s_xbgbr;
			}
			break;
		case WM_LBUTTONUP:
			{
			int x, y;
			x = (WORD)(p->lParam & 0xffff), y = (WORD)(p->lParam >> 16);
			if(s_rSet.left < x && x < s_rSet.right && s_rSet.top < y && y < s_rSet.bottom){
				if(!wgs.hWnd){
					CreateGraphicsWindow(&wgs);
					EnableThickFrame(&wgs, FALSE);
					wgs.bUserUpdate = TRUE;
					SetDraw(&wgs);
				}
				else{
					RestoreModeWin(&wgs);
					wgs.hWnd = NULL;
					strncpy(wgs.szName, pszSettings, sizeof wgs.szName);
				}
			}
			else if(s_rExit.left < x && x < s_rExit.right && s_rExit.top < y && y < s_rExit.bottom){
				g_close = 1;
			}
			else if(RECTHIT(s_rBGBr, x, y)){
cyclebgm:
				ADV(s_init.BackgroundBlendMethod, 3);
			}
			}
			break;
		case WM_KEYUP:
			if(p->wParam == 66){ /* B key */
				goto cyclebgm;
			}
			break;
#if 0
		case	WM_CREATE:
//			tid = SetTimer(p->hWnd, IDT_TIMER, FRAMERATE, FrameProc);
//			Debris(XSIZ / 2, YSIZ / 2, 20);
//			Explosion(XSIZ / 2, YSIZ / 2, 20);
			offset = foffset = clock();
			wg.bUserUpdate = TRUE;	/* 描画はアプリケーション更新 */
			break;
		case	WM_DESTROY:
//			KillTimer(p->hWnd,IDT_TIMER);
	/*		PostQuitMessage(0);*/
			break;
		case	WM_TIMER:
/*	if(t){*/
/*		int cx, cy;
		cx = rand() % XSIZ;
		cy = rand() % YSIZ;
//		switch(t % 60){
//			case 0: Explosion(cx, cy, 25); break;
//			case 20: Stars(cx, cy, 20, WG_BLACK); break;
//			case 40: Debris(cx, cy, 20); break;
//		}
*/
			wg.bUserUpdate = TRUE;	/* 描画はアプリケーション更新 */

			if(-5 < gs.vx && GetAsyncKeyState(VK_LEFT))
				gs.vx--;
//				SlipLeft(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MAX(bl.l[bl.cur-1].l - 1, 0));
			if(gs.vx < 5 && GetAsyncKeyState(VK_RIGHT))
				gs.vx++;
//				SlipRight(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MIN(bl.l[bl.cur-1].r + 1, XSIZ));
			if(GetAsyncKeyState(VK_DOWN)){
				SlipDown(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MIN(bl.l[bl.cur-1].b + 8,YSIZ));
			}
			if(gs.delay == 0 && GetAsyncKeyState(VK_SPACE) & ~1){
				int top;
				top = bl.l[bl.cur-1].t;
				SlipDown(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, YSIZ);
				AddTefbox(g_ptefl, bl.l[bl.cur-1].l, top, bl.l[bl.cur-1].r, bl.l[bl.cur-1].b, RGB(0, 64, 64), WG_BLACK, 32);
				gs.vx = 0;
				gs.delay = 10; /* unable to fall for 10 ticks */
			}
			/* P key */
			if(gs.delay == 0 && GetAsyncKeyState(0x50) & ~1){
				gs.pause = !gs.pause;
				gs.delay = 10; /* unable to fall for 10 ticks */
			}
/*
			if(GetAsyncKeyState(VK_UP)){
				temp = bl.l[bl.cur-1];
				ROTATEC(temp);
				if(GetAt(bl.l, bl.cur-1, &temp, bl.cur-1) == bl.cur-1)
					bl.l[bl.cur-1] = temp;
			}
*/
			Draw(gs.fc);
			gs.c++;
			if(gs.vx && gs.t % (6 - (gs.vx < 0 ? -gs.vx : gs.vx)) == 0)
				gs.vx += gs.vx < 0 ? 1 : -1;
			if(gs.delay) gs.delay--;
//		UpdateDraw(&wg);
/*	}
	else{
		KillTimer(p->hWnd, IDT_TIMER);
		sprintf(strbuf, "%-63d", clock() - offset);
		strbuf[63] = '\0';
		DrawFontWin(&wg, strbuf, 20, 20, WG_WHITE,NULL);
		UpdateDraw(&wg);
	}*/
			break;
#endif
/*
		case WM_KEYDOWN:
			switch(p->wParam){
				case VK_LEFT:
					SlipLeft(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MAX(bl.l[bl.cur-1].l - 1, 0));
					break;
				case VK_RIGHT:
					SlipRight(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MIN(bl.l[bl.cur-1].r + 1, XSIZ));
					break;
				case VK_DOWN:
					SlipDown(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MIN(bl.l[bl.cur-1].b + 8,YSIZ));
					break;
				case VK_SPACE:
					SlipDown(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, YSIZ);
					break;
			}
			break;
*/
		default:
			return FALSE;
	}

	return FALSE;
}

