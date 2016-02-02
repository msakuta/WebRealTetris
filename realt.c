#include "realt.h"
#include "tent.h"
#include "set.h"
#include "../lib/colseq.h"
#define VC_EXTRALEAN
#include <windows.h>
#include "ini.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include "dprint.h"
#include "debug.h"
#include "../WinGraph.h"
#ifndef NDEBUG
/*#include "tcounter.h"*/
#endif
#include "swapb.h"
#include "../lib/timemeas.h"
#include "../lib/color.h"
#include "../lib/cs2.h"
#include "../lib/rseq.h"

/* Switching macros */
#define RESTORE_MISSING_KEYS 1 /* whether append missing keys verbosely in the ini file */
#define FTIMEGRAPH 0 /* frametime graph, debug only */
#define MEMORYGRAPH 1 /* memory usage graph in comparition with the screen buffer, debug only */
#define TRANSBUFFER 1 /* whether use transforming effect in the screen buffer */
#define TRANSFUZZY 0 /* add fuzzyness onto transforming effect */
#define FADESCREEN (g_pinit->EnableFadeScreen) /* wheter the screen buffer is enabled */
#define BURNSCREEN 1 /* burnlet effects on the screen buffer */
#define TURBULSCREEN 2 /* turbulating effect on the screen buffer */
#define TURBULTRANS 1 /* turbulating effect on the transforming effect buffer */
#define BACKGRND 1 /* enable background image feature */
#define SERVICE 1 /* whether play fireworks on game over */
#define MULTH 1 /* multi thread */

/* turn off certain macros in release mode */
#ifdef NDEBUG
#undef FTIMEGRAPH
#define FTIMEGRAPH 0
#undef MEMORYGRAPH
#define MEMORYGRAPH 0
#endif

/* Utility macros */
#ifndef M_PI
#define M_PI 3.141592653589793238
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

/* Constant macros */
#define IDT_TIMER   0 /* game frame timer identifier */
#define MAX_BLOCK_WIDTH  50
#define MAX_BLOCK_HEIGHT 50
#define MIN_BLOCK_WIDTH  10
#define MIN_BLOCK_HEIGHT 10
#define MAX_DELAY 25 /* maximum block destroying delay */
#define MIN_SPACE (XSIZ * g_pinit->ClearRate)/*(XSIZ * 0.15)*/
#define ALT_SPACE (XSIZ * g_pinit->AlertRate)/*(XSIZ * 0.3)*/
#define INNERBOX_OFFSET 3
#define	pszWinName "Real Tetris (build "__DATE__" "__TIME__")" BYLANG(""," 日本語版")
#define pszPause BYLANG("Pause", "ポーズ")
#define pszSettings BYLANG("Settings", "設定")
#define pszScore BYLANG("Highscore", "ハイスコア")
#define pszExit BYLANG("Exit", "終了")
#define pszSetLoad BYLANG("Load", "読込")
#define pszSetSave BYLANG("Save", "保存")
#define pszSetName BYLANG("Setting", "設定項目")
#define pszSetType BYLANG("Type", "型")
#define pszSetDesc BYLANG("Description", "説明")
#define pszSetValu BYLANG("Value", "値")
#define SETSIZE MAX(MAX(sizeof pszSetName, sizeof pszSetValu), MAX(sizeof pszSetType, sizeof pszSetDesc))
#define INIFILE "realt.ini"
#define NUM_BREAKTYPES 5
#define ALLOCUNIT 0x80 /* anti-fragmentation */

/* fixed point value related macros */
#define FIXFACTOR 0x10000
#define FIXED(a) ((DWORD)((a) * FIXFACTOR))
#define FTOI(a) ((a) / FIXFACTOR)
#define FTOD(a) ((double)(a) / FIXFACTOR)
#define FIXEDG FIXED(GRAVITY)

/* Functional macros */
#define SET(rect,x,y) ((rect).r -= (rect).l - (x), (rect).l = (x), (rect).b -= (rect).t - (y), (rect).t = (y))
#define SET2(rect,x,y) {int dx=(rect).r-(rect).l, dy=(rect).b-(rect).t; (rect).l=(x), (rect).t=(y), (rect).r=(rect).l+dx, (rect).b=(rect).t+dy}
#define ROTATE(b) {int x = (b).r + (b).l, y = (b).b + (b).l; (b).r = ((b).l + (b).r) / 2 + ((b).b - (b).t) / 2;}
#define ROTATEC(b) {int x = (b).r; (b).r = (b).b - (b).t, (b).b = x - (b).l;}
#define ADV(a,m) ((a) = ((a) + 1) % (m)) /* advance a ring buffer pointer. */
#define DEV(a,m) ((a) = ((a) - 1 + (m)) % (m)) /* devance a ring buffer pointer. */
#define SIZE(begin,end,m) (((end) + (m) - (begin)) % (m)) /* get count of elements in a ring buffer. */
#define drand() ((double)rand() / RAND_MAX)
#define ARRAYSIZE(a) (sizeof(a)/sizeof*(a))
#define INDEXOF(a,m) ((int)((char*)(a)-(char*)&(m))/sizeof(m))
#define RECTHIT(r, x, y) ((r).left < x && x < (r).right && (r).top < y && y < (r).bottom)
#define Intersects(r1,r2) ((r1).r < (r2).l && (r2).r < (r1).l && (r1).b < (r2).t && (r2).b < (r1).t)
#define GetAsyncKeyState IsKeyActive
#define SPEED ((sqrt(gs.points / 2)) * (10. / (bl.cur + 10)) * bl.atop / (YSIZ - MAX_BLOCK_HEIGHT))

typedef DWORD fixed; /* fixed point representation, divide this by FIXFACTOR to gain intended value. */
typedef struct block BLOCK;

typedef struct block{
	int l, t, r, b; /* dimension of the block. */
//	fixed vx, vy; /* velocity in fixed format. display is in integral coordinates system, anyway. */
	unsigned life; /* if not UINT_MAX, the block is to be removed after this frames. */
} block_t;

/* stack-like data structure to store blocks. blocks must be able to exist in
  any number, so it is allocated in heap space. reallocated by a defined unit.
   I chose this data structure because tetris blocks tend to stack, heh! */
typedef struct block_list{
	unsigned cur;
	unsigned size;
	int atop; /* top of all brocks, unimportant */
	block_t *l;
} bl_t;

/* visual effects */
static tell_t *g_ptell;
static tefl_t *g_ptefl;
static tepl_t *g_ptepl;

/* Game status enough for save game */
static struct game_state{
	double t; /* elapsed time in seconds. */
	unsigned delay; /* command recognize delay */
	unsigned fc; /* graphical frame count */
	unsigned c; /* physical simulation steps */
	unsigned dpc; /* delta physical count */
	unsigned points; /* points the player obtained */
	unsigned area; /* erased area */
	int vx; /* horizontal velocity of currently controlling block */
	int go; /* game over? */
	int pause; /* if paused */
	int nextx; /* dimension of next falling block */
	int nexty;
	unsigned xc;
	unsigned slept;
	block_t *phit; /* hit block */
	BOOL hrt; /* whether high-res timer is available */
	double last, downlast, lastbar/*, xlast*/;
	double ft; /* frame time */
	double avgft; /* average frame time */
	double fft; /* flattened frame time, like average but of recent several frames */
/*	LARGE_INTEGER cps;*/ /* counts per second for high-resolution timer */
	timemeas_t tm;
	struct keystate{
		unsigned l : 1; /* left arrow */
		unsigned r : 1; /* right arrow */
		unsigned u : 1; /* up arrow */
		unsigned d : 1; /* down arrow */
		unsigned sp : 1; /* space bar */
		unsigned s : 1; /* suicide key */
		unsigned p : 1; /* pause (p) */
	} key;
} gs = {0,0,0,0,0,0,0,0,0,0};

/* GUI or graphics specific status for a set of windows */
static struct ui_state us = {NULL,0,0,0,0};

static int s_xbgbr;
static RECT s_rBGBr, s_rPause, s_rSet, s_rScore, s_rExit;

static LPCOLORREF peb = NULL, ptb = NULL, pimg = NULL;
static CRITICAL_SECTION csimg;
#if TRANSBUFFER
static byte *pbb = NULL, *pbtb = NULL;
#endif
static BOOL bMMXAvailable, bMMX;
static volatile long g_close = 0;

#ifndef NDEBUG
static double s_ftlog[XSIZ] = {0};
static unsigned s_curftl = 0;
#endif

/* color sequences */
#define DEFINE_COLSEQ(cnl,colrand,life) {ARRAYSIZE(cnl),(cnl),(colrand),(life)}
typedef const colnode_t cn_t;
typedef const struct color_sequence cs_t;
static const colnode_t cnl_redflash[] = { /* block death remain */
	{0.25, RGB(127, 0, 0)},
	{0, WG_WHITE},
	{1., RGB(128, 0, 0)},
//	{0, WG_BLACK},
};
static const struct color_sequence cs_redflash = DEFINE_COLSEQ(cnl_redflash, (COLORREF)-1, 1.25);
static const struct color_sequence cs_randomflash = DEFINE_COLSEQ(cnl_redflash, RGB(128,0,0), 1.25);
static const colnode_t cnl_rainbow[] = { /* cycle of 7 colors */
	{0.25, RGB(255,0,0)},
	{0.25, RGB(255,255,0)},
	{0.25, RGB(0,255,0)},
	{0.25, RGB(0,255,255)},
	{0.25, RGB(0,0,255)},
	{0.25, RGB(255,0,255)},
	{0.25, RGB(255,0,0)},
};
static const struct color_sequence cs_rainbow = DEFINE_COLSEQ(cnl_rainbow, (COLORREF)-1, 1.75);
static const colnode_t cnl_rainbow2[] = { /* fast rainbow */
	{0.05, RGB(255,0,0)},
	{0.05, RGB(255,255,0)},
	{0.05, RGB(0,255,0)},
	{0.05, RGB(0,255,255)},
	{0.05, RGB(0,0,255)},
	{0.05, RGB(255,0,255)},
	{0.05, RGB(255,0,0)},
};
static const struct color_sequence cs_rainbow2 = DEFINE_COLSEQ(cnl_rainbow2, (COLORREF)-1, 0.35);
colnode_t cnl_darkpurple[] = { /* Modifiable! */
	{0.5, RGB(32,0,64)},
};
static cs_t cs_darkpurple = DEFINE_COLSEQ(cnl_darkpurple, (COLORREF)-1, 0.5);
static const colnode_t cnl_fireburn[] = {
	{0.03, RGB(255, 255, 255)},
	{0.035, RGB(255, 255, 128)},
	{0.035, RGB(128, 32, 32)},
};
static const colnode_t cnl_blueburn[] = {
	{0.03, RGB(255, 255, 255)},
	{0.035, RGB(128, 128, 255)},
	{0.035, RGB(32, 32, 128)},
};
static const colnode_t cnl_purpleburn[] = {
	{0.03, RGB(255, 255, 255)},
	{0.035, RGB(255, 128, 255)},
	{0.035, RGB(128, 32, 128)},
};
static const colnode_t cnl_cyanburn[] = {
	{0.03, RGB(255, 255, 255)},
	{0.035, RGB(128, 255, 255)},
	{0.035, RGB(32, 128, 128)},
};
static const colnode_t cnl_redstar[] = {
	{0.1, RGB(255, 215, 255)},
	{0.2, RGB(128, 32, 32)},
};
static const colnode_t cnl_bluestar[] = {
	{0.1, RGB(255, 215, 255)},
	{0.2, RGB(32, 32, 128)},
};
static const colnode_t cnl_cyanstar[] = {
	{0.1, RGB(255, 215, 255)},
	{0.2, RGB(32, 128, 128)},
};
static const colnode_t cnl_yelstar[] = {
	{0.1, RGB(255, 215, 255)},
	{0.2, RGB(128, 128, 32)},
};
static const colnode_t cnl_blueshift[] = {
	{0.15, RGB(255, 128, 128)},
	{0.15, RGB(128, 128, 255)},
};
static const colnode_t cnl_redshift[] = {
	{0.15, RGB(128, 128, 255)},
	{0.15, RGB(255, 128, 128)},
};
static const colnode_t cnl_electric[] = {
	{0.25, RGB(255, 255, 255)},
	{0.25, RGB(255, 255, 255)},
}, cnl_bluethunder[] = {
	{0.25, RGB(255, 255, 255)},
	{0.25, RGB(127, 191, 255)},
}, cnl_redthunder[] = {
	{0.25, RGB(255, 255, 255)},
	{0.25, RGB(255, 191, 127)},
};
static const colnode_t cnl_hat[] = {
	{0.0, RGB(0,0,0)},
	{0.15, RGB(255,0,0)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(255,255,0)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(0,255,0)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(0,255,255)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(0,0,255)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(255,0,255)},
	{0.0, RGB(255,255,255)},
	{0.0, RGB(0,0,0)},
	{0.15, RGB(255,0,0)},
};
static const colseq_t cs_shotstar = DEFINE_COLSEQ(cnl_redstar, (COLORREF)-1, 0.3),
cs_fireburn = DEFINE_COLSEQ(cnl_fireburn, (COLORREF)-1, 0.1),
cs_blueburn = DEFINE_COLSEQ(cnl_blueburn, (COLORREF)-1, 0.1),
cs_purpleburn = DEFINE_COLSEQ(cnl_purpleburn, (COLORREF)-1, 0.1),
cs_cyanburn = DEFINE_COLSEQ(cnl_cyanburn, (COLORREF)-1, 0.1),
cs_caleidostar = DEFINE_COLSEQ(cnl_redstar, RGB(255, 215, 255), 0.3),
csl_electric[] = {
	DEFINE_COLSEQ(cnl_electric, RGB(255, 255, 255), 0.5),
	DEFINE_COLSEQ(cnl_bluethunder, RGB(255, 255, 255), 0.5),
	DEFINE_COLSEQ(cnl_redthunder, RGB(255, 255, 255), 0.5),
},
csl_fireworks[] = {
	DEFINE_COLSEQ(cnl_blueburn, (COLORREF)-1, 0.1),
	DEFINE_COLSEQ(cnl_purpleburn, (COLORREF)-1, 0.1),
	DEFINE_COLSEQ(cnl_cyanburn, (COLORREF)-1, 0.1),
	DEFINE_COLSEQ(cnl_fireburn, (COLORREF)-1, 0.1),
},
csl_allstar[] = {
	DEFINE_COLSEQ(cnl_redstar, (COLORREF)-1, 0.3),
	DEFINE_COLSEQ(cnl_bluestar, (COLORREF)-1, 0.3),
	DEFINE_COLSEQ(cnl_cyanstar, (COLORREF)-1, 0.3),
	DEFINE_COLSEQ(cnl_yelstar, (COLORREF)-1, 0.3),
	DEFINE_COLSEQ(cnl_blueshift, (COLORREF)-1, 0.3),
	DEFINE_COLSEQ(cnl_redshift, (COLORREF)-1, 0.3),
//	DEFINE_COLSEQ(cnl_hat, (COLORREF)-1, 0.875),
	DEFINE_COLSEQ(cnl_redstar, RGB(255, 215, 255), 0.3),
	DEFINE_COLSEQ(cnl_bluestar, RGB(255, 215, 255), 0.3),
	DEFINE_COLSEQ(cnl_cyanstar, RGB(255, 215, 255), 0.3),
	DEFINE_COLSEQ(cnl_yelstar, RGB(255, 215, 255), 0.3),
	DEFINE_COLSEQ(cnl_electric, RGB(255, 255, 255), 0.5),
//	DEFINE_COLSEQ(cnl_rainbow, (COLORREF)-1, 1.75),
	DEFINE_COLSEQ(cnl_rainbow2, (COLORREF)-1, 0.35),
//	DEFINE_COLSEQ(cnl_darkpurple, (COLORREF)-1, 0.5),
}, *const pcs = csl_allstar;
static const colnode_t cnl_nova[] = {
	{1.4, COLOR32RGBA(255,255,255,0)},
	{.2, COLOR32RGBA(255,255,255,255)},
	{.2, COLOR32RGBA(255,255,0,255)},
	{.2, COLOR32RGBA(0,255,0,255)},
	{0, COLOR32RGBA(0,0,0,0)},
};
static const colseq_t cs_nova = {ARRAYSIZE(cnl_nova), cnl_nova, 0, 2., 1};
static const colnode_t cnl_whitechip[] = {
	{1, COLOR32RGBA(255,255,255,255)},
	{0, COLOR32RGBA(255,255,255,0)},
};
static const colseq_t cs_whitechip = {ARRAYSIZE(cnl_whitechip), cnl_whitechip, 0, 1., 1};
static const HACKED_CS2(3,2) cs2_redfade = {
	3, 2,
	{
	{
		1,
		{
		{1, COLOR32RGBA(0,0,0,0)},
		{0, COLOR32RGBA(0,0,0,0)},
		}
	},
	{
		1,
		{
		{1, COLOR32RGBA(255,0,0,255)},
		{0, COLOR32RGBA(255,0,0,0)},
		}
	},
	{
		0,
		{
		{1, COLOR32RGBA(255,255,255,255)},
		{0, COLOR32RGBA(255,255,255,0)},
		}
	},
	}
};
#undef DEFINE_COLSEQ

/*-----------------------------------------------------------------------------
	static function declarations
-----------------------------------------------------------------------------*/
static void *allocBuffers(void); /* allocate fade/trans buffers */
/*static int LoadInitFile(const char *);
static int SaveInitFile(const char *);*/
LRESULT CALLBACK    WindowProc(HWND ,UINT ,WPARAM ,LPARAM );
static unsigned SlipDown(
	const BLOCK *paBlockList,
	unsigned     nBlockCount,
	block_t     *pTargetBlock,
	unsigned     nIgnoreBlockIndex,
	long         lDestinationY);
static unsigned SlipLeft(
	const BLOCK *paBlockList,
	unsigned     nBlockCount,
	BLOCK       *pTargetBlock,
	unsigned     nIgnoreBlockIndex,
	long         lDestinationX);
static unsigned SlipRight(
	const BLOCK *paBlockList,
	unsigned     nBlockCount,
	BLOCK       *pTargetBlock,
	unsigned     nIgnoreBlockIndex,
	long         lDestinationX);
static unsigned GetAt(
	const BLOCK *paBlockList,
	unsigned     nBlockCount,
	const BLOCK *pTargetBlock,
	unsigned     nIgnoreBlockIndex);
static unsigned AddBlock(bl_t *, int l, int t, int r, int b);
static unsigned KillBlock(bl_t *, unsigned);
static void AnimBlock(bl_t *, unsigned t);
static void DrawBlock(WINGRAPH *pwg, bl_t *, unsigned t);
static void BlockBreak(const block_t *b, int type);
static void initBlocks();
static BOOL MsgProc(LPWG_APPPROC p);
static VOID CALLBACK FrameProc(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
);
static SHORT IsKeyActive(int);
#if 0
static void SetDraw(LPWINGRAPH, struct ui_state*);
static BOOL SetMsgProc(LPWG_APPPROC p);
#endif
static LPCOLORREF LoadBmp(LPCSTR FileName,int nWidth,int nHeight,DWORD dwSize);
static LPCOLORREF LoadJPG(LPCSTR FileName,int nWidth,int nHeight,DWORD dwSize);

static WINGRAPH wg = {
	NULL, /* hWnd*/ NULL, /* hVDC */
	NULL, /* hVBmp */ NULL, /* pVWBuf */
	NULL, /* hPen */ NULL, /* hBrush */
	NULL, /* hFont */ 0, 0, /* xScr,yScr */
	0, /* dwUserParam */ {0}, /* hEventObj[WGR_TBLMAX] */
	0, /* dwParameter */
	pszWinName, /* szName[256] */
	XRES, /* nWidth */ YRES, /* nHeight */
	MsgProc /*/ NULL*/, /* AppProc */
	-1, /* nFirstMsgIdx */
}
#if 0
, wgs = {
	NULL, /* hWnd*/ NULL, /* hVDC */
	NULL, /* hVBmp */ NULL, /* pVWBuf */
	NULL, /* hPen */ NULL, /* hBrush */
	NULL, /* hFont */ 0, 0, /* xScr,yScr */
	(DWORD)&us, /* dwUserParam */ {0}, /* hEventObj[WGR_TBLMAX] */
	0, /* dwParameter */
	pszSettings, /* szName[256] */
	SETW, /* nWidth */ SETH, /* nHeight */
	SetMsgProc, /* AppProc */
	-1, /* nFirstMsgIdx */
}
#endif
;
const WINGRAPH *const g_pwg = &wg;
static RECT WinRect = {0,0,XRES,YRES};
static bl_t bl = {0, 0, 0, NULL};
static int g_fontw, g_fonth;
static struct init s_init = { /* the defaults; will be overridden by the ini file. */
	98.0, /* gravity */
	0.030, /* frametime */
	60, /* CapFrameRate */
	0.1, /* SidePanelRefreshRate */
	0.15, 0.50, /* ClearRate, AlertRate */
	0.5, /* InitBlockRate */
	8.0, 1.5, 2.5, /* BlockDebriLength, BlockDebriLifeMin, BlockDebriLifeMax */
	0.5, /* BlockFollowShadowLife */
	0.7, /* TelineElasticModulus */
	0.5, /* TelineShrinkStart */
	0.02, 0.5, 1.0, 0.25, /* fragm_factor, fragm_life_min, fragm_life_max, BlockFragmentTrailRate */
	50, 10, /* fragm_c_max, BlockFragmentStarWeight */
	5, 8, /* max_vx, max_vy */
	10, /* InitBlocks */
	1024, 256, 256, /* TelineMaxCount, TelineInitialAlloc, TelineExpandUnit */
	1, /* TelineAutoExpand */
	128, 64, 32, /* TefboxMaxCount, TefboxInitialAlloc, TefboxExpandUnit */
	1, 1, /* TefboxAutoExpand, TefpolAutoExpand */
	512, 256, 256, /* TefpolMaxCount, TefpolInitialAlloc, TefpolExpandUnit */
	2048, /* PolygonVertexBufferSize */
	2, 0, 0, /* GuidanceLevel, CollapseEffectType, TransBufferFuzzyness */
	30, 1, /* BackgroundBrightness, BackgroundBlendMethod */
	1, /* BlockDrawMethod */
	"bg.bmp", "realt.dat", /* BackgroundImage, HighScoreFileName */
	1, 0, 0, /* clearall, EnableFadeScreen, EnableTransBuffer */
	1, /* EnableSuicideKey */
	0, 1, 1, /* DrawPause, DrawPauseString, DrawGameOver */
	0, /* RoundValue */
	1, /* EnableMMX */
	1, /* EnableMultiThread */
};
const struct init *const g_pinit = &s_init;

static void MsgBoxErr(void){
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	MessageBox( NULL, (LPCTSTR)lpMsgBuf, PROJECT" Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

static LRESULT CALLBACK WndProc(
	HWND hwnd,      // handle to window
	UINT uMsg,      // message identifier
	WPARAM wParam,  // first message parameter
	LPARAM lParam   // second message parameter
){
	hwnd, uMsg, wParam, lParam;
	return 0;
}

int WINAPI WinMain(
					HINSTANCE		hInstance,
					HINSTANCE		hPrevInstance,
					LPSTR			szArgv,
					int 			nCmdShow)
{
//	WINGRAPH		wg;
/*	LPSTR			pszWinName = "Real Tetoris (build "__DATE__" "__TIME__")";
	HWND			hWnd;
	BYTE			Pat[PAT_MAX];
	DWORD			dwDispTime;
	BOOL			bShowOnly;
	int				nLv,nMiss,nRet;
	int				nFontWidth,nFontHeight;
	MSG Message;
	unsigned t, i;
	char strbuf[64];*/
	clock_t offset;
	LARGE_INTEGER li1, li2;
	nCmdShow; szArgv; hPrevInstance; /* nicht used */

#ifndef NDEBUG
	{
	time_t tp;
	time(&tp);
	dprintfile( fopen("debug.log", "w") );
	dprintf("Build: "__DATE__", "__TIME__". Session: %s", ctime(&tp));
	}
#endif

	srand((unsigned)time(NULL));	/* ランダムシード */

	STARTTIMERP(LoadInitFile)
	if(!LoadInitFile(&s_init, INIFILE))
		return 0;
	STOPTIMER(LoadInitFile)

	LoadScore(g_pinit->HighScoreFileName);

	if((g_pinit->EnableFadeScreen ? !allocBuffers() : 0) ||
	   !(g_ptell = NewTeline(g_pinit->TelineMaxCount, g_pinit->TelineInitialAlloc, g_pinit->TelineAutoExpand ? g_pinit->TelineExpandUnit : 0)) ||
	   !(g_ptefl = NewTefbox(g_pinit->TefboxMaxCount, g_pinit->TefboxInitialAlloc, g_pinit->TefboxAutoExpand ? g_pinit->TefboxExpandUnit : 0)) ||
	   !(g_ptepl = NewTefpol(g_pinit->TefpolMaxCount, g_pinit->TefpolInitialAlloc, g_pinit->TefpolAutoExpand ? g_pinit->TefpolExpandUnit : 0))){
		MessageBox(NULL, BYLANG(
			"Visual Effect Engine initialization failed",
			"視覚効果エンジンの初期化に失敗しました"), pszWinName, MB_OK | MB_ICONHAND);
		return 1;
	}
	if(FADESCREEN){
		bMMXAvailable = IsAvailableMMX();
		memset(ptb, 0, 2 * XSIZ * (1+YSIZ) * sizeof *peb);
		peb = &ptb[XSIZ * (1+YSIZ)];
#if TRANSBUFFER
		pbb = (byte*)&peb[XSIZ * (1+YSIZ)];
		pbtb = &pbb[XSIZ * (1+YSIZ)];
		{ /* fill transform buffer */
		int i, j;
		for(i = 0; i < YSIZ; i++) for(j = 0; j < XSIZ; j++)
			pbb[XSIZ*i+j] = (byte)(MAX(0, 15 - ((XSIZ / 2 - j) * (XSIZ / 2 - j) + (YSIZ / 2 - i) * (YSIZ / 2 - i)) / 512) |
			(MAX(0, 15 - ((XSIZ / 2 - j) * (XSIZ / 2 - j) + (YSIZ / 2 - i) * (YSIZ / 2 - i)) / 512) << 4));
		}
#endif
	}
	InitializeCriticalSection(&csimg);
	pimg = LoadBmp(g_pinit->BackgroundImage, XSIZ, YSIZ, XSIZ * YSIZ * sizeof *pimg);
	if(pimg && !FADESCREEN){
		int i, j;
		for(i = 0; i < YSIZ; i++) for(j = 0; j < XSIZ; j++){
			COLORREF *pc = &pimg[i*XSIZ+j];
			*pc = RGB(GetRValue(*pc) * g_pinit->BackgroundBrightness / 100,
				GetGValue(*pc) * g_pinit->BackgroundBrightness / 100,
				GetBValue(*pc) * g_pinit->BackgroundBrightness / 100);
		}
	}

	gs.nextx = rand() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH;
	gs.nexty = rand() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;

	/* settings */
/*	strcpy(wg.szName, pszWinName);
	wg.nWidth		= XRES;
	wg.nHeight		= YRES;
 	wg.AppProc		= NULL;*/
/*	wg.AppProc		= MsgProc;*/
/*	wg.nFirstMsgIdx = -1;*/
	wg.hInstance	= hInstance;

/*	gs.hrt = QueryPerformanceFrequency(&gs.cps);*/
	gs.go = 1;

	/* メインウインドウの作成 */
	if (CreateGraphicsWindow(&wg) == FALSE){
		MessageBox(NULL,
			BYLANG("Failed to create the window", "ウインドウの作成に失敗しました")
			, wg.szName,MB_OK | MB_ICONHAND);
		return 0;
	}

	/* init contents of teout */
	GetObject(us.to.hbm = wg.hVBmp, sizeof us.to.bm, &us.to.bm);
	us.to.hdc = CreateCompatibleDC(wg.hVDC);
	us.to.trans[0] = us.to.trans[3] = 1;
	us.to.clip.left = 0, us.to.clip.top = 0;
	us.to.clip.right = XSIZ, us.to.clip.bottom = YSIZ;

	/* Title bar buttons initialization */
	{
	LONG_PTR lp;
	lp = GetWindowLongPtr(wg.hWnd, GWL_STYLE);
	if(lp){
		SetWindowLongPtr(wg.hWnd, GWL_STYLE, (lp &~(WS_MAXIMIZEBOX|WS_HSCROLL|WS_VSCROLL)) /*|WS_MINIMIZEBOX|WS_MAXIMIZEBOX*/|WS_SYSMENU);
	}
	EnableThickFrame(&wg, FALSE);
	}
	{
#if 0
	ATOM atom;
	HWND hWnd;
	{
		WNDCLASS wc = {
			0, //    UINT       style; 
			DefWindowProc, //    WNDPROC    lpfnWndProc; 
			0, //    int        cbClsExtra; 
			0, //    int        cbWndExtra; 
			hInstance, //    HINSTANCE  hInstance; 
			NULL, //HICON      hIcon; 
			NULL, //HCURSOR    hCursor; 
			COLOR_BACKGROUND, //HBRUSH     hbrBackground; 
			NULL, //LPCTSTR    lpszMenuName;
			"namely", //LPCTSTR    lpszClassName; 
		};
		if(0 == (atom = RegisterClass(&wc))){
			MsgBoxErr();
			goto the_end;
		}
	}
	if(NULL == (hWnd = CreateWindow(atom, pszWinName, WS_OVERLAPPED | WS_VISIBLE, CW_USEDEFAULT, 0, 100, 100, NULL, NULL, NULL, NULL))){
		MsgBoxErr();
		goto the_end;
	}
#endif

	wg.bUserUpdate = TRUE;	/* 描画はアプリケーション更新 */

	TimeMeasStart(&gs.tm);

	if(gs.hrt){
		QueryPerformanceCounter(&li1);
		UpdateDraw(&wg);
		QueryPerformanceCounter(&li2);
		gs.ft = TimeMeasLap(&gs.tm)/*(double)(li2.QuadPart - li1.QuadPart) / gs.cps.QuadPart*/;
	}
	else{
		offset = clock();
		UpdateDraw(&wg);
		gs.ft = (double)(clock() - offset) / CLOCKS_PER_SEC;
	}

	/* gain font sizes */
#if 0
	GetFontSizeWin(&wg, &g_fontw, &g_fonth);
#else
	{
	TEXTMETRIC tm;
	if(!GetTextMetrics(wg.hVDC, &tm)){
		assert(0);
		MsgBoxErr();
		goto the_end;
	}
	us.fontw = g_fontw = tm.tmAveCharWidth, us.fonth = g_fonth = tm.tmHeight;
	}
#endif
	/* Set metrical information */
	s_xbgbr = XSIZ + 20 + g_fontw * sizeof"BG";
	s_rBGBr.left = XSIZ + 10, s_rBGBr.top = YRES - g_fonth;
	s_rBGBr.right = s_rBGBr.left + g_fontw * sizeof"BG", s_rBGBr.bottom = YRES;
	s_rExit.left = XRES - g_fontw * sizeof pszExit - 20, s_rExit.top = YRES - 2 * g_fonth;
	s_rExit.right = XRES - 20, s_rExit.bottom = YRES - g_fonth;
	s_rSet.left = s_rExit.left - g_fontw * sizeof pszSettings, s_rSet.top = YRES - 2 * g_fonth;
	s_rSet.right = s_rExit.left, s_rSet.bottom = YRES - g_fonth;
	s_rScore.right = s_rExit.right, s_rScore.top = s_rExit.top - g_fonth;
	s_rScore.left = s_rScore.right - g_fontw * sizeof pszScore, s_rScore.bottom = s_rScore.top + g_fonth;
	s_rPause.left = s_rSet.left - g_fontw * sizeof pszPause, s_rPause.top = YRES - 2 * g_fonth;
	s_rPause.right = s_rSet.left, s_rPause.bottom = YRES - g_fonth;

	do{
		FrameProc(0, WM_TIMER, 0, GetTickCount());
		if(GetAsyncKeyState(VK_ESCAPE) || g_close)
			break;
	}while(1);

//	WaitMouseButtonDown(&wg,INFINITE);	/* マウス待ち */
	ClearScreenWin(&wg,WG_BLACK);		/* クリア */

//	DestroyWindow(hWnd);
	}
the_end:
	RestoreModeWin(&wg);

//	TentExit();
	DelTeline(g_ptell);
	DelTefbox(g_ptefl);
	DelTefpol(g_ptepl);

	SaveScore(g_pinit->HighScoreFileName);

	return 0;
}

static void *allocBuffers(void){
	if(ptb) return ptb;
	ptb = malloc(2 * XSIZ * (1+YSIZ) * sizeof *peb
#if TRANSBUFFER
	+ 2 * XSIZ * (1+YSIZ) * sizeof *pbb
#endif
	);
	if(!ptb) return NULL;
	peb = &ptb[XSIZ * (1+YSIZ)];
#if TRANSBUFFER
	pbb = (byte*)&peb[XSIZ * (1+YSIZ)];
	pbtb = &pbb[XSIZ * (1+YSIZ)];
	{ /* fill transform buffer */
	int i, j;
	for(i = 0; i < YSIZ; i++) for(j = 0; j < XSIZ; j++)
		pbb[XSIZ*i+j] = (byte)(MAX(0, 15 - ((XSIZ / 2 - j) * (XSIZ / 2 - j) + (YSIZ / 2 - i) * (YSIZ / 2 - i)) / 512) |
		(MAX(0, 15 - ((XSIZ / 2 - j) * (XSIZ / 2 - j) + (YSIZ / 2 - i) * (YSIZ / 2 - i)) / 512) << 4));
	}
#endif

	/* pseudo bitmap */
	us.toptb.bm.bmBitsPixel = 32;
	us.toptb.bm.bmHeight = YSIZ;
	us.toptb.bm.bmWidth = XSIZ;
	us.toptb.bm.bmWidthBytes = XSIZ * sizeof*ptb;
	us.toptb.bm.bmBits = ptb;
	us.toptb.clip.left = us.toptb.clip.top = 0;
	us.toptb.clip.right = XSIZ;
	us.toptb.clip.bottom = YSIZ;
	us.toptb.trans[0] = us.toptb.trans[3] = 1;

	return peb;
}


static unsigned SlipDown(const BLOCK *pa, unsigned c, BLOCK* pr, unsigned ig, long dest){
	unsigned i, ret;
	long min;

	assert(pr->b <= dest);
	ret = c;
	min = dest;

	for(i = 0; i < c; i++){
		if(i != ig && pr->l < pa[i].r && pa[i].l < pr->r && pr->b <= pa[i].t && pa[i].t < min){
			ret = i;
			min = pa[i].t;
		}
	}

	min -= pr->b - pr->t;
	SET(*pr, pr->l, min);

	return ret;
}

static unsigned SlipLeft(const BLOCK *pa, unsigned c, BLOCK* pr, unsigned ig, long dest){
	unsigned i, ret;
	long max;

	assert(dest <= pr->l);
	ret = c;
	max = dest;

	for(i = 0; i < c; i++){
		if(i != ig && pr->t < pa[i].b && pa[i].t < pr->b && pa[i].r <= pr->l && max < pa[i].r){
			ret = i;
			max = pa[i].r;
		}
	}

	SET(*pr, max, pr->t);

	return ret;
}

static unsigned SlipRight(const BLOCK *pa, unsigned c, BLOCK* pr, unsigned ig, long dest){
	unsigned i, ret;
	long min;

	assert(pr->r <= dest);
	ret = c;
	min = dest;

	for(i = 0; i < c; i++){
		if(i != ig && pr->t < pa[i].b && pa[i].t < pr->b && pr->r <= pa[i].l && pa[i].l < min){
			ret = i;
			min = pa[i].l;
		}
	}

	min -= pr->r - pr->l;
	SET(*pr, min, pr->t);

	return ret;
}

/* Check intersection to each block */
static unsigned GetAt(const BLOCK *pa, unsigned c, const BLOCK *p, unsigned ig){
	unsigned i;

	for(i = 0; i < c; i++)
		if(i != ig && p->l < pa[i].r && pa[i].l < p->r && p->t < pa[i].b && pa[i].t < p->b)
			return i;

	return c;
}

static unsigned AddBlock(bl_t *p, int l, int t, int r, int b){
#ifndef NDEBUG
	{
		block_t tmp; tmp.l = l, tmp.t = t, tmp.r = r, tmp.b = b;
		assert(l <= r && t <= b);
		assert(GetAt(p->l, p->cur, &tmp, p->cur) == p->cur);
		assert(0 <= l && 0 <= t && r <= XSIZ && b <= YSIZ);
	}
#endif

	/* stack overflow! */
	if(p->size == p->cur)
		p->l = realloc(p->l, (p->size += ALLOCUNIT) * sizeof *p->l);

#define tel p->l[p->cur]
	tel.l = l;
	tel.t = t;
	tel.r = r;
	tel.b = b;
	tel.life = UINT_MAX;
#undef tel

	/* advance the pointer. */
	p->cur++;
	return p->cur;
}

static unsigned KillBlock(bl_t *p, unsigned index){
	unsigned i;

	assert(index < p->cur);
	for(i = index; i < p->cur; i++){
		p->l[i] = p->l[i + 1];
	}
	return --p->cur;
}

static void spawnNextBlock(void){
		block_t temp;
		unsigned retries = 0;
		do{
			if(100 < retries++)
				break;
			temp.r = gs.nextx + (temp.l = rand() % (XSIZ - MIN_BLOCK_WIDTH - gs.nextx));
//			if(XSIZ < temp.r) temp.r = XSIZ;
			temp.b = gs.nexty + (temp.t = rand() % (YSIZ / 4 - MIN_BLOCK_HEIGHT - gs.nexty));
//			if(YSIZ < temp.b) temp.r = YSIZ;
		} while(GetAt(bl.l, bl.cur, &temp, bl.cur) != bl.cur);
		gs.vx = 0;
		AddBlock(&bl, temp.l, temp.t, temp.r, temp.b);
		AddTefbox(g_ptefl, temp.l, temp.t, temp.r, temp.b, RGB(0, 0, 128), WG_BLACK, 1.0, 0);

		gs.nextx = rand() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH;
		gs.nexty = rand() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;

#ifndef NDEBUG
		gs.points++;
#endif
}

static void AnimBlock(bl_t *p, unsigned t){
	unsigned i, j;
	int spaceb, spacet, yt, yb, moved = 0;
//	block_t temp;

	if(!gs.go && p->cur == 1){
		initBlocks();
		if(gs.points){ /* must have erased some to gain clear bonus */
			gs.points += 10; /* bonus points */
			us.message = "CLEAR BONUS!!";
			us.mdelay = 2000;
			AddTeline(g_ptell, XSIZ/2, YSIZ/2, 0, 0, 70, 0, 0, 0, &cs_nova, TEL_GRADCIRCLE, 3);
		}
	}

	p->atop = (YSIZ - MAX_BLOCK_HEIGHT);
#if 1
	for(i = 0; i < p->cur; i++){
		if(p->l[i].life != UINT_MAX){
			if(p->l[i].life == 0){
				if(!gs.go){
					gs.points++;
					gs.area += (p->l[i].r - p->l[i].l) * (p->l[i].b - p->l[i].t);
				}
#if SERVICE
				BlockBreak(&p->l[i], g_pinit->CollapseEffectType ?
					g_pinit->CollapseEffectType : us.lastbreak ? us.lastbreak : 1 + rand() % NUM_BREAKTYPES);
#else
				BlockBreak(&p->l[i], gs.go ? 0 : us.lastbreak);
#endif
				KillBlock(p, i);
				continue;
			}
			--p->l[i].life;
		}

		/* don't check currently falling block, because it's annoying to see blocks
		  explode in the air */
		if(i+1 == p->cur)
			break;

		/* detect atop of brocks to consider how the player suffers. */
		if(p->l[i].t < p->atop) p->atop = p->l[i].t;

		/* check horizontal spaces */
		spaceb = spacet = XSIZ;
		for(j = 0; j+1 < p->cur; j++){
			if(i == j) continue;
			/* bottom line */
			if(p->l[j].t < p->l[i].b && p->l[i].b <= p->l[j].b)
				spaceb -= p->l[j].r - p->l[j].l;
			/* top line */
			if(p->l[j].t <= p->l[i].t && p->l[i].t < p->l[j].b)
				spacet -= p->l[j].r - p->l[j].l;
		}
		if(spaceb < MIN_SPACE || spacet < MIN_SPACE){
			yb = p->l[i].b;
			yt = p->l[i].t;
			us.lastbreak = 1 + rand() % NUM_BREAKTYPES;
			/* destroy myself. */
			p->l[i].life = rand() % MAX_DELAY;
			/* destroy all blocks in a horizontal line. */
			if(spaceb < MIN_SPACE)
			for(j = 0; j < p->cur; j++){
				if(p->l[j].t < yb && yb <= p->l[j].b){
					p->l[j].life = rand() % MAX_DELAY;
/*					BlockBreak(&p->l[j]);
					KillBlock(p, j);
					points++;*/
				}
			}
			if(spacet < MIN_SPACE)
			for(j = 0; j < p->cur; j++){
				if(p->l[j].t <= yt && yt < p->l[j].b){
					p->l[j].life = rand() % MAX_DELAY;
/*					BlockBreak(&p->l[j]);
					KillBlock(p, j);
					points++;*/
				}
			}
		}
	}
#else
	for(i = 0; i < p->cur; i++){
		if(p->l[i].life != UINT_MAX){
			if(p->l[i].life == 0){
				if(!gs.go){
					gs.points++;
					gs.area += (p->l[i].r - p->l[i].l) * (p->l[i].b - p->l[i].t);
				}
				BlockBreak(&p->l[i]);
				KillBlock(p, i);
				continue;
			}
			--p->l[i].life;
		}
	}
	{
		block_t *pb = &p->l[p->cur-1];
		/* check horizontal spaces */
		spaceb = spacet = XSIZ;
		for(j = 0; j < p->cur; j++){
			if(p->cur-1 == j) continue;
			/* bottom line */
			if(p->l[j].t < pb->b && pb->b <= p->l[j].b)
				spaceb -= p->l[j].r - p->l[j].l;
			/* top line */
			if(p->l[j].t <= pb->t && pb->t < p->l[j].b)
				spacet -= p->l[j].r - p->l[j].l;
		}
		if(spaceb < MIN_SPACE || spacet < MIN_SPACE){
			yb = pb->b;
			yt = pb->t;
			/* destroy myself. */
			pb->life = rand() % MAX_DELAY;
			/* destroy all blocks in a horizontal line. */
			if(spaceb < MIN_SPACE)
			for(j = 0; j < p->cur; j++){
				if(p->l[j].life != UINT_MAX && p->l[j].t < yb && yb <= p->l[j].b){
					p->l[j].life = rand() % MAX_DELAY;
				}
			}
			if(spacet < MIN_SPACE)
			for(j = 0; j < p->cur; j++){
				if(p->l[j].life != UINT_MAX && p->l[j].t <= yt && yt < p->l[j].b){
					p->l[j].life = rand() % MAX_DELAY;
				}
			}
		}
	}
#endif

	for(i = 0; i < p->cur; i++){
		int l, t;
		if(p->l[i].life != UINT_MAX) continue;
	/*	if(SlipDown(p->l, p->cur, &p->l[i], i, YSIZ < p->l[i].b + 3 ? YSIZ : p->l[i].b + 3) != p->cur || p->l[i].b == YSIZ)
			SlipLeft(p->l, p->cur, &p->l[i], i, p->l[i].l - 1 < 0 ? 0 : p->l[i].l - 1);*/
		l = p->l[i].l;
		t = p->l[i].t;
		if(i == p->cur - 1){
			unsigned ind;
			if(gs.vx > 0){
				if(XSIZ < p->l[i].r + gs.vx)
					gs.vx = XSIZ - p->l[i].r;
				SlipRight(p->l, p->cur, &p->l[i], i, p->l[i].r + gs.vx);
			}
			else if(gs.vx < 0){
				if(p->l[i].l + gs.vx < 0)
					gs.vx = -p->l[i].l;
				SlipLeft(p->l, p->cur, &p->l[i], i, p->l[i].l + gs.vx);
			}
			{
			int spd = (int)SPEED;
			for(j = 0; j <= spd; j++)
			if(p->cur != (ind = SlipDown(p->l, p->cur, &p->l[i], i, MIN(p->l[i].b + 1, YSIZ)))){
				gs.phit = &p->l[ind];
				break;
			}
			else
				gs.phit = NULL;
			}
		}
		else
			SlipDown(p->l, p->cur, &p->l[i], i, MIN(p->l[i].b + 3, YSIZ));

		if(/*l != p->l[i].l ||*/ t != p->l[i].t)
			moved = 1;
	}

	if(moved == 0 && gs.go == 0){
		/* if the top is above the dead line, game is over. */
		if(p->cur && p->l[p->cur-1].t < MAX_BLOCK_HEIGHT){
#if SERVICE
			us.lastbreak = rand() % (NUM_BREAKTYPES + 1);
#endif
			gs.go = 1;
			{int place;
			if(0 < (place = PutScore(gs.points)+1)){
				static char buf[64];
				us.mdelay = 5000;
				sprintf(buf, "HIGH SCORE %d%s place!!", place, place == 1 ? "st" : place == 2 ? "nd" : place == 3 ? "rd" : "th");
				us.message = buf;
			}}

			/* do a firework */
			if(g_pinit->clearall) for(j = 0; j < p->cur; j++){
				p->l[j].life = rand() % (MAX_DELAY * 5);
			}
		}
		else{

		/* if every block gone calm, check and spawn new block. */
		block_t *pb = &p->l[p->cur-1];

		/* game over */
//		if(p->l[p->cur-1].t < MAX_BLOCK_HEIGHT){
//			gs.go = 1;
/*			for(i = 0; i < p->cur; i++){
				p->l[i].life = rand() % (MAX_DELAY * 5);
			}*/
//			return;
//		}

		if(p->cur && INNERBOX_OFFSET * 2 < pb->r - pb->l && INNERBOX_OFFSET * 2 < pb->b - pb->t)
			AddTefbox(g_ptefl, p->l[p->cur-1].l + INNERBOX_OFFSET, p->l[p->cur-1].t + INNERBOX_OFFSET,
				p->l[p->cur-1].r - INNERBOX_OFFSET, p->l[p->cur-1].b - INNERBOX_OFFSET,
				WG_WHITE, WG_BLACK, 0.5, TEF_BORDER);

		spawnNextBlock();
		}
	}
}

static void DrawBlock(WINGRAPH *pwg, bl_t *p, unsigned t){
	unsigned i, j;
	double d;
	int spaceb, spacet, msb = XSIZ, mst = XSIZ;

	d = ((16 * t) % 512 < 256 ? (16 * t) % 256 : 255 - ((16 * t) % 256)) / 256.0;

	/* divided in 2 paths to draw background stuff. */
	for(i = 0; i < p->cur; i++){
		if(p->l[i].life != UINT_MAX) continue; /* is to be destroyed */

#if 1
		/* check horizontal spaces */
		spaceb = spacet = XSIZ;
		for(j = 0; j < p->cur; j++){
			if(i == j || p->l[j].life != UINT_MAX) continue;
			/* bottom line */
			if(p->l[j].t < p->l[i].b && p->l[i].b < p->l[j].b)
				spaceb -= p->l[j].r - p->l[j].l;
			/* top line */
			if(p->l[j].t < p->l[i].t && p->l[i].t < p->l[j].b)
				spacet -= p->l[j].r - p->l[j].l;
		}

		if(spaceb < msb)
			msb = spaceb;
		if(spacet < mst)
			mst = spacet;

		if(spaceb < ALT_SPACE){
			LineWin(pwg,
			 0, p->l[i].b,
			 XSIZ, p->l[i].b,
			 RGB((unsigned char)(256 * (ALT_SPACE - spaceb) / (ALT_SPACE - MIN_SPACE) * d), 0, 0));
//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, RGB((unsigned char)(256 * spaceb / (XSIZ * 0.7)), 0, 0));
		}
		if(spacet < ALT_SPACE){
			LineWin(pwg,
			 0, p->l[i].t,
			 XSIZ, p->l[i].t,
			 RGB((unsigned char)(256 * (ALT_SPACE - spacet) / (ALT_SPACE - MIN_SPACE) * d), 0, 0));
//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, RGB((unsigned char)(256 * spaceb / (XSIZ * 0.7)), 0, 0));
		}
#endif
	}

	/* guidance lines */
	if(p->cur != 0 && !gs.go){
		block_t *pb;
		pb = &p->l[p->cur-1];
		switch(g_pinit->GuidanceLevel){
			case 0: break;
			case 1:
				LineWin(pwg,
					pb->l, pb->b,
					pb->l, YSIZ,
					RGB(0, 64, 0));
				LineWin(pwg,
					pb->r-1, pb->b,
					pb->r-1, YSIZ,
					RGB(0, 64, 0));
				break;
			case 2:
			case 3:
//			case 4:
			{
				block_t b;
				b.l = pb->l;
				b.t = pb->t;
				b.r = pb->r;
				b.b = pb->b;
				SlipDown(p->l, p->cur, &b, p->cur, YSIZ);
				if(g_pinit->GuidanceLevel == 3){
					LineWin(pwg,
						pb->l, pb->b,
						b.l, b.t,
						RGB(0, 64, 0));
					LineWin(pwg,
						pb->r-1, pb->b,
						b.r-1, b.t,
						RGB(0, 64, 0));
				}
#if 0
				else if(g_pinit->GuidanceLevel == 4){
					COLORREF col;
					double space;
					spaceb = spacet = XSIZ;
					for(j = 0; j < p->cur; j++){
						if(p->l[j].life != UINT_MAX) continue;
						/* bottom line */
						if(p->l[j].t < b.b && b.b < p->l[j].b)
							spaceb -= p->l[j].r - p->l[j].l;
						/* top line */
						if(p->l[j].t < b.t && b.t < p->l[j].b)
							spacet -= p->l[j].r - p->l[j].l;
					}

					if(spaceb < ALT_SPACE){
						LineWin(pwg,
						0, b.b,
						XSIZ, b.b,
						RGB(0, (unsigned char)(256 * (ALT_SPACE - spaceb) / (ALT_SPACE - MIN_SPACE) * (1-d)), 0));
			//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, RGB((unsigned char)(256 * spaceb / (XSIZ * 0.7)), 0, 0));
					}
					if(spacet < ALT_SPACE){
						LineWin(pwg,
						0, b.t,
						XSIZ, b.t,
						RGB(0, (unsigned char)(256 * (ALT_SPACE - spacet) / (ALT_SPACE - MIN_SPACE) * (1-d)), 0));
			//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, RGB((unsigned char)(256 * spaceb / (XSIZ * 0.7)), 0, 0));
					}
					space = MIN(spaceb, spacet);
					if((ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE) > 1)
						RectangleWin(pwg, b.l, b.t, b.r, b.b, RGB(255,255,255));
					else
						RectangleWin(pwg, b.l, b.t, b.r, b.b, RGB((unsigned char)(256 * (ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE)), 128 + (unsigned char)(128 * (ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE)),(unsigned char)(256 * (ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE))));
					if(INNERBOX_OFFSET * 2 < pb->r - pb->l && INNERBOX_OFFSET * 2 < pb->b - pb->t){
						if((ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE) > 1)
							FillRectangleWin(&wg, b.l + INNERBOX_OFFSET, b.t + INNERBOX_OFFSET, b.r - INNERBOX_OFFSET, b.b - INNERBOX_OFFSET,
							 RGB(255,0,0));
						else
							RectangleWin(&wg, b.l + INNERBOX_OFFSET, b.t + INNERBOX_OFFSET, b.r - INNERBOX_OFFSET, b.b - INNERBOX_OFFSET,
							 RGB(0,64 + (unsigned char)(64 * (ALT_SPACE - space) / (ALT_SPACE - MIN_SPACE)),0));
					}
				}
#endif
				else{ /* predicted rect */
					RectangleWin(pwg, b.l, b.t, b.r, b.b, RGB(0,64,0));
					if(INNERBOX_OFFSET * 2 < pb->r - pb->l && INNERBOX_OFFSET * 2 < pb->b - pb->t)
						RectangleWin(&wg, b.l + INNERBOX_OFFSET, b.t + INNERBOX_OFFSET, b.r - INNERBOX_OFFSET, b.b - INNERBOX_OFFSET,
						RGB(0,32,0));
				}
				break;
			}
		}
		/* Inner box */
		{
		int dx, dy;
		dx = pb->r - pb->l, dy = pb->b - pb->t;
		if(INNERBOX_OFFSET * 2 < dx && INNERBOX_OFFSET * 2 < dy){
			RectangleWin(&wg, pb->l + INNERBOX_OFFSET, pb->t + INNERBOX_OFFSET, pb->r - INNERBOX_OFFSET, pb->b - INNERBOX_OFFSET,
			 RGB((t % 512 < 256 ? t : 255 - (t % 256)) / 2, (255) / 2, (255 - (t % 512 < 256 ? t : 255 - (t % 256))) / 2));
//			 WG_WHITE);
		}
		}
		/* draw traces */
		if(!gs.pause && gs.dpc && cnl_darkpurple[0].t){
			block_t *pb = &p->l[p->cur-1];
	//		AddTefbox(g_ptefl, pb->l-1, pb->t-1, pb->r+1, pb->b+1, RGB(32,0,64), WG_BLACK, 0.5, TEF_BORDER);
			AddTefboxCustom(g_ptefl, pb->l-1, pb->t-1, pb->r+1, pb->b+1, 0, NULL, &cs_darkpurple);
		}

	}

	/* block itself is drawn at last so is always foreground. */
	for(i = 0; i < p->cur; i++){
		int r = t % 512 < 256 ? t : 255 - (t % 256);
		int g = 255 * i / p->cur;
		int b = 255 - (t % 512 < 256 ? t : 255 - (t % 256));
//		if(i != p->cur-1)
//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, WG_BLACK);
//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, WG_BLACK);
//			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, WG_BLACK);

		if(g_pinit->BlockDrawMethod == 1)
			FillRectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b, RGB(r, 255 - g, b));
		else if(g_pinit->BlockDrawMethod == 2)
			AddConstBox(wg.pVWBuf, COLOR32RGBA(r, 255 - g, b, 127), p->l[i].l, p->l[i].t, p->l[i].r - p->l[i].l, p->l[i].b - p->l[i].t, wg.nWidth * sizeof(COLORREF));
		RectangleWin(&wg, p->l[i].l, p->l[i].t, p->l[i].r, p->l[i].b,
		 p->l[i].life == UINT_MAX || (t & (1<<1)) ?
		 RGB(t % 512 < 256 ? t : 255 - (t % 256), 255 * i / p->cur, 255 - (t % 512 < 256 ? t : 255 - (t % 256))) :
		 WG_WHITE);
	}

}

static void Debris(int x, int y, unsigned c){
	while(c--){
		AddTeline(g_ptell,
			(double)x, (double)y,
			8 * drand() - 4, 8 * drand() - 4,
			5,
			M_PI * drand(),
			M_PI / 3 * (drand() - 0.5),
			1.,
			WG_BLACK,
			TEL_REFLECT,
			rand() % 80 + 50);
	}
}

static void Stars(int x, int y, unsigned c, COLORREF acol){
	unsigned i;
	for(i = 0; i < c; i++){
		double vx, vy, len, ang, omg, life;
		COLORREF col;
		vx = 16 * drand() - 8;
		vy = 16 * drand() - 8;
		len = rand() % 3 + 4;
		ang = M_PI * drand();
		omg = M_PI * (drand() - 0.5);
		life = rand() % 30 + 20;
/*		if(acol == WG_BLACK)
			col = RGB(rand() % 128 + 128, rand() % 128 + 128, rand() % 128 + 128);
*/
		AddTeline(g_ptell,
			(double)x, (double)y,
			vx, vy,
			len,
			ang,
			omg,
			1.0,
			col,
			TEL_SHRINK | TEL_STAR,
			life);
/*		AddTeline(g_ptell,
			(double)x, (double)y,
			vx, vy,
			len,
			ang + M_PI / 2,
			omg,
			1.0,
			col,
			TEL_SHRINK,
			life);
*/	}
}

static void Explosion(int x, int y, unsigned c){
	unsigned i;
	for(i = 0; i < c; i++){
		double vx, vy, phase;
		vx = 32 * drand() - 16;
		vy = 32 * drand() - 16;
		phase = drand() * 2 * M_PI;

		AddTeline(g_ptell,
			(double)x, (double)y,
			i % 3 ? vx : 16 * cos(phase), i % 3 ? vy : 16 * sin(phase),
		/*	sqrt(vx * vx + vy * vy) / 2,*/ i % 3 ? sqrt(vx * vx + vy * vy) / 2 : 8,
		/*	atan2(vy, vx),*/ i % 3 ? atan2(vy, vx) : phase,
			0.0,
			2.0,
			i % 2 ? WG_WHITE : WG_BLACK,
			TEL_HEADFORWARD | TEL_REFLECT | TEL_SHRINK,
			rand() % 10 + 20);
	}
}

static void BlockBreak(const block_t *b, int type){
	static unsigned calls = 0;
	int i, area;

	area = (b->r - b->l) * (b->b - b->t);

/*	AddTefbox(g_ptefl, b->l, b->t, b->r, b->b, calls % 5 == 0 ? WG_BLACK : RGB(128, 0, 0), WG_BLACK, 1.5);
	AddTefbox(g_ptefl, b->l, b->t, b->r, b->b, RGB(128, 0, 0), WG_WHITE, 0.15);*/
	AddTefboxCustom(g_ptefl, b->l, b->t, b->r, b->b, 0, calls % 5 == 0 ? &cs_randomflash : &cs_redflash, NULL);
/*	Debris((b->r + b->l) / 2, (b->b + b->t) / 2, MIN(area / 100, 20));*/

	AddTeline(g_ptell, b->l, b->t, 0, 0, 20, 0, 0, 0, (COLORREF)&cs2_redfade, TEL_GRADCIRCLE2, 2);

	if(type == 1){ /* standard */
		for(i = 0; i < MIN(area /*/ 100 */* g_pinit->fragm_factor, g_pinit->fragm_c_max); i++){
			double vx, vy;
	//		DWORD flags = 0;
			vx = 700 * (drand() - .5);
			vy = 700 * (drand() - .7);
	//		flags |= i&4 ? TEL_STAR | TEL_SHRINK | TEL_REFLECT : TEL_HEADFORWARD | TEL_REFLECT | TEL_SHRINK;
	//		flags |= TEL_FILLCIRCLE | TEL_NOLINE;
			if(drand() < g_pinit->BlockFragmentTrailRate){
			AddTefpol(g_ptepl, 
				rand() % (b->r - b->l) + b->l, rand() % (b->b - b->t) + b->t,
				vx, vy, 2.0, &csl_allstar[rand() % ARRAYSIZE(csl_allstar)],
	//			TEP_REFLECT | (i % 6 == 1 ? TEL_POINT : i % 6 == 2 ? TEL_CIRCLE : i % 6 == 3 ? TEL_FILLCIRCLE : i % 6 == 4 ? TEL_RECTANGLE : i % 6 == 5 ? TEL_PENTAGRAPH : 0),
				TEP_REFLECT | ((i % 8) << 10),
				drand() * (g_pinit->fragm_life_max - g_pinit->fragm_life_min) + g_pinit->fragm_life_min);
			}
			else
			AddTeline(g_ptell,
				rand() % (b->r - b->l) + b->l, rand() % (b->b - b->t) + b->t,
	//			(b->r + b->l) / 2, (b->b + b->t) / 2,
				vx, vy,
				/*flags & (TEL_CIRCLE|TEL_FILLCIRCLE) == TEL_POINT ? 0.0 :*/ i & 4 ? drand() * 3. + 3. : sqrt(vx * vx + vy * vy) / 50 /*: 0.03*/,
				/*flags & (TEL_CIRCLE|TEL_FILLCIRCLE) == TEL_POINT ? 0.0 :*/ i & 4 ? drand() * M_PI : 0.0, /* meaningless */
				/*flags & (TEL_CIRCLE|TEL_FILLCIRCLE) == TEL_POINT ? 0.0 :*/ i & 4 ? drand() * M_PI * 3 : 0.0,
				2.0,
				i & 2 ? WG_WHITE : WG_BLACK,
				i & 4 ? TEL_STAR | TEL_SHRINK | TEL_REFLECT : TEL_HEADFORWARD | TEL_REFLECT | TEL_SHRINK,
	//			flags,
				drand() * (g_pinit->fragm_life_max - g_pinit->fragm_life_min) + g_pinit->fragm_life_min);
		}
	}
	else if(type == 2){ /* fiery */
		switch(rand() % 5){
		case 0: case 1:
			for(i = 0; i < 100; i++){
			double ang = drand() * 2 * M_PI;
			double mag = sqrt(drand()) * 900;
			AddTeline(g_ptell,
				(b->r + b->l) / 2, (b->b + b->t) / 2,
				cos(ang) * mag, sin(ang) * mag,
				.01,
				0.0, /* meaningless */
				0.0,
				0.0,
				RGB(192, 192, 192),
				TEL_HEADFORWARD | TEL_SHRINK | TEL_VELOLEN,
				.5);
			}
			break;
		case 2: case 3:{
			DWORD flags = (rand() & 2 ? TEP_WIND : 0) | TEP_FINE | TEP_ADDBLEND /*| TEP_THICK*/;
			for(i = 0; i < 50; i++){
				double ang = drand() * 2 * M_PI;
				double mag = sqrt(drand()) * 900;
				AddTefpol(g_ptepl,
					(b->r + b->l) / 2, (b->b + b->t) / 2,
					cos(ang) * mag, sin(ang) * mag,
					2.0, &cs_fireburn, flags, .5);
			}
			break;}
		case 4:{
			int j;
			double mag = sqrt(drand()) * 150;
			for(j = 0; j < ARRAYSIZE(csl_fireworks); j++, mag += 70) for(i = 0; i < 50; i++){
				double ang = drand() * 2 * M_PI;
				AddTefpol(g_ptepl,
					(b->r + b->l) / 2, (b->b + b->t) / 2,
					cos(ang) * mag, sin(ang) * mag,
					2.0, &csl_fireworks[j], TEP_FINE | TEP_ADDBLEND /*| TEP_THICK*/, drand() * 1. + 0.5);
			}
#if LANG==JP
			AddTeline(g_ptell,
				(b->r + b->l) / 2 - 7, (b->b + b->t) / 2 - 7,
				0, 14, 40, 0.0, 0.0, .25, WG_BLACK,
				TEL_HEXAGRAPH | TEL_NOLINE, 1.2);
#endif
			break;}
		default: assert(0);
		}
	}
	else if(type == 3){/* electric sarge */
		for(i = 0; i < 3; i++)
			AddTefpol(g_ptepl, 
				rand() % (b->r - b->l) + b->l, rand() % (b->b - b->t) + b->t,
				1000 * (drand() - .5), 1000 * (drand() - .5), 0.0, &csl_electric[rand() % ARRAYSIZE(csl_electric)],
				TEP_DRUNK | TEP_SHAKE | TEP_REFLECT | TEP_WIND | (i % 2 ? TEP_THICK : 0) | TEP_HEAD /*| (i == 2 ? TEP_SUBBLEND : 0)*/ /*| TEP_FINE | TEP_ROUGH*/,
				drand() * 2.0 + 1.);
	}
	else if(type == 4){ /* Bubble pop */
		for(i = 0; i < MIN(area /*/ 100 */* g_pinit->fragm_factor, g_pinit->fragm_c_max); i++){
			double vx, vy;
			vx = 500 * (drand() - .5);
			vy = 500 * (drand() - .7);
			AddTeline(g_ptell,
				rand() % (b->r - b->l) + b->l, rand() % (b->b - b->t) + b->t,
				vx, vy,
#if LANG==EN
				3.0,
#else
				/*i & 4 ? 3.0 : 15.*/ 3.,
#endif
				0.0, 0.0, 2.0,
/*				i & 2 ? WG_WHITE : WG_BLACK*/ &cs_whitechip,
			/*	(i & 4 ? TEL_CIRCLE : TEL_FILLCIRCLE) |*/ TEL_SHRINK | TEL_REFLECT | TEL_NOLINE | TEL_GRADCIRCLE,
				drand() * (g_pinit->fragm_life_max - g_pinit->fragm_life_min) + g_pinit->fragm_life_min);
		}
	}

	/* lines spawned on top / bottom border */
	for(i = 0; i < (b->r - b->l) / g_pinit->BlockDebriLength; i++){
		AddTeline(g_ptell,
			b->l + i * g_pinit->BlockDebriLength + g_pinit->BlockDebriLength / 2, b->t,
			100 * (drand() - .5), 100 * (drand() - .5),
			g_pinit->BlockDebriLength / 2,
			0.0,
			M_PI * 6 * (drand() - 0.5),
			1.0,
			WG_BLACK,
			TEL_REFLECT,
			drand() * g_pinit->BlockDebriLifeMin + (g_pinit->BlockDebriLifeMax - g_pinit->BlockDebriLifeMin));
		AddTeline(g_ptell,
			b->l + i * g_pinit->BlockDebriLength + g_pinit->BlockDebriLength / 2, b->b,
			100 * (drand() - .5), 100 * (drand() - .5),
			g_pinit->BlockDebriLength / 2,
			0.0,
			M_PI * 6 * (drand() - 0.5),
			1.0,
			WG_BLACK,
			TEL_REFLECT,
			drand() * g_pinit->BlockDebriLifeMin + (g_pinit->BlockDebriLifeMax - g_pinit->BlockDebriLifeMin));
	}
	/* lines spawned on left / right border */
	for(i = 0; i < (b->b - b->t) / g_pinit->BlockDebriLength; i++){
		AddTeline(g_ptell,
			b->l, b->t + i * g_pinit->BlockDebriLength + g_pinit->BlockDebriLength / 2,
			100 * (drand() - .5), 100 * (drand() - .5),
			g_pinit->BlockDebriLength / 2,
			M_PI / 2,
			M_PI * 6 * (drand() - 0.5),
			1.0,
			WG_BLACK,
			TEL_REFLECT,
			drand() * 1.0 + 1.5);
		AddTeline(g_ptell,
			b->r, b->t + i * g_pinit->BlockDebriLength + g_pinit->BlockDebriLength / 2,
			100 * (drand() - .5), 100 * (drand() - .5),
			g_pinit->BlockDebriLength / 2,
			M_PI / 2,
			M_PI * 6 * (drand() - 0.5),
			1.0,
			WG_BLACK,
			TEL_REFLECT,
			drand() * 1.0 + 1.5);
	}
/*
	for(i = 0; i < 5; i++){
		AddTeline(g_ptell,
			(b->r + b->l) / 2, (b->b + b->t) / 2,
			0.0, 0.0,
			drand() * 20 + 10,
			drand() * 2 * M_PI,
			(drand() - 0.5) * M_PI / 8,
			0.0,
			WG_WHITE,
			TEL_STAR,
			rand() % 25 + 30);
	}
	*/

	calls++;
}

#ifndef NDEBUG
static double animtime = 0.;
#endif

static void Animate(void){
#ifndef NDEBUG
	timemeas_t tm;
#endif

	AnimTefbox(g_ptefl, gs.ft);
	AnimTefpol(g_ptepl, gs.ft);
	AnimTeline(g_ptell, gs.ft);
	/* drawing time and physical time may differ in machines. */
	gs.dpc = 0;
	while(gs.last + FRAMETIME <= gs.t){
		AnimBlock(&bl, gs.c);
		gs.c++;
		gs.last += FRAMETIME;
		gs.dpc++;
	}

#ifndef NDEBUG
	animtime = TimeMeasLap(&tm);
#endif
}

struct DrawFadeData{
	int lineskip;
	int linenum;
	int random[2];
	struct random_sequence rs;
};

static void DrawFade(COLORREF *dst, const COLORREF *src, unsigned long t, struct DrawFadeData *dfd){
	int rt = 0, gt = 0, bt = 0;
#define ADDCR(a,b) RGB(MIN(255,(GetRValue(a)+GetRValue(b))/2),MIN(255,(GetGValue(a)+GetGValue(b))/2),MIN(255,(GetBValue(a)+GetBValue(b))/2))
#define ADDCR2(a,b,c) RGB(MIN(255,(2*GetRValue(a)+GetRValue(b)+GetRValue(c))/4),MIN(255,(2*GetGValue(a)+GetGValue(b)+GetGValue(c))/4),MIN(255,(2*GetBValue(a)+GetBValue(b)+GetBValue(c))/4))
#define ADDCR3(a,b,c,d) RGB((3*GetRValue(a)+GetRValue(b)+GetRValue(c)+GetRValue(d))/6,(3*GetGValue(a)+GetGValue(b)+GetGValue(c)+GetGValue(d))/6,(3*GetBValue(a)+GetBValue(b)+GetBValue(c)+GetBValue(d))/6)
#define TC4(a) ((a)<0?~(-(a)-1):(a))
#define DTC4(a) ((a)&0x8?~(a)+1:(a))
#if TRANSBUFFER == 1
#	define TTT
#else
#	define TTT TC4
#endif
#define TFACTOR 2
#define ADDTR2(a,b,c) ((2*TTT((a)&0xf)+TTT((b)&0xf)+TTT((c)&0xf))/4|((2*((a)>>4)+((b)>>4)+((c)>>4))/4)<<4)
#define ADDTR3(a,b,c,d) ((3*TTT((a)&0xf)+TTT((b)&0xf)+TTT((c)&0xf)+TTT((d)&0xf)+TFACTOR)/6|((3*((a)>>4)+((b)>>4)+((c)>>4)+((d)>>4)+TFACTOR)/6)<<4)
	/*if(gs.fc % 2)*/{
		int i, j, k;
		for(i = dfd->linenum; i < YSIZ; i += dfd->lineskip){
//			dst[i*XSIZ] = ADDCR2(src[i*XSIZ], src[(i+1)*XSIZ], src[(i+1)*XSIZ+1]);
			if(bMMX){
				FadeAddMMXT(&dst[i*XSIZ+2], &src[(i+1)*XSIZ+2], XSIZ-4, &dfd->random);
//				dfd->random[0] = rseq(&dfd->rs);
#if TURBULSCREEN && 0
				for(j = 1; j < XSIZ-1; j += 2){
					const COLORREF *ps0, *ps = ps0 = &src[(i+1+YSIZ)%YSIZ*XSIZ+j];
					int r;
					r = rseq(&dfd->rs);
					if(j != 1 && j != XSIZ-2) switch(r % 8){
						case 0: ps++; break;
						case 1: ps--; break;
					}
					if(ps != ps0)
						FadeAddMMX2(&dst[i*XSIZ+j], &src[(i+1)*XSIZ+j]);
				}
#endif
#if BURNSCREEN && 0
				for(j = 1; j < XSIZ-1; j++){
					COLORREF *pc = &dst[i*XSIZ+j];
					const COLORREF *ps = &src[i*XSIZ+j];
					int r;
					r = rseq(&dfd->rs);
					if((r / 4) % 4096 == 0) for(k = 0; k < 24; k += 8)
					if(64 < (0xff & (((*pc)>>k))))
						*pc |= (0xff << k);
				}
#endif
			}
			else{
				for(j = 1; j < XSIZ-1; j++){
					COLORREF *pc = &dst[i*XSIZ+j];
					const COLORREF *ps = &src[i*XSIZ+j];
					int r;
					r = rseq(&dfd->rs);
	#if TURBULSCREEN
					if(j != 1 && j != XSIZ-2) switch(r % 4){
	#	if TURBULSCREEN == 1
						case 0: *pc = ADDCR2(*pc, ps[XSIZ-1], ps[XSIZ]); goto turbulexit;
						case 1: *pc = ADDCR2(*pc, ps[XSIZ], ps[XSIZ+1]); goto turbulexit;
	#	else
						case 0: *pc = ADDCR3(*pc, ps[XSIZ-2], ps[XSIZ-1], ps[XSIZ]); goto turbulexit;
						case 1: *pc = ADDCR3(*pc, ps[XSIZ], ps[XSIZ+1], ps[XSIZ+2]); goto turbulexit;
	#	endif
					}
	#endif
					*pc = ADDCR3(*pc, ps[XSIZ-1], ps[XSIZ], ps[XSIZ+1]);
turbulexit:;
	#if BURNSCREEN
					if((r / 4) % 4096 == 0) for(k = 0; k < 24; k += 8)
					if(64 < (0xff & (((*pc)>>k))))
						*pc |= (0xff << k);
	#endif
				}
			}
//			dst[i*XSIZ+XSIZ-1] = ADDCR2(src[i*XSIZ+XSIZ-1], src[(i+1)*XSIZ+XSIZ-2], src[(i+1)*XSIZ+XSIZ-1]);
		}
/*		dst[(YSIZ-1)*XSIZ] = ADDCR2(src[(YSIZ-1)*XSIZ], src[0], src[1]);
		for(j = 1; j < XSIZ-1; j++){
			COLORREF *pc = &dst[(YSIZ-1)*XSIZ+j];
			*pc = ADDCR3(*pc, src[j-1], src[j], src[j+1]);
		}
		dst[(YSIZ-1)*XSIZ+XSIZ-1] = ADDCR2(src[(YSIZ-1)*XSIZ+XSIZ-1], src[XSIZ-2], src[XSIZ-1]);*/
	}
}

static DrawProc(unsigned long t, struct DrawFadeData *dfd){
	/* ptb -> pbb addition first */
#if TRANSBUFFER
#	if 1
	if(g_pinit->EnableTransBuffer){/* transbuffer add */
		int i, j;
		for(i = dfd->linenum; i < YSIZ; i += dfd->lineskip) for(j = 1; j < XSIZ-1; j++) if(ptb[i*XSIZ+j]){
			unsigned k;
			k = (GetRValue(ptb[i*XSIZ+j]) + rseq(&dfd->rs) % 128) / 128;
#		if TRANSBUFFER > 1
			if(7 < k) k = 7;
			if(rseq(&dfd->rs) % 2) k = ~(k-1);
#		endif
			pbb[i*XSIZ+j] += k /*MIN(7, (GetRValue(ptb[i*XSIZ+j])*//*+GetGValue(ptb[i*XSIZ+j])+GetBValue(ptb[i*XSIZ+j])*//*) / 128))*/ << 4 |
				MIN(16, GetGValue(ptb[i*XSIZ+j]) / 128);
		}
	}
#	endif
#endif

	DrawFade(ptb, peb, t, dfd);
}

struct DrawThreadData{
	HANDLE hrun;
	HANDLE hend;
	unsigned long t;
};

static DWORD WINAPI DrawThread(LPVOID pv){
	volatile struct DrawThreadData *dtd = (struct DrawThreadData*)pv;
	struct DrawFadeData dfd; /* each fade data resides in thread's stack. */
	init_rseq(&dfd.rs, (unsigned)dtd);
	dfd.lineskip = 2;
	dfd.linenum = 1;
	dfd.random[0] = 1;
	while(1){
		WaitForSingleObject(dtd->hrun, INFINITE);
		ResetEvent(dtd->hrun);
		DrawProc(dtd->t, &dfd);
		SetEvent(dtd->hend);
	}
}

static void DrawMain(unsigned long t){
#if MULTH
	static init = 0;
	static HANDLE hth = NULL;
	static struct DrawThreadData dtd;
	static struct DrawFadeData dfd;
#endif
#ifndef NDEBUG
	int random;
	char strbuf[64];
#endif
	if(!peb)
		allocBuffers();

#if MULTH
	dfd.lineskip = g_pinit->EnableMultiThread ? 2 : 1;
	if(dfd.lineskip){
		if(!init){
			SYSTEM_INFO si;
			init = 1;
			GetSystemInfo(&si);
			if(2 <= si.dwNumberOfProcessors){
				DWORD dw;
				dtd.hrun = CreateEvent(NULL, TRUE, FALSE, NULL);
				dtd.hend = CreateEvent(NULL, TRUE, FALSE, NULL);
				hth = CreateThread(NULL, 0, DrawThread, &dtd, 0, &dw);
				init_rseq(&dfd.rs, (unsigned)&dfd);
				dfd.lineskip = 2;
				dfd.linenum = 0;
			}
			else{
				dfd.lineskip = 1;
				dfd.linenum = 0;
				dfd.random[0] = 0;
			}
		}
		if(hth)
			dtd.t = t;
		else
			dfd.lineskip = 1;
	}
#endif

	/* MMX */
	if(bMMX){
		int i, j;
		for(i = 0; i < YSIZ; i++) for(j = 0; j < XSIZ; j++){
			COLORREF *pt = &ptb[i*XSIZ+j];
			COLORREF *pc = &peb[i*XSIZ+j];
			if(*pc)
//				*pc = ((unsigned long)*pc >> 3) & 0x1f1f1f1f;
				*pc = ((unsigned long)*pc >> 2) & 0x3f3f3f3f;
/*			if(*pt)
				*pt = ((unsigned long)*pt >> 1) & 0x7f7f7f7f;*/
		}
	}

#if MULTH
	if(dfd.lineskip == 2){
		if(hth)
			SetEvent(dtd.hrun);
		DrawProc(t, &dfd);
		if(hth){
			WaitForSingleObject(dtd.hend, 2000);
			ResetEvent(dtd.hend);
		}
	}
	else
#endif
	DrawProc(t, &dfd);
//	smoothing(ptb, peb, 1, XSIZ, YSIZ);
//	memcpy(peb, ptb, XSIZ * YSIZ * sizeof *peb);
#if TRANSBUFFER
	/*if(t % 512 < 256 ? t % ((t % 512) / 64 + 1) == 0 : t % (4 - (t % 256) / 64) == 0)*/
	if(g_pinit->EnableTransBuffer){
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
			}
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

//	GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);

	/* subtract constant from ptb */
	if(t % 2){
		unsigned t2 = t/2;
		int r = (t2 % 512 < 256 ? t2 : (255 - t2 % 256)) % 256 / 16;
		COLORREF c = RGB(r, (t2 % 0x300 < 0x180 ? t2 : (0x17f - t2 % 0x180)) % 0x180 / 0x18, 16 - r /* (255 - (t2 % 512 < 256 ? t2 : 255 - (t2 % 256))) / 16*/);
		subconstant(ptb, c, XSIZ * YSIZ * sizeof *peb, bMMX);
#ifndef NDEBUG
		{
		int i;
		const int thickness = 3;
		for(i = 0; i < 3; i++){
		FillRectangleWin(&wg, 10+2*(0xff&(c>>(i*8))), i*thickness+YRES - 2 * g_fonth, 10+2*16, (i+1)*thickness+YRES - 2 * g_fonth, 0x80<<(i*8));
		FillRectangleWin(&wg, 10, i*thickness+YRES - 2 * g_fonth, 10+2*(0xff&(c>>(i*8))), (i+1)*thickness+YRES - 2 * g_fonth, 0x7f7f7f|(0xff<<(i*8)));
		}}
		sprintf(strbuf, "(%i,%i,%i)", r, GetGValue(c), GetBValue(c));
		DrawFontWin(&wg, strbuf, 10, YRES - 3 * g_fonth, WG_WHITE, NULL);
	}
/*	sprintf(strbuf, "(%8.5g,%8.5g,%8.5g)", (double)rt / ((XSIZ-1)*YSIZ), (double)bt / ((XSIZ-1)*YSIZ), (double)gt / ((XSIZ-1)*YSIZ));
//	addblend(peb, ptb, 15, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
	DrawFontWin(&wg, strbuf, 10, YRES - g_fonth, WG_WHITE, NULL);*/
#else
	}
#endif
/*	GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);*/
//	addblend(ptb, peb, 100, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
	pastepat(wg.pVWBuf, ptb, 0, 0, XSIZ, YSIZ, XRES);

	/* blend background (be careful on mutual access) */
	if(pimg && g_pinit->BackgroundBlendMethod){
		EnterCriticalSection(&csimg);
		(g_pinit->BackgroundBlendMethod == 1 ? alphablend2 : addblend2)(wg.pVWBuf, pimg, g_pinit->BackgroundBrightness, XSIZ, YSIZ, XRES, bMMX);
		LeaveCriticalSection(&csimg);
	}

#if TRANSBUFFER
	if(g_pinit->EnableTransBuffer){/* transcoloring */
		int i, j;
		COLORREF *pc;
		for(i = 0; i < YSIZ; i++){
			for(j = 1; j < XSIZ-1; j++){
				byte pb;
/*				pbb[i*XSIZ+j] += MIN(16, (GetRValue(ptb[i*XSIZ+j])) / 128) << 4 |
					MIN(16, GetGValue(ptb[i*XSIZ+j]) / 128);*/
				pb = pbb[i*XSIZ+j];
				if(pb){
					int k;
#if TRANSBUFFER == 1
					k = i*XRES+j + (pb & 0xf) + (pb >> 4) * XRES;
#else
					k = i*XRES+j + (pb & 0x8 ? ~(pb & 0x7)+1 : (pb & 0x7)) + (pb >> 4) * XRES;
#endif
					if(0 < k && k < XRES*YSIZ)
						wg.pVWBuf[i*XRES+j] = wg.pVWBuf[k];
				}
			}
		}
//		pastepat(wg.pVWBuf, ptb, 0, 0, XSIZ, YSIZ, XRES);
	}
#endif

	LineWin(&wg, 0, MAX_BLOCK_HEIGHT, XSIZ, MAX_BLOCK_HEIGHT, RGB(0, 0, 80));

	{
		LPCOLORREF tmp = peb;
		peb = ptb;
		us.toptb.bm.bmBits = ptb = tmp;
	}
}

static void Draw(unsigned t){
#ifndef NDEBUG
	char strbuf[64];
	timemeas_t tm;
	static double drawtime = 0.;
	TimeMeasStart(&tm);
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

	DrawTefbox(g_pinit->EnableFadeScreen ? &us.toptb : &us.to, g_ptefl);
	/* game over line */
#if MEMORYGRAPH
	{
	int x = 0, y;
	size_t total = 0;
#define sumsize(v) (total += (v), (v) / 4 / XSIZ)
#	if LANG==JP
	struct GryphCacheUsageRet r;
	GryphCacheUsage(&r);
	if(r.bytes){
		y = sumsize(r.bytes);
		LineWin(&wg, 0, y, XSIZ, y, RGB(0, 0, 255));
		x += DrawFontWin(&wg, "GC", x, y, RGB(0, 0, 255), NULL) * g_fontw;
	}
#	endif
	y = sumsize(g_tetimes.teline_m * g_tetimes.so_teline_t);
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 80, 0));
	x += DrawFontWin(&wg, "teline", x, y, RGB(80, 80, 0), NULL) * g_fontw;
	y = sumsize(g_tetimes.tefbox_m * g_tetimes.so_tefbox_t);
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 0, 0));
	x += DrawFontWin(&wg, "tefbox", x, y, RGB(80, 0, 0), NULL) * g_fontw;
	y = sumsize(g_tetimes.tefpol_m * g_tetimes.so_tefpol_t);
	LineWin(&wg, 0, y, XSIZ, y, RGB(0, 80, 80));
	x += DrawFontWin(&wg, "tefpol", x, y, RGB(0, 80, 80), NULL) * g_fontw;
	y = sumsize(g_pinit->PolygonVertexBufferSize * g_tetimes.so_tevert_t);
	LineWin(&wg, 0, y, XSIZ, y, RGB(0, 80, 0));
	x += DrawFontWin(&wg, "tevert", x, y, RGB(0, 80, 0), NULL) * g_fontw;
	y = total / 4 / XSIZ;
	LineWin(&wg, 0, y, XSIZ, y, RGB(80, 80, 80));
	x += DrawFontWin(&wg, "total", x, y, RGB(80, 80, 80), NULL);
#	if TRANSBUFFER
	if(g_pinit->EnableTransBuffer){
		y = (XSIZ-1) * YSIZ * sizeof *pbb / 4 / XSIZ;
		LineWin(&wg, 0, y, XSIZ, y, RGB(80, 0, 80));
		DrawFontWin(&wg, "transb", x, y, RGB(80, 0, 80), NULL);
	}
#	endif
	}
#endif
	if(!FADESCREEN){
		LineWin(&wg, 0, MAX_BLOCK_HEIGHT, XSIZ, MAX_BLOCK_HEIGHT, RGB(0, 0, 80));
	}
	DrawTefpol(g_pinit->EnableFadeScreen ? &us.toptb : &us.to, g_ptepl);
	DrawTeline(g_pinit->EnableFadeScreen ? &us.toptb : &us.to, g_ptell);

#if 1
	if(g_pinit->EnableFadeScreen)
		DrawMain(t);
#else
	if(FADESCREEN){
	int rt = 0, gt = 0, bt = 0;
	if(!peb)
		allocBuffers();
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
	/*if(t % 512 < 256 ? t % ((t % 512) / 64 + 1) == 0 : t % (4 - (t % 256) / 64) == 0)*/
	if(g_pinit->EnableTransBuffer){
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
			}
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
		}}
		sprintf(strbuf, "(%i,%i,%i)", r, GetGValue(c), GetBValue(c));
		DrawFontWin(&wg, strbuf, 10, YRES - 3 * g_fonth, WG_WHITE, NULL);
	}
	sprintf(strbuf, "(%8.5g,%8.5g,%8.5g)", (double)rt / ((XSIZ-1)*YSIZ), (double)bt / ((XSIZ-1)*YSIZ), (double)gt / ((XSIZ-1)*YSIZ));
	DrawFontWin(&wg, strbuf, 10, YRES - g_fonth, WG_WHITE, NULL);
#else
	}
#endif
/*	GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);*/
#if TRANSBUFFER
#	if 1
	if(g_pinit->EnableTransBuffer){/* transbuffer add */
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
	if(pimg && g_pinit->BackgroundBlendMethod){
		EnterCriticalSection(&csimg);
		(g_pinit->BackgroundBlendMethod == 1 ? alphablend : addblend)(ptb, pimg, g_pinit->BackgroundBrightness, (XSIZ-1) * YSIZ * sizeof *peb, bMMX);
		LeaveCriticalSection(&csimg);
	}
#if TRANSBUFFER
	if(g_pinit->EnableTransBuffer){/* transcoloring */
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
#if TRANSFUZZY
						if(0 < k && k < XSIZ*(YSIZ-1)-1) switch(g_pinit->TransBufferFuzzyness){
							case 0: ptb[i*XSIZ+j] = ptb[k]; break;
							case 1: ptb[i*XSIZ+j] = ADDCR2(ptb[k], ptb[k+1], ptb[k+XSIZ]); break;
							case 2: ptb[i*XSIZ+j] = ADDCR3(ptb[k], ptb[k+1], ptb[k+XSIZ], ptb[k+1+XSIZ]);
						}
#else
						if(0 < k && k < XSIZ*YSIZ)
							ptb[i*XSIZ+j] = ptb[k];
#endif
					}
				}
			}
		}
	}
#endif
	PutPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, ptb);
	LineWin(&wg, 0, MAX_BLOCK_HEIGHT, XSIZ, MAX_BLOCK_HEIGHT, RGB(0, 0, 80));
	}
#endif

	/* Blend background */
#if 0
	if(pimg && g_pinit->BackgroundBlendMethod && !FADESCREEN){
		if(peb || NULL != allocBuffers()){
			GetPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, peb);
			EnterCriticalSection(&csimg);
			(g_pinit->BackgroundBlendMethod == 1 ? alphablend : addblend)(peb, pimg, g_pinit->BackgroundBrightness, XSIZ * YSIZ * sizeof *peb, bMMX);
			LeaveCriticalSection(&csimg);
			PutPatternWin(&wg, 0, 0, XSIZ-1, YSIZ, peb);
		}
	}
#else
	if(pimg && g_pinit->BackgroundBlendMethod && !g_pinit->EnableFadeScreen){
		EnterCriticalSection(&csimg);
		(g_pinit->BackgroundBlendMethod == 1 ? alphablend2 : addblend2)(wg.pVWBuf, pimg, g_pinit->BackgroundBrightness, XSIZ, YSIZ, XRES, bMMX);
		LeaveCriticalSection(&csimg);
	}
#endif


	DrawBlock(&wg, &bl, gs.fc);

	/* game over message */
	if(gs.go && g_pinit->DrawGameOver){
#define PRESSPACE BYLANG("PRESS SPACE BAR TO RESTART", "スペースバーで開始")
		DrawFontWin(&wg, "GAME OVER", XSIZ / 2 - g_fontw * sizeof "GAME OVER" / 2 , YSIZ / 2 - g_fonth * 2, WG_WHITE,NULL);
		DrawFontWin(&wg, PRESSPACE, XSIZ / 2 - g_fontw * sizeof PRESSPACE / 2 , YSIZ / 2 - g_fonth, WG_WHITE,NULL);
	}
	/* pause message */
	if(gs.pause && g_pinit->DrawPauseString){
		DrawFontWin(&wg, "PAUSED", XSIZ / 2 - g_fontw * sizeof "PAUSED" / 2, YSIZ / 2, WG_WHITE,NULL);
	}
	/* custom message */
	if(0 < us.mdelay && us.message){
		int br = us.mdelay * 256 / 1000;
		br = MIN(br, 255);
		DrawFontWin(&wg, us.message, XSIZ / 2 - g_fontw * strlen(us.message) / 2 , YSIZ / 2 + 2 * g_fonth, RGB(br,br,br),NULL);
		us.mdelay -= gs.ft * 1000;
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
#if !defined NDEBUG
	/* print frame time */
/*	if(gs.hrt){*/
		sprintf(strbuf, "fps: %-8g (%-8g)", 1. / gs.ft, gs.avgft ? 1. / gs.fft : 1.);
/*	}
	else{
		sprintf(strbuf, "MSPF: %-8d", (unsigned)(clock() - foffset));
		foffset = clock();
	}*/
	{
	int y = 0;
	DrawFontWin(&wg, strbuf, XSIZ + 10, 0, WG_WHITE,NULL);
	sprintf(strbuf, "time: %8f", gs.t);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "frametime: %8g", gs.ft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "averageft: %8g", gs.avgft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y += g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "points: %u lvl: %g", gs.points, SPEED);
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
/*	sprintf(strbuf, "AnimTefbox: %8g", g_tetimes.animtefbox);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "AnimTeline: %8g", g_tetimes.animteline);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
*/	sprintf(strbuf, "DrawTefpol: %8g", g_tetimes.drawtefpol);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "DrawTefbox: %8g", g_tetimes.drawtefbox);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	sprintf(strbuf, "DrawTeline: %8g", g_tetimes.drawteline);
	DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
#if LANG==JP
	{
		struct GryphCacheUsageRet r;
		GryphCacheUsage(&r);
		if(r.count)
			sprintf(strbuf, "GC: %lu / %lu = %g", r.bytes, r.count, (double)r.bytes / r.count);
		else
			sprintf(strbuf, "GC: Unallocated");
		DrawFontWin(&wg, strbuf, XSIZ + 10, y -= g_fonth, WG_WHITE, NULL);
	}
#endif
	}
#else
	sprintf(strbuf, "Frame Rate: %-8g", 1. / gs.ft);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 0, WG_WHITE,NULL);
	sprintf(strbuf, "points: %u lvl: %g", gs.points, SPEED);
	DrawFontWin(&wg, strbuf, XSIZ + 10, g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "erased: %u", gs.area);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 2*g_fonth, WG_WHITE,NULL);
	sprintf(strbuf, "blocks: %-3u", bl.cur);
	DrawFontWin(&wg, strbuf, XSIZ + 10, 3*g_fonth, WG_WHITE,NULL);
#	ifndef NDEBUG
	{
		struct VertexUsageRet r;
		VertexUsage(&r);
		sprintf(strbuf, "Vertex: %lu/%lu", r.count, r.maxcount);
		DrawFontWin(&wg, strbuf, XSIZ + 10, 4*g_fonth, WG_WHITE,NULL);
	}
#	endif
#endif
	{
		int x = XSIZ + 10;
		if(g_pinit->BackgroundBlendMethod)
		FillRectangleWin(&wg, s_rBGBr.left, s_rBGBr.top, s_rBGBr.right, s_rBGBr.bottom,
			g_pinit->BackgroundBlendMethod == 1 ? RGB(0, 80, 0) : RGB(0, 40, 80));
		DrawFontWin(&wg, "BG", x, YRES - g_fonth, WG_WHITE,NULL);
		FillRectangleWin(&wg, s_xbgbr, YRES - g_fonth, s_xbgbr + 100, YRES - 0, RGB(40, 10, 5));
		sprintf(strbuf, "%i", g_pinit->BackgroundBrightness);
		DrawFontWin(&wg, strbuf, s_xbgbr + 4 + g_pinit->BackgroundBrightness, YRES - g_fonth, RGB(255, 127, 127),NULL);
		x = s_xbgbr + g_pinit->BackgroundBrightness;
		LineWin(&wg, x, YRES - g_fonth, x, YRES, RGB(255, 127, 127));
		FillRectangleWin(&wg, s_rPause.left, s_rPause.top, s_rPause.right, s_rPause.bottom, gs.pause ? RGB(80, 80, 40) : RGB(40, 40, 0));
		DrawFontWin(&wg, pszPause, s_rPause.left + g_fontw / 2, s_rPause.top, gs.pause ? RGB(255, 255, 255) : RGB(255, 255, 0),NULL);
		FillRectangleWin(&wg, s_rSet.left, s_rSet.top, s_rSet.right, s_rSet.bottom, RGB(5, 10, 40));
		DrawFontWin(&wg, pszSettings, s_rSet.left + g_fontw / 2, s_rSet.top, wgs.hWnd ? RGB(255, 255, 255) : RGB(0, 255, 255),NULL);
		FillRectangleWin(&wg, s_rScore.left, s_rScore.top, s_rScore.right, s_rScore.bottom, RGB(40, 10, 40));
		DrawFontWin(&wg, pszScore, s_rScore.left + g_fontw / 2, s_rScore.top, IsScoreWindowActive() ? RGB(255, 255, 255) : RGB(255, 0, 255),NULL);
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
	drawtime = TimeMeasLap(&tm);
#endif
}

static BOOL MsgProc(LPWG_APPPROC p){
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
			if(!us.setedit && RECTHIT(s_rSet, x, y)){
				if(!wgs.hWnd){
					wgs.dwUserParam = (DWORD)&us;
					CreateGraphicsWindow(&wgs);
					{
					LONG_PTR lp;
					lp = GetWindowLongPtr(wgs.hWnd, GWL_STYLE);
					if(lp)
						SetWindowLongPtr(wgs.hWnd, GWL_STYLE, (lp &~(WS_MAXIMIZEBOX|WS_HSCROLL|WS_VSCROLL)) /*|WS_MINIMIZEBOX|WS_MAXIMIZEBOX*/|WS_SYSMENU);
					}
					EnableThickFrame(&wgs, FALSE);
					wgs.bUserUpdate = TRUE;
					SetDraw(&wgs, &us);
				}
				else if(GetWindowLong(wgs.hWnd, GWL_STYLE) & WS_VISIBLE){
					goto destroy_setting_window;
				}
				else
					ShowWindow(wgs.hWnd, SW_SHOW);
			}
			else if(RECTHIT(s_rExit, x, y)){
				g_close = 1;
			}
			else if(RECTHIT(s_rPause, x, y)){
				us.pbutton = 1;
			}
			else if(RECTHIT(s_rScore, x, y)){
				IsScoreWindowActive() ? HideScore() : ShowScore();
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
		case WM_APP:
			goto destroy_setting_window;
		default:
			return FALSE;
	}
	return FALSE;
destroy_setting_window:
	ShowWindow(wgs.hWnd, SW_HIDE);
/*	RestoreModeWin(&wgs);
	wgs.hWnd = NULL;
	strncpy(wgs.szName, pszSettings, sizeof wgs.szName);*/
	return FALSE;
}

/* called every display frame */
static VOID CALLBACK FrameProc(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
){
	hwnd, uMsg, idEvent, dwTime; /* unused? */
//	wg.bUserUpdate = TRUE;	/* 描画はアプリケーション更新 */

	bMMX = bMMXAvailable && g_pinit->EnableMMX;

	{
	static clock_t foffset = 0;
	static LARGE_INTEGER li;
	clock_t clock1;
	LARGE_INTEGER li1;
	double oft = gs.ft;
	static timemeas_t tm;
	double dtime;
	static double sdtime;
	if(!foffset){
		foffset = clock();
		if(!QueryPerformanceCounter(&li))
			gs.hrt = FALSE;
		TimeMeasStart(&tm);
		sdtime = 0;
	}
	dtime = TimeMeasLap(&tm);
	gs.t = dtime;
	gs.ft = dtime - sdtime;
	gs.fft = (gs.fft * MIN(gs.fc, 10) + gs.ft) / (MIN(gs.fc, 10) + 1);
#ifndef NDEBUG
	s_ftlog[s_curftl] = gs.ft;
	ADV(s_curftl, ARRAYSIZE(s_ftlog));
#endif
	if(!gs.pause || g_pinit->DrawPause){
		gs.avgft = (gs.avgft * gs.fc + gs.ft) / (gs.fc + 1);
		gs.fc++;
	}
	sdtime = dtime;
	if(!gs.last) gs.last =/* gs.xlast =*/ gs.t;
	{
	/* check for P key every 100 ms if noDrawPause, almost sleeping all time */
	double caprate = gs.pause && !g_pinit->DrawPause ? 0.1000 :
		g_pinit->CapFrameRate ? 7./4/g_pinit->CapFrameRate : 0; /* donnask why, but it works! */
	if(caprate == 0){
		HDC hdc;
		hdc = GetDC(hwnd);
		caprate = 7./4./GetDeviceCaps(hdc, VREFRESH);
		ReleaseDC(hwnd, hdc);
	}
	oft = (oft + gs.ft) / 2;
	/* don't draw more frequent than the monitor's frequency which is plain waste of machine power. */
	if(oft < caprate) SleepEx(gs.slept = (DWORD)((caprate - oft)*1000)+1, FALSE);
	else gs.slept = INFINITE;
	}
	}
	if(!gs.pause){
		Animate();
		{
/*		int absvx = gs.vx == 0 ? 1 : gs.vx < 0 ? -gs.vx : gs.vx;
		while(gs.xlast + FRAMETIME / absvx <= gs.t){
			gs.xlast += FRAMETIME / absvx;
			if(gs.vx && gs.xc % (g_pinit->max_vx + 1 - absvx) == 0){
				gs.vx += gs.vx < 0 ? 1 : -1;
				absvx = gs.vx < 0 ? -gs.vx : gs.vx;
			}
			gs.xc++;
*//*				if(gs.vx < 0)
				SlipLeft(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur, bl.l[bl.cur-1].l-1);
			else
				SlipRight(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur, bl.l[bl.cur-1].r+1);
*/
//			}
		}
		if(gs.dpc){
/*			static unsigned delay = 1;
			static int avx;*/
			{
			unsigned i;
			for(i = 0; i < gs.dpc; i++){
				if(gs.vx && (gs.phit || !gs.delay || bl.l[bl.cur-1].b == YSIZ /*(gs.c-gs.dpc+i) % 2*/)/*&& (gs.c-gs.dpc+i) % ((g_pinit->max_vx + 1 - (gs.vx < 0 ? -gs.vx : gs.vx))) == 0*/)
					gs.vx += gs.vx < 0 ? 1 : -1, gs.delay = 2/*ABS(gs.vx)+1*/;
				if(gs.delay) gs.delay--; /* frictional dragging force */
			}
			}
			if(-(int)g_pinit->max_vx < gs.vx && GetAsyncKeyState(VK_LEFT)){
				int i = gs.vx - gs.dpc /** (ovx+2)*/;
				gs.vx = i < -(int)g_pinit->max_vx ? -(int)g_pinit->max_vx : i;
//				if(gs.vx == -(int)g_pinit->max_vx) dprintf("hit max -vx\n");
//				if(gs.vx < -(int)g_pinit->max_vx -1) gs.vx = -(int)g_pinit->max_vx-1;
			}
			if(gs.vx < (int)g_pinit->max_vx && GetAsyncKeyState(VK_RIGHT)){
				int i = gs.vx + gs.dpc /** (ovx-2)*/;
				gs.vx = i > (int)g_pinit->max_vx ? g_pinit->max_vx : i;
//				if(gs.vx == (int)g_pinit->max_vx) dprintf("hit max +vx\n");
//				if((int)g_pinit->max_vx +1 < gs.vx) gs.vx = (int)g_pinit->max_vx+1;
			}
/*			avx = gs.vx;*/
		}

		if(!gs.go){
		if(GetAsyncKeyState(VK_DOWN)){
			if(!gs.key.d){
				gs.downlast = gs.t;
				gs.key.d = 1;
			}
		}
		else if(gs.key.d) gs.key.d = 0;
		if(gs.key.d){
			int d = 0;
			while(gs.downlast + FRAMETIME / g_pinit->max_vy <= gs.t) d++, gs.downlast += FRAMETIME / g_pinit->max_vy;
			if(d && bl.cur)
				SlipDown(bl.l, bl.cur, &bl.l[bl.cur-1], bl.cur-1, MIN(bl.l[bl.cur-1].b + d,YSIZ));
		}

		/* rotate block */
		if(gs.key.u){
			if(!(GetAsyncKeyState(VK_UP)))
				gs.key.u = 0;
		}
		else if(GetAsyncKeyState(VK_UP)){
			block_t tb, *pb = &bl.l[bl.cur-1];
			int i;
			tb.l = (pb->t - pb->b + pb->l + pb->r) / 2; /* rotation math */
			tb.t = (pb->l - pb->r + pb->t + pb->b) / 2;
			tb.r = (pb->b - pb->t + pb->l + pb->r) / 2;
			tb.b = (pb->r - pb->l + pb->t + pb->b) / 2;
			i = GetAt(bl.l, bl.cur-1, &tb, bl.cur);
			if(i != (int)bl.cur-1){ /* some blocks stand on the way */
				block_t *pb = &bl.l[i];
				AddTefbox(g_ptefl, tb.l, tb.t, tb.r, tb.b, RGB(64, 64, 16), WG_BLACK, .5, 0);
				AddTefbox(g_ptefl, MAX(pb->l, tb.l), MAX(pb->t, tb.t), MIN(pb->r, tb.r),
					MIN(pb->b, tb.b), RGB(192, 192, 32), WG_BLACK, .75, 0);
			}
			else{
				if(tb.l < 0) SET(tb, 0, tb.t); /* check outcage case */
				if(XSIZ < tb.r) tb.l -= tb.r - XSIZ, tb.r = XSIZ; /* pull back to the ring */
				AddTefbox(g_ptefl, pb->l, pb->t, pb->r, pb->b, RGB(0, 128, 64), WG_BLACK, 1., TEF_BORDER);
				pb->l = tb.l;
				pb->t = tb.t;
				pb->r = tb.r;
				pb->b = tb.b;
			}
			gs.key.u = 1;
		}

#ifndef NDEBUG
		/* cheat! */
#endif
		}

		if(gs.key.s || (GetAsyncKeyState(VK_SPACE) & ~1)){
			if(gs.key.s || !gs.key.sp){
			if(!gs.go && bl.cur){ /* slide down */
				int top;
				unsigned n;
				block_t *cb;
				cb = &bl.l[bl.cur-1];
				top = cb->t;
				n = SlipDown(bl.l, bl.cur, cb, bl.cur-1, YSIZ);
				if(top != cb->t){ /* if moved at least a pixel, show sparks */
					double vy;
					int x;
					int i;
					if(n == bl.cur || bl.l[n].l < cb->l) x = cb->l, vy = -50;
					else x = bl.l[n].l, vy = 50;
					for(i = 0; i < 5; i++)
						AddTeline(g_ptell, x, cb->b, drand() * -50, drand() * vy + vy, .05,
							0, 0, 1., WG_WHITE, TEL_HEADFORWARD | TEL_REFLECT | TEL_SHRINK | TEL_VELOLEN, .25 + drand() * .25);
					if(n == bl.cur || cb->r < bl.l[n].r) x = cb->r, vy = -50;
					else x = bl.l[n].r, vy = 50;
					for(i = 0; i < 5; i++)
						AddTeline(g_ptell, x, cb->b, drand() * 50, drand() * vy + vy, .05,
							0, 0, 1., WG_WHITE, TEL_HEADFORWARD | TEL_REFLECT | TEL_SHRINK | TEL_VELOLEN, .25 + drand() * .25);
					AddTefbox(g_ptefl, bl.l[bl.cur-1].l, top, bl.l[bl.cur-1].r, bl.l[bl.cur-1].b, RGB(32,0,64), WG_BLACK, cnl_darkpurple[0].t, TEF_ADDBLEND);
				}
//				AddTefboxCustom(g_ptefl, bl.l[bl.cur-1].l, top, bl.l[bl.cur-1].r, bl.l[bl.cur-1].b, 0, &cs_rainbow, NULL);
				gs.vx = 0;
			}
			else if(gs.key.s){
				gs.key.s = 0; /* stop suicidal tendencies */
			}
			else{
				while(bl.cur){ /* immediately kill all blocks */
					BlockBreak(&bl.l[0], 0);
					KillBlock(&bl, 0);
				}
/*				initBlocks();*/
				gs.points = gs.area = 0;
				gs.go = 0;
			}
			gs.key.sp = 1;
			}
		}
		else if(gs.key.sp) gs.key.sp = 0;

		/* suicide key, enjoy the firework! */
		if(g_pinit->EnableSuicideKey && (GetAsyncKeyState(0x53) & ~1)){
			if(!gs.key.sp){
				gs.key.s = 1;
			}
		}
	}

	{
	int intopause = 0;
	/* P key */
	if(GetAsyncKeyState(0x50)/* & ~1*/ || us.pbutton){
		if(!gs.key.p){
			intopause = 1;
			gs.pause = !gs.pause;
			gs.key.p = 1;
			us.pbutton = 0;
			gs.last = gs.downlast /*= gs.xlast*/ = gs.t; /* initialize to begin frame. */
		}
	}
	else if(gs.key.p) gs.key.p = 0;

	if(!gs.pause || g_pinit->DrawPause)
		Draw(gs.fc);
	else if(intopause && g_pinit->DrawPauseString){
		DrawFontWin(&wg, "PAUSED", XSIZ / 2 - g_fontw * sizeof "PAUSED" / 2, YSIZ / 2, WG_WHITE,NULL);
		UpdateDrawRect(&wg, XSIZ/2 - g_fontw * sizeof"PAUSED"/2, YSIZ/2,
			XSIZ/2 + g_fontw * sizeof"PAUSED"/2, YSIZ/2 + g_fonth);
	}
	}
//	if(gs.vx && gs.c % (g_pinit->max_vx + 1 - (gs.vx < 0 ? -gs.vx : gs.vx)) == 0)
//	gs.vx += gs.vx < 0 ? 1 : -1;
}

/* spawn initial blocks */
static void initBlocks(){
	unsigned c = g_pinit->InitBlocks;
	int k = (int)((YSIZ - MAX_BLOCK_HEIGHT) * g_pinit->InitBlockRate);
	int bcb = 0;
	block_t cb; /* current block */
	if(bl.cur != 0) cb = bl.l[bl.cur-1], bcb = 1, bl.cur--;
	while(c--){
		block_t tmp;
		unsigned retries = 0;
		do{
			if(100 < retries++) goto escapeinitblock;
			tmp.l = rand() % (XSIZ - MIN_BLOCK_WIDTH);
			tmp.t = YSIZ - k +
				rand() % (k - MIN_BLOCK_HEIGHT - MAX_BLOCK_HEIGHT);
			tmp.r = tmp.l + MIN_BLOCK_WIDTH + rand() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH);
			if(XSIZ < tmp.r) tmp.r = XSIZ;
			tmp.b = tmp.t + MIN_BLOCK_HEIGHT + rand() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT);
			if(YSIZ < tmp.b) tmp.b = YSIZ;
		} while(GetAt(bl.l, bl.cur, &tmp, bl.cur) != bl.cur && (!bcb || !Intersects(tmp, cb)));
		SlipDown(bl.l, bl.cur, &tmp, bl.cur, YSIZ);
		AddBlock(&bl, tmp.l, tmp.t, tmp.r, tmp.b);
	}
escapeinitblock:
	if(bcb) AddBlock(&bl, cb.l, cb.t, cb.r, cb.b), bl.l[bl.cur-1] = cb;
	/*spawnNextBlock()*/;
}

#undef GetAsyncKeyState
static SHORT IsKeyActive(int vKey){
	if(GetForegroundWindow() != wg.hWnd) return 0;
	return GetAsyncKeyState(vKey);
}

int ReloadBGImage(struct ui_state *pus){
	COLORREF *pb;
	if(NULL != (pb = LoadBmp((path_t*)&((char*)&s_init)[(int)initset[pus->seti].dst], XSIZ, YSIZ, XSIZ*YSIZ*sizeof*pb))){
		EnterCriticalSection(&csimg);
		if(pimg) free(pimg);
		pimg = pb;
		LeaveCriticalSection(&csimg);
		return 1;
	}
	else
		return 0;
}

#if 0
static int s_seti = 0; /* setting index */
static RECT s_rLA, s_rRA, s_rBar, s_rSN, s_rSL, s_rSS, s_rVal, s_rSE; /* Save Settings button */
static enum lastselect{LS_NONE, LS_LA, LS_RA, LS_BAR, LS_SN, LS_SL, LS_SS, LS_VAL, LS_SE, NUM_LS} s_lastsel = LS_NONE; /* for buttons that take effects on mouse up */
static RECT *const s_parSet[NUM_LS] = {&s_rLA, &s_rRA, &s_rBar, &s_rSN, &s_rSL, &s_rSS, &s_rVal, &s_rSE};

#define SETARROW g_fonth
#define SETUNIT 4
#define SETXOFS (4 + SETARROW)

/* Draw contents of settings window */
static void SetDraw(LPWINGRAPH pwg, struct ui_state *pus){
	int x = 0, y = 0;
	int a = pus->seti;
	const char *p;
	static char strbuf[64];
	static int init = 0;
	if(!init){
		init = 1;
		s_rLA.left = 2, s_rLA.top = 2, s_rLA.right = 2 + SETARROW, s_rLA.bottom = g_fonth - 2;
		s_rRA.left = s_rLA.right + SETUNIT * g_initsetsize + 4, s_rRA.top = 2;
		s_rRA.right = s_rRA.left + SETARROW, s_rRA.bottom = g_fonth - 2;
		s_rBar.left = s_rLA.right + 2, s_rBar.top = 2, s_rBar.right = s_rRA.left - 2, s_rBar.bottom = g_fonth - 2;
		s_rSS.right = SETW, s_rSS.top = 0;
		s_rSS.left = SETW - g_fontw * sizeof pszSetSave, s_rSS.bottom = g_fonth;
		s_rSL.left = s_rSS.left - g_fontw * sizeof pszSetLoad, s_rSL.top = s_rSS.top;
		s_rSL.right = s_rSS.left, s_rSL.bottom = s_rSS.bottom;
		s_rSN.left = s_rRA.right + 2, s_rSN.top = 0;
		s_rSN.right = s_rSL.left, s_rSN.bottom = g_fonth;
		s_rVal.left = g_fontw *SETSIZE, s_rVal.top = 3 * g_fonth, s_rVal.right = SETW, s_rVal.bottom = 4 * g_fonth;
		s_rSE.top = s_rVal.top, s_rSE.right = SETW, s_rSE.bottom = s_rVal.bottom;
	}
	s_rSE.left = s_rVal.right = initset[pus->seti].cdc == &cdc_path ? SETW - g_fontw *sizeof"..." : SETW;
	ClearScreenWin(pwg, WG_BLACK);

	/* Draw the bar and the triangles */
	FillTriangleWin(pwg, 2, g_fonth / 2, SETARROW, g_fonth - 2, SETARROW, 2, RGB(192, 192, 0));
	FillRectangleWin(pwg, 4 + SETARROW, 2, SETUNIT * g_initsetsize + 4 + SETARROW, g_fonth - 2, RGB(40, 40, 0));
	RectangleWin(pwg, 4 + SETARROW, 2, SETUNIT * g_initsetsize + 4 + SETARROW, g_fonth - 2, RGB(127, 127, 0));
	FillRectangleWin(pwg, 4 + SETARROW + SETUNIT * a, 2, 4 + SETARROW + SETUNIT * (a+1), g_fonth - 2, RGB(255, 255, 0));
//	x = 6 + SETARROW + SETUNIT * g_initsetsize;
	x = s_rRA.left;
	FillTriangleWin(pwg, x + SETARROW - 2, g_fonth / 2, x, g_fonth - 2, x, 2, RGB(192, 192, 0));

	/* Draw setting number */
	sprintf(strbuf, "%d / %d", a+1, g_initsetsize);
	DrawFontWin(pwg, strbuf, SETUNIT * g_initsetsize + 8 + 2 * SETARROW, 0, RGB(255, 255, 127), NULL);

	/* Draw load/save button */
	FillRectangleWin(pwg, s_rSL.left, s_rSL.top, s_rSL.right, s_rSL.bottom, RGB(0, 40, 40));
	DrawFontWin(pwg, pszSetLoad, s_rSL.left + g_fontw / 2, s_rSL.top, RGB(127, 255, 255), NULL);
	FillRectangleWin(pwg, s_rSS.left, s_rSS.top, s_rSS.right, s_rSS.bottom, RGB(40, 0, 40));
	DrawFontWin(pwg, pszSetSave, s_rSS.left + g_fontw / 2, s_rSS.top, RGB(255, 127, 255), NULL);

	/* Draw contents of the current setting */
	DrawFontWin(pwg, pszSetName, 0, g_fonth, RGB(127, 255, 127), NULL);
	DrawFontWin(pwg, initset[a].str, g_fontw *SETSIZE, g_fonth, WG_WHITE, NULL);
	DrawFontWin(pwg, pszSetType, 0, 2 * g_fonth, RGB(127, 255, 127), NULL);
	DrawFontWin(pwg, initset[a].cdc->name, g_fontw *SETSIZE, 2 * g_fonth, WG_WHITE, NULL);
	DrawFontWin(pwg, pszSetValu, 0, 3 * g_fonth, RGB(127, 255, 127), NULL);
	initset[a].cdc->printer(strbuf, &((char*)&s_init)[(int)initset[a].dst], sizeof strbuf);
	if(initset[a].flags){ 
		RectangleWin(pwg, s_rVal.left, s_rVal.top, s_rVal.right, s_rVal.bottom, RGB(127, 127, 127));
		if(s_rSE.left != SETW){
			FillRectangleWin(pwg, s_rSE.left, s_rSE.top, s_rSE.right, s_rSE.bottom, RGB(0, 40, 0));
			DrawFontWin(pwg, "...", s_rSE.left + g_fontw / 2, s_rSE.top, RGB(127, 255, 127), NULL);
		}
	}
	DrawFontWin(pwg, strbuf, g_fontw *SETSIZE, 3 * g_fonth, initset[a].flags ? RGB(255, 255, 127) : RGB(127, 127, 127), NULL);
	DrawFontWin(pwg, pszSetDesc, 0, 4 * g_fonth, RGB(127, 255, 127), NULL);
	p = initset[a].desc;
	y = 4 * g_fonth;
	if(p) for(;; y += g_fonth){
		DrawFontWin(pwg, p, g_fontw *SETSIZE, y, WG_WHITE, NULL);
		if(strlen(p) < (SETW - g_fontw * SETSIZE) / g_fontw) break;
		p += (SETW - g_fontw * SETSIZE) / g_fontw;
	}
	UpdateDraw(pwg);
}

/* Message procedure for the settings window */
static BOOL SetMsgProc(LPWG_APPPROC p){
	int x, y;
	struct ui_state *const pus = (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam;
	//static int s_edit = 0;
	x = (WORD)(p->lParam & 0xffff), y = (WORD)(p->lParam >> 16);
	switch(p->Message){
		case WM_SYSCOMMAND:
			switch(p->wParam){
				case SC_CLOSE:
				if(!pus->setedit)
					PostMessage(wg.hWnd, WM_APP, 0, 0); /* notify the game window to kill me */
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
			else if(pus->setls == LS_VAL && initset[pus->seti].flags && RECTHIT(s_rVal, x, y) /*SETSIZE < x && x < SETW && 3 * g_fonth < y && y < 4 * g_fonth*/){
				static void *b = NULL;
				static size_t bsz = 0;
				initset[pus->seti].cdc->printer(buf, &((char*)&s_init)[(int)initset[pus->seti].dst], sizeof buf);
				if(initset[pus->seti].cdc == &cdc_bool){ /* special rule for bools */
					*(bool*)&(((char*)&s_init)[(int)initset[pus->seti].dst]) = !*(bool*)&(((char*)&s_init)[(int)initset[pus->seti].dst]);
				}
				else{ /* generally cdc->parser will do the job to interpret the input */
					pus->setedit = 1;
					if(Edit(&wgs, g_fontw * SETSIZE, 3 * g_fonth, SETW - g_fontw * SETSIZE, g_fonth, buf, sizeof buf, TRUE)){
loadpath:
						if(bsz < initset[pus->seti].cdc->size && NULL == (b = realloc(b, bsz = initset[pus->seti].cdc->size))){
							bsz = 0;
							return FALSE;
						}
						if(
							initset[pus->seti].cdc->parser(b, buf) &&
							(!initset[pus->seti].vrc || initset[pus->seti].vrc(b)) &&
							memcpy(&((char*)&s_init)[(int)initset[pus->seti].dst], b, initset[pus->seti].cdc->size)){
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
				strncpy(buf, (path_t*)&((char*)&s_init)[(int)initset[pus->seti].dst], sizeof buf);
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
					if(!SaveInitFile(&s_init, ofn.lpstrFile, 0))
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
					memcpy(bgimg, s_init.BackgroundImage, sizeof bgimg);
					if(!LoadInitFile(&s_init, ofn.lpstrFile))
						MessageBox(wgs.hWnd, BYLANG("Failed to read", "読み込みに失敗しました"), PROJECT, MB_OK | MB_ICONHAND);
					if(strncmp(bgimg, s_init.BackgroundImage, sizeof bgimg))
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
			if(!pus->setedit && pus->setls == LS_BAR && p->wParam & MK_LBUTTON && SETXOFS < x && x < SETXOFS + SETUNIT * g_initsetsize && 2 < y && y < g_fonth - 2){
				pus->seti = (x - SETXOFS) / SETUNIT;
				assert(0 <= pus->seti && pus->seti < g_initsetsize);
				SetDraw(&wgs, (struct ui_state*)((LPWINGRAPH)p->pGrpParam)->dwUserParam);
			}
			break;
		default:
			return FALSE;
	}
	return FALSE;
}
#endif

/* ビットマップファイルのパターンを読み込みウインドウサイズに合わせる */
static LPCOLORREF LoadBmp(LPCSTR FileName,int nWidth,int nHeight,DWORD dwSize)
{
	BITMAPINFOHEADER	bi;
	BITMAPDATA			bd;
	LPCOLORREF			pPat;
	DWORD ret;

	/* ビットマップファイルを読み込む(1) */
	if (!LoadBitmapData(FileName,&bi,&bd) && !LoadJPEGData(FileName,&bd)){
		if(INVALID_FILE_ATTRIBUTES != (ret = GetFileAttributes(FileName))){
			MessageBox(wgs.hWnd, BYLANG("The image's format is not supported.", "画像フォーマットがサポートされていないようです。"),
				PROJECT, MB_OK);
			assert(0);
		}
		return NULL;
	}
	assert(INVALID_FILE_ATTRIBUTES != (ret = GetFileAttributes(FileName)));

	/* ウインドウサイズ分のメモリを確保 */
	pPat = (LPCOLORREF )malloc(dwSize);
	if (!pPat){
		FreeBitmapData(&bd);
		return NULL;
	}

	/* 読み込んだビットマップパターンを指定サイズに合わせる */
	StretchPatternWin(
		nWidth,nHeight,
		bd.nWidth,bd.nHeight,bd.pBmp,pPat);

	FreeBitmapData(&bd);	/* ビットマップ用のメモリは解放 */

	swap3byte(pPat, dwSize);

	return pPat;
}


/* JPEGファイルのパターンを読み込みウインドウサイズに合わせる */
static LPCOLORREF LoadJPG(LPSTR FileName,int nWidth,int nHeight,DWORD dwSize)
{
	BITMAPDATA			bd;
	LPCOLORREF			pPat;

	/* ビットマップファイルを読み込む(1) */
	if (!LoadJPEGData(FileName,&bd))	return NULL;

	/* ウインドウサイズ分のメモリを確保 */
	pPat = (LPCOLORREF )malloc(dwSize);
	if (!pPat){
		FreeBitmapData(&bd);
		return NULL;
	}

	/* 読み込んだビットマップパターンをウインドウサイズに合わせる */
	StretchPatternWin(
		nWidth,nHeight,
		bd.nWidth,bd.nHeight,bd.pBmp,pPat);

	FreeBitmapData(&bd);	/* ビットマップ用のメモリは解放 */

	return pPat;
}

