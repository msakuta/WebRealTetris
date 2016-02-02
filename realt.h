#ifndef REALT_H
#define REALT_H

/*#############################################################################
	macro definitions
#############################################################################*/
#define PROJECT "Real Tetris" /* Project Name */
#define EN 0 /* Language Codes */
#define JP 1
#ifndef LANG /* I won't use these silly resources */
#define LANG EN
#endif
#define	XRES 500 /* Window width */
#define	YRES 480 /* Window height */
#define XSIZ 300 /* game zone size */
#define YSIZ 480 /* " */
#define SETW 400 /* Settings Window width */
#define SETH 200 /* " height */
#define FRAMETIME (g_pinit->Frametime)/*0.030*/ /* each physical frame lasts this seconds */
#define GRAVITY (g_pinit->Gravity) /* units/s^2 */
#if LANG==JP
#define BYLANG(en,jp) jp
#else
#define BYLANG(en,jp) en
#endif

/* ƒTƒCƒY‚ª‘å‚«‚¢•û‚É‡‚í‚¹‚é */
#if (XRES > YRES)
	#define		SCW			YRES
	#define		SCH			YRES
	#define		SC_OX		((XRES - YRES) / 2)
	#define		SC_OY		0
#else
	#define		SCW			XRES
	#define		SCH			XRES
	#define		SC_OX		0
	#define		SC_OY		((YRES - XRES) / 2)
#endif


/*-----------------------------------------------------------------------------
	external function declarations
-----------------------------------------------------------------------------*/
extern int PutScore(int scoreValue);
extern int ShowScore(void);
extern int LoadScore(const char *file);
extern int SaveScore(const char *file);
extern int IsScoreWindowActive(void);
struct ui_state;
int ReloadBGImage(struct ui_state *pus);


/*=============================================================================
	type definitions
=============================================================================*/
typedef unsigned char bool;
typedef char path_t[260];
extern const struct init{
	double Gravity;
	double Frametime;
	double CapFrameRate;
	double SidePanelRefreshRate;
	double ClearRate, AlertRate;
	double InitBlockRate;
	double BlockDebriLength, BlockDebriLifeMin, BlockDebriLifeMax;
	double BlockFollowShadowLife;
	double TelineElasticModulus;
	double TelineShrinkStart;
	double fragm_factor, fragm_life_min, fragm_life_max, BlockFragmentTrailRate;
	unsigned fragm_c_max, BlockFragmentStarWeight;
	unsigned max_vx, max_vy;
	unsigned InitBlocks;
	unsigned TelineMaxCount, TelineInitialAlloc, TelineExpandUnit;
	bool TelineAutoExpand;
	unsigned TefboxMaxCount, TefboxInitialAlloc, TefboxExpandUnit;
	bool TefboxAutoExpand, TefpolAutoExpand;
	unsigned TefpolMaxCount, TefpolInitialAlloc, TefpolExpandUnit;
	unsigned PolygonVertexBufferSize;
	unsigned GuidanceLevel, CollapseEffectType, TransBufferFuzzyness;
	unsigned BackgroundBrightness, BackgroundBlendMethod;
	unsigned BlockDrawMethod;
	path_t BackgroundImage, HighScoreFileName;
	bool clearall, EnableFadeScreen, EnableTransBuffer;
	bool EnableSuicideKey;
	bool DrawPause, DrawPauseString, DrawGameOver;
	bool RoundDouble;
	bool EnableMMX;
	bool EnableMultiThread;
} * const g_pinit; /* initialize file contents, not modifiable */

#endif /* REALT_H */
