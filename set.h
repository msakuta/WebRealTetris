#ifndef SET_H
#define SET_H
#include "tent.h"
#define VC_EXTRALEAN
#include <windows.h>
#include "../WinGraph.h"

extern void SetDraw(LPWINGRAPH, struct ui_state*);
extern BOOL SetMsgProc(LPWG_APPPROC p);

/* GUI or graphics specific status for a set of windows */
struct ui_state{
	const char *message; /* displaying message in center of game zone */
	int mdelay; /* in milliseconds */
	int pbutton; /* temporal variable to notify the frameproc that the pause button on the gui is pressed */
	int lastbreak; /* graphics type */
	int seti; /* set index, current setting identifier */
	int setedit; /* whether editing field is active in the settings window */
	enum lastselect setls; /* most recently selected element */
	int fonth, fontw; /* font height/width */
	teout_t to; /* store trivial values here */
	teout_t toptb; /* buffered drawing */
};

extern WINGRAPH wgs;

#endif
