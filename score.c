/* high score handling module */
#include "realt.h"
#include <windows.h>
#include "../WinGraph.h"
#include <stdio.h>

#define pszScoreWinName "RealTetris Highscore"

#define numof(a) (sizeof(a)/sizeof*(a))

static BOOL ScoreMsgProc(LPWG_APPPROC p);

static int s_scores[32];
static int g_fonth, g_fontw;
static int pos = 0;
static WINGRAPH wgh = {
	NULL, /* hWnd*/ NULL, /* hVDC */
	NULL, /* hVBmp */ NULL, /* pVWBuf */
	NULL, /* hPen */ NULL, /* hBrush */
	NULL, /* hFont */ 0, 0, /* xScr,yScr */
	0, /* dwUserParam */ {0}, /* hEventObj[WGR_TBLMAX] */
	0, /* dwParameter */
	pszScoreWinName, /* szName[256] */
	200, /* nWidth */ 320, /* nHeight */
	ScoreMsgProc, /* AppProc */
	-1, /* nFirstMsgIdx */
};

/* draw contents of the highscore window */
static void DrawScore(void){
	int i, fontw, fonth;
	if(!wgh.hWnd) return;
	GetFontSizeWin(&wgh, &fontw, &fonth);
	ClearScreenWin(&wgh, WG_BLACK);
	for(i = 0; i < numof(s_scores); i++) if(0 <= i - pos && i - pos < (wgh.nHeight + fonth - 1) / fonth){
		char buf[64];
		sprintf(buf, "%3d:", i+1);
		DrawFontWin(&wgh, buf, 0, fonth * (i - pos), RGB(i * 256 / numof(s_scores), 255, (numof(s_scores) - i) * 256 / numof(s_scores)), NULL);
		sprintf(buf, "%d", s_scores[i]);
		DrawFontWin(&wgh, buf, 5 * fontw, fonth * (i - pos), WG_WHITE, NULL);
	}
	/* draw scroll bar */
	FillRectangleWin(&wgh, wgh.nWidth - 16, 16, wgh.nWidth, wgh.nHeight - 16, RGB(31,31,0));
	RectangleWin(&wgh, wgh.nWidth - 16, 16, wgh.nWidth, wgh.nHeight - 16, RGB(127,127,0));
	{int top = 17 + pos * (wgh.nHeight - 34) / numof(s_scores);
	FillRectangleWin(&wgh, wgh.nWidth - 15, top, wgh.nWidth - 1, top + (wgh.nHeight / fonth) * (wgh.nHeight - 34) / numof(s_scores), RGB(255,255,0));
	}
	UpdateDraw(&wgh);
}

static BOOL ScoreMsgProc(LPWG_APPPROC p){
	static int hold = 0;
	switch(p->Message){
		case WM_KEYDOWN:
			if(p->wParam == VK_UP && 0 < pos){
				pos--;
				DrawScore();
			}
			else if(p->wParam == VK_DOWN && pos < numof(s_scores) - wgh.nHeight / g_fonth){
				pos++;
				DrawScore();
			}
			break;
		case WM_MOUSEMOVE:
			if(!hold) break;
		case WM_LBUTTONDOWN:{
			int x, y;
			x = (WORD)(p->lParam & 0xffff), y = (WORD)(p->lParam >> 16);
			if(wgh.nWidth - 16 < x && x < wgh.nWidth && 16 < y && y < wgh.nHeight - 16){
				int tpos = (y - 16) * numof(s_scores) / (wgh.nHeight - 34);
				if(numof(s_scores) - wgh.nHeight / g_fonth < tpos) tpos = numof(s_scores) - wgh.nHeight / g_fonth;
				hold = 1;
				if(pos != tpos){
					pos = tpos;
					DrawScore();
				}
			}
			/* up button */
			else if(wgh.nWidth - 16 < x && x < wgh.nWidth && y < 16 && 0 < pos){
				pos--;
				DrawScore();
			}
			else if(wgh.nWidth - 16 < x && x < wgh.nWidth && 16 < y && pos < numof(s_scores) - wgh.nHeight / g_fonth){
				pos++;
				DrawScore();
			}
			}
			break;
		case WM_LBUTTONUP:
			if(hold) hold = 0;
			break;
	}
	return FALSE;
}

int PutScore(int v){
	int i;
	for(i = 0; i < numof(s_scores); i++) if(s_scores[i] < v){
		if(i+1 < numof(s_scores))
			memmove(&s_scores[i+1], &s_scores[i], (numof(s_scores) - i-1) * sizeof*s_scores);
		s_scores[i] = v;
		break;
	}
	DrawScore();
	return i == numof(s_scores) ? -1 : i;
}

int ShowScore(void){
	if(wgh.hWnd) return 0;
	strncpy(wgh.szName, pszScoreWinName, sizeof wgh.szName);
	if(!CreateGraphicsWindow(&wgh)) return 0;
	EnableThickFrame(&wgh, FALSE);
	wgh.bUserUpdate = TRUE;
	GetFontSizeWin(&wgh, &g_fontw, &g_fonth);
	DrawScore();
	return 1;
}

int LoadScore(const char *file){
	FILE *fp;
	fp = fopen(file, "rb");
	if(!fp) return 0;
	fread(s_scores, sizeof*s_scores, numof(s_scores), fp);
	fclose(fp);
	return 1;
}

int SaveScore(const char *file){
	FILE *fp;
	fp = fopen(file, "wb");
	if(!fp) return 0;
	fwrite(s_scores, sizeof*s_scores, numof(s_scores), fp);
	fclose(fp);
	return 1;
}
int HideScore(void){
}
int IsScoreWindowActive(){
	return 1;
}