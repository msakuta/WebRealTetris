#include "realt.h"
#include "set.h"
#include "tent.h"
#include "ini.h"
#include "../WinGraph.h"
#include <assert.h>
#include <commctrl.h>

#define pszSettings BYLANG("Settings", "設定")
#define pszSetLoad BYLANG("Load", "読込")
#define pszSetSave BYLANG("Save", "保存")
#define pszSetName BYLANG("Setting", "設定項目")
#define pszSetType BYLANG("Type", "型")
#define pszSetDesc BYLANG("Description", "説明")
#define pszSetValu BYLANG("Value", "値")

#define ADV(a,m) ((a) = ((a) + 1) % (m)) /* advance a ring buffer pointer. */
#define DEV(a,m) ((a) = ((a) - 1 + (m)) % (m)) /* devance a ring buffer pointer. */
#define RECTHIT(r, x, y) ((r).left < x && x < (r).right && (r).top < y && y < (r).bottom)
#define numof(a) (sizeof(a)/sizeof*(a))

#define SETSIZE MAX(MAX(sizeof pszSetName, sizeof pszSetValu), MAX(sizeof pszSetType, sizeof pszSetDesc))

/* Utility macros */
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

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof*(a))
#endif

static WNDPROC DefEditProc;
static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);

struct ui_state;


WINGRAPH wgs = {
	NULL, /* hWnd*/ NULL, /* hVDC */
	NULL, /* hVBmp */ NULL, /* pVWBuf */
	NULL, /* hPen */ NULL, /* hBrush */
	NULL, /* hFont */ 0, 0, /* xScr,yScr */
	0, /* dwUserParam */ {0}, /* hEventObj[WGR_TBLMAX] */
	0, /* dwParameter */
	pszSettings, /* szName[256] */
	SETW, /* nWidth */ SETH, /* nHeight */
	SetMsgProc, /* AppProc */
	-1, /* nFirstMsgIdx */
};

extern const WINGRAPH *const g_pwg;


static int s_seti = 0; /* setting index */
static RECT s_rLA, s_rRA, s_rBar, s_rSN, s_rSL, s_rSS, s_rVal, s_rSE; /* Save Settings button */
static enum lastselect{LS_NONE, LS_LA, LS_RA, LS_BAR, LS_SN, LS_SL, LS_SS, LS_VAL, LS_SE, NUM_LS} s_lastsel = LS_NONE; /* for buttons that take effects on mouse up */
static RECT *const s_parSet[NUM_LS] = {&s_rLA, &s_rRA, &s_rBar, &s_rSN, &s_rSL, &s_rSS, &s_rVal, &s_rSE};
static HWND hEdit, hButton, hCB, hCtrl[3];

#define SETARROW pus->fonth
#define SETUNIT 4
#define SETXOFS (4 + SETARROW)

/* Draw contents of settings window */
void SetDraw(LPWINGRAPH pwg, struct ui_state *pus){
	int x = 0, y = 0;
	int a = pus->seti;
	const unsigned char *p;
	static char strbuf[64];
	static int init = 0;
	if(!init){
		init = 1;
		s_rLA.left = 2, s_rLA.top = 2, s_rLA.right = 2 + SETARROW, s_rLA.bottom = pus->fonth - 2;
		s_rRA.left = s_rLA.right + SETUNIT * g_initsetsize + 4, s_rRA.top = 2;
		s_rRA.right = s_rRA.left + SETARROW, s_rRA.bottom = pus->fonth - 2;
		s_rBar.left = s_rLA.right + 2, s_rBar.top = 2, s_rBar.right = s_rRA.left - 2, s_rBar.bottom = pus->fonth - 2;
		s_rSS.right = SETW, s_rSS.top = 0;
		s_rSS.left = SETW - pus->fontw * sizeof pszSetSave, s_rSS.bottom = pus->fonth;
		s_rSL.left = s_rSS.left - pus->fontw * sizeof pszSetLoad, s_rSL.top = s_rSS.top;
		s_rSL.right = s_rSS.left, s_rSL.bottom = s_rSS.bottom;
		s_rSN.left = s_rRA.right + 2, s_rSN.top = 0;
		s_rSN.right = s_rSL.left, s_rSN.bottom = pus->fonth;
		s_rVal.left = pus->fontw *SETSIZE, s_rVal.top = 3 * pus->fonth, s_rVal.right = SETW, s_rVal.bottom = 4 * pus->fonth;
		s_rSE.top = s_rVal.top, s_rSE.right = SETW, s_rSE.bottom = s_rVal.bottom;
	//	InitCommonControls();
		hCtrl[0] = hEdit = CreateWindow("EDIT", "", WS_CHILD, s_rVal.left, s_rVal.top, s_rVal.right - s_rVal.left, s_rVal.bottom - s_rVal.top,
			wgs.hWnd, NULL, NULL, NULL);
		DefEditProc = GetWindowLongPtr(hEdit, GWLP_WNDPROC); /* Be sure to save initial procedure prior to setting */
		SetWindowLongPtr(hEdit, GWLP_WNDPROC, EditProc); /**/
		hCtrl[1] = hButton = CreateWindow("BUTTON", "", WS_CHILD | BS_FLAT | BS_LEFT | WS_VISIBLE, s_rVal.left, s_rVal.top, s_rVal.right - s_rVal.left, s_rVal.bottom - s_rVal.top,
			wgs.hWnd, NULL, NULL, NULL);
		hCtrl[2] = hCB = CreateWindow("COMBOBOX", "", WS_CHILD | CBS_DROPDOWNLIST, s_rVal.left, s_rVal.top, s_rVal.right - s_rVal.left, s_rVal.bottom - s_rVal.top,
			wgs.hWnd, NULL, NULL, NULL);
	}
	s_rSE.left = s_rVal.right = initset[pus->seti].cdc == &cdc_path ? SETW - pus->fontw *sizeof"..." : SETW;
	ClearScreenWin(pwg, WG_BLACK);

	/* Draw the bar and the triangles */
	FillTriangleWin(pwg, 2, pus->fonth / 2, SETARROW, pus->fonth - 2, SETARROW, 2, RGB(192, 192, 0));
	FillRectangleWin(pwg, 4 + SETARROW, 2, SETUNIT * g_initsetsize + 4 + SETARROW, pus->fonth - 2, RGB(40, 40, 0));
	RectangleWin(pwg, 4 + SETARROW, 2, SETUNIT * g_initsetsize + 4 + SETARROW, pus->fonth - 2, RGB(127, 127, 0));
	FillRectangleWin(pwg, 4 + SETARROW + SETUNIT * a, 2, 4 + SETARROW + SETUNIT * (a+1), pus->fonth - 2, RGB(255, 255, 0));
//	x = 6 + SETARROW + SETUNIT * g_initsetsize;
	x = s_rRA.left;
	FillTriangleWin(pwg, x + SETARROW - 2, pus->fonth / 2, x, pus->fonth - 2, x, 2, RGB(192, 192, 0));

	/* Draw setting number */
	sprintf(strbuf, "%d / %d", a+1, g_initsetsize);
	DrawFontWin(pwg, strbuf, SETUNIT * g_initsetsize + 8 + 2 * SETARROW, 0, RGB(255, 255, 127), NULL);

	/* Draw load/save button */
	FillRectangleWin(pwg, s_rSL.left, s_rSL.top, s_rSL.right, s_rSL.bottom, RGB(0, 40, 40));
	DrawFontWin(pwg, pszSetLoad, s_rSL.left + pus->fontw / 2, s_rSL.top, RGB(127, 255, 255), NULL);
	FillRectangleWin(pwg, s_rSS.left, s_rSS.top, s_rSS.right, s_rSS.bottom, RGB(40, 0, 40));
	DrawFontWin(pwg, pszSetSave, s_rSS.left + pus->fontw / 2, s_rSS.top, RGB(255, 127, 255), NULL);

	/* Draw contents of the current setting */
	DrawFontWin(pwg, pszSetName, 0, pus->fonth, RGB(127, 255, 127), NULL);
	DrawFontWin(pwg, initset[a].str, pus->fontw *SETSIZE, pus->fonth, WG_WHITE, NULL);
	DrawFontWin(pwg, pszSetType, 0, 2 * pus->fonth, RGB(127, 255, 127), NULL);
	DrawFontWin(pwg, initset[a].cdc->name, pus->fontw *SETSIZE, 2 * pus->fonth, WG_WHITE, NULL);
	DrawFontWin(pwg, pszSetValu, 0, 3 * pus->fonth, RGB(127, 255, 127), NULL);
	initset[a].cdc->printer(strbuf, &((char*)g_pinit)[(int)initset[a].dst], sizeof strbuf);
	if(initset[a].flags){ 
		RectangleWin(pwg, s_rVal.left, s_rVal.top, s_rVal.right, s_rVal.bottom, RGB(127, 127, 127));
		if(s_rSE.left != SETW){
			FillRectangleWin(pwg, s_rSE.left, s_rSE.top, s_rSE.right, s_rSE.bottom, RGB(0, 40, 0));
			DrawFontWin(pwg, "...", s_rSE.left + pus->fontw / 2, s_rSE.top, RGB(127, 255, 127), NULL);
		}
	}
	{
		int i;
		HWND h;
		h = initset[a].cdc == &cdc_eBlockDrawMethod ? hCB : initset[a].cdc == &cdc_bool ? hButton : hEdit;
		if(initset[a].cdc == &cdc_eBlockDrawMethod){
			SendMessage(h, CB_RESETCONTENT, 0, 0);
			SendMessage(h, CB_ADDSTRING, 0, "Border Only");
			SendMessage(h, CB_ADDSTRING, 0, "Fill Interior");
			SendMessage(h, CB_ADDSTRING, 0, "Addblend Interior");
		}
		for(i = 0; i < numof(hCtrl); i++) if(hCtrl[i] != h)
			ShowWindow(hCtrl[i], SW_HIDE);
/*		ShowWindow(h, SW_SHOW);*/
		SetWindowPos(h, HWND_TOP, s_rVal.left, s_rVal.top, s_rVal.right - s_rVal.left, s_rVal.bottom - s_rVal.top, SWP_SHOWWINDOW);
		EnableWindow(h, initset[a].flags);
		SendMessage(h, WM_SETTEXT, 0, strbuf);
	}
/*	DrawFontWin(pwg, strbuf, pus->fontw *SETSIZE, 3 * pus->fonth, initset[a].flags ? RGB(255, 255, 127) : RGB(127, 127, 127), NULL);
	DrawFontWin(pwg, pszSetDesc, 0, 4 * pus->fonth, RGB(127, 255, 127), NULL);*/
	p = initset[a].desc;
	y = 4 * pus->fonth;
	if(p) for(;; y += pus->fonth){
#if LANG==JP /* Shift-JIS aware*/
		int cc;
		for(cc = 0; p[cc];){
			if(0x81 <= p[cc] && p[cc] <= 0x9f || 0xe0 <= p[cc] && p[cc] <= 0xff){
				if(pwg->nWidth <= pus->fontw * (SETSIZE + cc + 1))
					break;
				cc++;
			}
			if(pwg->nWidth <= pus->fontw * (SETSIZE + cc))
				break;
			cc++;
		}
		strncpy(strbuf, p, cc);
		strbuf[cc] = '\0';
		DrawFontWin(pwg, strbuf, pus->fontw *SETSIZE, y, WG_WHITE, NULL);
		if(p[cc] == '\0') break;
		p += cc;
#else
		DrawFontWin(pwg, p, pus->fontw *SETSIZE, y, WG_WHITE, NULL);
		if(strlen(p) < (SETW - pus->fontw * SETSIZE) / pus->fontw) break;
		p += (SETW - pus->fontw * SETSIZE) / pus->fontw;
#endif
	}
	UpdateDraw(pwg);
}

/* Message procedure for the settings window */
BOOL SetMsgProc(LPWG_APPPROC p){
	static void *b = NULL;
	static size_t bsz = 0;
	int x, y;
	struct ui_state *const pus = (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam;
	//static int s_edit = 0;
	x = (WORD)(p->lParam & 0xffff), y = (WORD)(p->lParam >> 16);
	switch(p->Message){
		case WM_SYSCOMMAND:
			switch(p->wParam){
				case SC_CLOSE:
				if(!pus->setedit)
					PostMessage(g_pwg->hWnd, WM_APP, 0, 0); /* notify the game window to kill me */
				return TRUE;
				case SC_RESTORE:{
				LONG_PTR lp;
				lp = GetWindowLongPtr(wgs.hWnd, GWL_STYLE);
				if(lp)
					SetWindowLongPtr(wgs.hWnd, GWL_STYLE, lp &~(WS_HSCROLL|WS_VSCROLL));
				}
				return FALSE;
			}
			break;
		case WM_COMMAND:
			{
				int id = LOWORD(p->wParam);
				int event = HIWORD(p->wParam);
				HWND h = (HWND)p->lParam;
				if(h == hEdit && event == EN_CHANGE){
				}
				else if(h == hButton){
		case WM_APP:
					if(initset[pus->seti].cdc == &cdc_bool){
						*(bool*)&(((char*)g_pinit)[(int)initset[pus->seti].dst]) = !*(bool*)&(((char*)g_pinit)[(int)initset[pus->seti].dst]);
					}
					else {
						char buf[MAX_PATH];
						GetWindowText(hEdit, buf, sizeof buf);
						if(bsz < initset[pus->seti].cdc->size && NULL == (b = realloc(b, bsz = initset[pus->seti].cdc->size))){
							bsz = 0;
							return FALSE;
						}
						if(
							initset[pus->seti].cdc->parser(b, buf) &&
							(!initset[pus->seti].vrc || initset[pus->seti].vrc(b)) &&
							memcpy(&((char*)g_pinit)[(int)initset[pus->seti].dst], b, initset[pus->seti].cdc->size)){
							COLORREF *pb;
							if(!strcmp(initset[pus->seti].str, "BackgroundImage")/* &&
								NULL != (pb = LoadBmp((path_t*)&((char*)&s_init)[(int)initset[pus->seti].dst], XSIZ, YSIZ, XSIZ*YSIZ*sizeof*pb))){
								EnterCriticalSection(&csimg);
								if(pimg) free(pimg);
								pimg = pb;
								LeaveCriticalSection(&csimg);
							}*/)
								if(!ReloadBGImage(pus))
								/*	MessageBox(NULL, BYLANG("The specified file could not found or not supported format.", "あなたの言うファイルは存在しないか、フォーマットがサポートされていません"), BYLANG("Couldn't Load Image", "画像が読み込めません"), MB_ICONERROR)*/;
						}
					}
					SetDraw(&wgs, (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam);
				}
			}
			break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			{
			char buf[MAX_PATH];
			if(pus->setedit) break; /* no operations until editing is finished */
			else if(pus->setls == LS_SN && RECTHIT(s_rSN, x, y)){
				if(p->Message == WM_LBUTTONUP) ADV(pus->seti, ARRAYSIZE(initset));
				else if(p->Message == WM_RBUTTONUP) DEV(pus->seti, ARRAYSIZE(initset));
			}
			else if(pus->setls == LS_LA && RECTHIT(s_rLA, x, y)){
				DEV(pus->seti, g_initsetsize);
			}
			else if(pus->setls == LS_RA && RECTHIT(s_rRA, x, y)){
				ADV(pus->seti, g_initsetsize);
			}
			else if(pus->setls == LS_VAL && initset[pus->seti].flags && RECTHIT(s_rVal, x, y) /*SETSIZE < x && x < SETW && 3 * pus->fonth < y && y < 4 * pus->fonth*/){
				initset[pus->seti].cdc->printer(buf, &((char*)g_pinit)[(int)initset[pus->seti].dst], sizeof buf);
				if(initset[pus->seti].cdc == &cdc_bool){ /* special rule for bools */
					*(bool*)&(((char*)g_pinit)[(int)initset[pus->seti].dst]) = !*(bool*)&(((char*)g_pinit)[(int)initset[pus->seti].dst]);
				}
				else{ /* generally cdc->parser will do the job to interpret the input */
					pus->setedit = 1;
					if(Edit(&wgs, pus->fontw * SETSIZE, 3 * pus->fonth, SETW - pus->fontw * SETSIZE, pus->fonth, buf, sizeof buf, TRUE)){
loadpath:
						if(bsz < initset[pus->seti].cdc->size && NULL == (b = realloc(b, bsz = initset[pus->seti].cdc->size))){
							bsz = 0;
							return FALSE;
						}
						if(
							initset[pus->seti].cdc->parser(b, buf) &&
							(!initset[pus->seti].vrc || initset[pus->seti].vrc(b)) &&
							memcpy(&((char*)g_pinit)[(int)initset[pus->seti].dst], b, initset[pus->seti].cdc->size)){
							COLORREF *pb;
							if(!strcmp(initset[pus->seti].str, "BackgroundImage")/* &&
								NULL != (pb = LoadBmp((path_t*)&((char*)&s_init)[(int)initset[pus->seti].dst], XSIZ, YSIZ, XSIZ*YSIZ*sizeof*pb))){
								EnterCriticalSection(&csimg);
								if(pimg) free(pimg);
								pimg = pb;
								LeaveCriticalSection(&csimg);
							}*/)
								ReloadBGImage(pus);
						}
					}
					pus->setedit = 0;
				}
			}
			else if(pus->setls == LS_SE && initset[pus->seti].cdc == &cdc_path && RECTHIT(s_rSE, x, y)){
				static const char filter[] = 
					"Image Files\0*.bmp;*.jpg\0"
					"Bitmap Image File (*.bmp)\0*.bmp\0"
					"JPEG Image File (*.jpg)\0*.jpg\0"
					"all (*.*)\0*.*\0";
				OPENFILENAME ofn = {sizeof(OPENFILENAME), p->hWnd, 0, filter,
					NULL, 0, 0, buf, sizeof buf, NULL, 0, NULL, NULL, 0/*OFN_DONTADDTORECENT*/,
				};
				strncpy(buf, (path_t*)&((char*)g_pinit)[(int)initset[pus->seti].dst], sizeof buf);
				if(GetOpenFileName(&ofn)){
					goto loadpath;
				}
			}
			else if(pus->setls == LS_SS && RECTHIT(s_rSS, x, y)){
				static const char filter[] = "Initialization File (*.ini)\0*.ini\0";
				char buf[MAX_PATH] = "realt.ini";
				OPENFILENAME ofn = {sizeof(OPENFILENAME), p->hWnd, 0, filter,
					NULL, 0, 0, buf, sizeof buf, NULL, 0, NULL, NULL, 0/*OFN_DONTADDTORECENT*/,
				};
				if(GetSaveFileName(&ofn)){
					if(!SaveInitFile(g_pinit, ofn.lpstrFile, 0))
						MessageBox(wgs.hWnd, BYLANG("Failed to write", "書き込みに失敗しました"), PROJECT, MB_OK | MB_ICONHAND);
				}
			}
			else if(pus->setls == LS_SL && RECTHIT(s_rSL, x, y)){
				static const char filter[] = "Initialization File (*.ini)\0*.ini\0";
				char buf[MAX_PATH] = "realt.ini";
				OPENFILENAME ofn = {sizeof(OPENFILENAME), p->hWnd, 0, filter,
					NULL, 0, 0, buf, sizeof buf, NULL, 0, NULL, NULL, 0/*OFN_DONTADDTORECENT*/,
				};
				if(GetOpenFileName(&ofn)){
					path_t bgimg;
					memcpy(bgimg, g_pinit->BackgroundImage, sizeof bgimg);
					if(!LoadInitFile(g_pinit, ofn.lpstrFile))
						MessageBox(wgs.hWnd, BYLANG("Failed to read", "読み込みに失敗しました"), PROJECT, MB_OK | MB_ICONHAND);
					if(strncmp(bgimg, g_pinit->BackgroundImage, sizeof bgimg))
						ReloadBGImage(pus);
				}
			}
			else goto slider;
			SetDraw(&wgs, (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam);
			}
			break;
		case WM_LBUTTONDOWN:
			{
			int i;
			for(i = 0; i < NUM_LS-1; i++)
				if(RECTHIT(*s_parSet[i], x, y)){
					pus->setls = i + 1;
					goto slider;
				}
			pus->setls = LS_NONE;
			}
		case WM_MOUSEMOVE:
slider:
			if(!pus->setedit && pus->setls == LS_BAR && p->wParam & MK_LBUTTON && SETXOFS < x && x < SETXOFS + SETUNIT * g_initsetsize && 2 < y && y < pus->fonth - 2){
				pus->seti = (x - SETXOFS) / SETUNIT;
				assert(0 <= pus->seti && pus->seti < g_initsetsize);
				SetDraw(&wgs, (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam);
			}
			break;
		case WM_CTLCOLOREDIT:
		{
			HDC hdcs, hdce;
			HGDIOBJ hsf;
			hdcs = GetWindowDC(wgs.hWnd);
			SetBkColor((HDC)p->wParam, RGB(127, 63, 0));
	//		hdce = GetWindowDC(hEdit);
	//		SelectObject(hdce, GetCurrentObject(hdcs, OBJ_FONT));
			SelectObject((HDC)p->wParam, GetCurrentObject(hdcs, OBJ_FONT));
			ReleaseDC(wgs.hWnd, hdcs);
	//		ReleaseDC(hEdit, hdce);
			break;
		}
		default:
			return FALSE;
	}
	return FALSE;
}

/* wrapper function of edit control, catching enter key input. */
static LRESULT CALLBACK EditProc(HWND hw, UINT m, WPARAM wParam, LPARAM lParam){
	if(m == WM_KEYDOWN && wParam == VK_RETURN)
		return SendMessage(GetParent(hw), WM_APP, 0, 0);
	return DefEditProc(hw, m, wParam, lParam);
}

