#ifndef SWAPB_H
#define SWAPB_H

void AddConstBox(COLORREF *pDest, COLORREF value, int x, int y, int dx, int dy, int w);
extern void FadeAddMMX(DWORD dst[4], const DWORD src[6], int dwcount);
extern void FadeAddMMXT(DWORD dst[4], const DWORD src[6], int dwcount, int (*random_seed)[2]);
extern void FadeAddMMX2(DWORD dst[4], const DWORD src[6]);


#endif
