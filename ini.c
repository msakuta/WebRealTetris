#include "realt.h"
#include "dprint.h"
#include <windows.h>
#include "ini.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define numof(a) (sizeof(a)/sizeof*(a))

/* parse/encode/print functions for each types */
static int parse_int(void *p, const char *s){*(int*)p = atoi(s); return 1;}
static int parse_uint(void *p, const char *s){int i = atoi(s); if(i < 0) return 0; *(unsigned*)p = atoi(s); return 1;}
static int parse_double(void *p, const char *s){*(double*)p = atof(s); return 1;}
static int parse_bool(void *p, const char *s){if(strstr(s, "false")) *(bool*)p = FALSE; else if(strstr(s, "true")) *(bool*)p = TRUE; else return 0; return 1;}
static int parse_color24(void *p, const char *s){byte r, g, b; if(sscanf(s, "%u %u %u", &r, &g, &b) != 3) return 0; *(COLORREF*)p = RGB(r, g, b); return 1;}
static int parse_cstr(char const **p, const char *s){
	char *st;
	while(*s && isspace(*s)) s++;
	if(NULL == (st = (char*)malloc(strlen(s)+1))) return 0;
	strcpy(st, s); *p = st;
	return 1;
}
static int parse_path(const path_t *p, const char *s){while(*s && isspace(*s)) s++; strncpy((char*)p, s, sizeof(path_t)); return 1;}
static int encode_int(FILE *fp, const void *p){fprintf(fp, "%d", *(int*)p); return 1;}
static int encode_uint(FILE *fp, const void *p){fprintf(fp, "%u", *(unsigned*)p); return 1;}
static int encode_double(FILE *fp, const void *p){fprintf(fp, "%f", *(double*)p); return 1;}
static int encode_bool(FILE *fp, const void *p){fprintf(fp, *(bool*)p ? "true" : "false"); return 1;}
static int encode_color24(FILE *fp, const void *p){fprintf(fp, "%u %u %u", GetRValue(*(COLORREF*)p), GetGValue(*(COLORREF*)p), GetBValue(*(COLORREF*)p)); return 1;}
static int encode_cstr(FILE *fp, char const* const*p){fprintf(fp, "%s", *(char const* const*)p); return 1;}
static int encode_path(FILE *fp, const path_t *p){fprintf(fp, "%s", p); return 1;}
static int print_int(char *s, const int *p, size_t sz){sprintf(s, "%i", *p); return 1;}
static int print_uint(char *s, const unsigned *p, size_t sz){sprintf(s, "%u", *p); return 1;}
static int print_double(char *s, const double *p, size_t sz){sprintf(s, "%g", *p); return 1;}
static int print_bool(char *s, const bool *p, size_t sz){strncpy(s, *p ? "true" : "false", sz); return 1;}
static int print_color24(char *s, const COLORREF *p, size_t sz){sprintf(s, "%u %u %u", GetRValue(*(COLORREF*)p), GetGValue(*(COLORREF*)p), GetBValue(*(COLORREF*)p)); return 1;}
static int print_cstr(char *s, const char * const*p, size_t sz){strncpy(s, *p, sz); return 1;}
static int print_path(char *s, const path_t *p, size_t sz){strncpy(s, (const char*)p, sz); return 1;}
#define    vrc_double_na NULL
static int vrc_double_g0(const void *p){return *(double*)p > 0 ? 1 : 0;}
static int vrc_double_ge0(const void *p){return *(double*)p >= 0 ? 1 : 0;}
static int vrc_double_0to1(const void *p){return *(double*)p >= 0 && *(double*)p <= 1 ? 1 : 0;}
#define    vrc_uint_na NULL
#define    vrc_uint_ge0 NULL /* always in this range */
static int vrc_uint_g0(const void *p){return *(unsigned*)p > 0 ? 1 : 0;}
static int vrc_uint_0to100(const unsigned *p){return (0 <= *p && *p <= 100) ? 1 : 0;}
//static int vrc_uint_ge0(const void *p){return *(unsigned*)p >= 0 ? 1 : 0;}
#define    vrc_bool_na NULL
#define    vrc_color24_na NULL
#define    vrc_cstr_na NULL
#define    vrc_path_na NULL
#define    vrc_eBlockDrawMethod_na NULL

typedef int (*parserproc)(void*, const char*);
typedef int (*encoderproc)(FILE*, const void*);

static const struct codec
//cdc_int = {parse_int, encode_int, BYLANG("integer", "����"), sizeof(int)},
cdc_uint = {(parserproc)parse_uint, (encoderproc)encode_uint, print_int, BYLANG("unsigned integer", "����������"), sizeof(int)},
cdc_double = {parse_double, encode_double, print_double, BYLANG("real", "����"), sizeof(double)}
/*cdc_cstr = {parse_cstr, encode_cstr, print_cstr, BYLANG("string", "������"), sizeof(char*)},*/
;
const struct codec
cdc_bool = {parse_bool, encode_bool, print_bool, BYLANG("boolean", "�_��"), sizeof(bool)},
cdc_path = {parse_path, encode_path, print_path, BYLANG("file path", "�t�@�C���p�X"), sizeof(path_t)}
;
/*cdc_color24 = {parse_color24, encode_color24};*/
/*
static const struct vrc{
	int (*checker)(const void*);
	const char *name;
}
vrc_double_g0 = {vrcf_double_g0, "Greater than 0"},
vrc_double_ge0 = {vrcf_double_ge0, "Greater than or equal to 0"};
*/
const struct codec
cdc_eBlockDrawMethod = {(parserproc)parse_uint, (encoderproc)encode_uint, print_int, "BlockDrawMethod", sizeof(enum eBlockDrawMethod)};
#if LANG==JP
#define MATCHSET(name,type,range,desc,descj) {#name,&((struct init*)NULL)->name,&cdc_##type,vrc_##type##_##range,descj,0}
#define MATCHSETM(name,type,range,desc,descj) {#name,&((struct init*)NULL)->name,&cdc_##type,vrc_##type##_##range,descj,1}
#else
#define MATCHSET(name,type,range,desc,descj) {#name,&((struct init*)NULL)->name,&cdc_##type,vrc_##type##_##range,desc,0}
#define MATCHSETM(name,type,range,desc,descj) {#name,&((struct init*)NULL)->name,&cdc_##type,vrc_##type##_##range,desc,1}
#endif
struct matchset initset[] = {
	MATCHSETM(BackgroundImage, path, na, "Bitmap image file name used to decolate the background", "�w�i�Ɏg����摜�t�@�C����"),
	MATCHSET(HighScoreFileName, path, na, "Name of file whom highscores are written to.", "�n�C�X�R�A���L�^�����t�@�C����"),
	MATCHSETM(BackgroundBrightness, uint, 0to100, "[0-100] Brightness of background image", "[0-100] �w�i�摜�̖��邳"),
	MATCHSETM(BackgroundBlendMethod, uint, ge0, "0 - Disabled, 1 - Alpha, 2 - Additive, Hotkey: \'B\'", "0 - ����, 1 - �A���t�@, 2 - ���Z�A �z�b�g�L�[: \'B\'"),
	MATCHSETM(CollapseEffectType, uint, ge0, "0 - auto, 1 - shards, 2 - fireworks, 3 - electric surge", "�������: 0 - ����, 1 - �j��, 2 - �ԉ�, 3 - �d��"),
	MATCHSETM(Gravity, double, na,
		"[pixels/second^2] Magnitude of gravity which affects behavior of visual effects",
		"[�s�N�Z��/�b^2] ���o���ʂɉe������d�͂̑傫��"),
	MATCHSET(Frametime, double, g0,
		"[seconds] How long it takes to calculate next physical step",
		"[�b] �������Z�̊Ԋu"),
	MATCHSETM(CapFrameRate, double, ge0,
		"[Hz] Game's frame rate will aim this value, set monitor's frequency to gain optimized performance. It's nonsense to draw more rapid than the monitor.",
		"[Hz] �v���O�����́A�\���t���[�����[�g�����̒l���傫���Ȃ�Ȃ��悤�ɂ��悤�Ƃ���B���j�^���p�ɂɕ`�悵�Ă��Ӗ����Ȃ��̂ŁA���j�^�̎��g���������̂���Ԍ������悢�B"),
	MATCHSETM(SidePanelRefreshRate, double, ge0,
		"[seconds] How often to refresh the information bar at right side, set 0 to refresh every frame",
		"[�b] ���̏��o�[���X�V����Ԋu�B0���w�肷��Ɩ��t���[���X�V����"),
	MATCHSET(ClearRate, double, 0to1,
		"The least horizontal space rate to blocks not explode, specified between 0 and 1",
		"�u���b�N�����������̉������̌��Ԃ̕���0����1�܂łŎw�肷��"),
	MATCHSET(AlertRate, double, 0to1,
		"If the space gets below, red lines will appear to indicate collapse chance, specified between 0 to 1",
		"�u���b�N�����������ȂƂ����\�����n�߂錄�Ԃ̕���0����1�܂łŎw�肷��"),
	{"MaxHorizontalVelocity", &((struct init*)NULL)->max_vx, &cdc_uint, NULL, BYLANG(
		"[pixels/second] How fast the falling block can maneuver sideways",
		"[�s�N�Z�����b] �u���b�N���������ɓ����ō��̑���")},
	{"MaxVerticalVelocity", &((struct init*)NULL)->max_vy, &cdc_uint, NULL, BYLANG(
		"[pixels/second] How fast the falling block can fall when down key is pressed",
		"[�s�N�Z�����b] ���������̑���")},
	MATCHSETM(InitBlockRate, double, 0to1, "Rate at how full the initially spawned blocks can stack", "�ŏ��̃u���b�N���ǂꂾ����Ԃ��݂�����"),
	MATCHSETM(InitBlocks, uint, ge0, "Count of initially spawned blocks", "�ŏ��Ɍ����u���b�N�̐�"),
	MATCHSET(BlockDebriLength, double, g0, "Length of individual debris", "�u���b�N������Ƃ��̐��̒���"),
	MATCHSET(BlockDebriLifeMin, double, g0, "[seconds] Maximum life for each debri", "[�b] ���̍Œ�����"),
	MATCHSETM(BlockDebriLifeMax, double, g0, "[seconds] Minimum life for each debri", "[�b] ���̍ŒZ����"),
	{"BlockDeathFragmentFactor", &((struct init*)NULL)->fragm_factor, &cdc_double, vrc_double_g0, BYLANG(
		"Rate that how many fragments are spawned per pixel area when block is being destroyed",
		"�u���b�N������Ƃ��A�P�ʕ����s�N�Z�������肢���̔j�Ђ��������邩")},
	MATCHSETM(BlockFragmentTrailRate, double, 0to1,
		"[0-1] Rate at how oftenly trailing fragment appear",
		"[0�`1] �������������m��"),
	{"BlockDeathFragmentMax", &((struct init*)NULL)->fragm_c_max, &cdc_uint, vrc_uint_g0, BYLANG(
		"How many fragments are spawned for a block at most",
		"�u���b�N������Ƃ��̃u���b�N������̔j�Ђ̍ő吔"), 1},
	{"BlockDeathFragmentLifeMin", &((struct init*)NULL)->fragm_life_min, &cdc_double, vrc_double_g0},
	{"BlockDeathFragmentLifeMax", &((struct init*)NULL)->fragm_life_max, &cdc_double, vrc_double_g0},
	MATCHSETM(TelineElasticModulus, double, ge0,
		"Degree on how hard line effects bounce back from walls or floor",
		"�����ǂ��甽�����鋭���̓x����(�e���W��)"),
	MATCHSET(TelineShrinkStart, double, ge0,
		"[seconds] Remaining lifetime when a line effect starts to shrink in length",
		"[�b] �����k�ݎn�߂�_�ł̎c�����"),
	MATCHSET(TelineMaxCount, uint, ge0,
		"Temporary entities' maximum buffer size, set 0 to disable the feature",
		"���o���ʗp�̃o�b�t�@�̍ő吔�B0���w�肷��Ό��ʂ͎g���Ȃ�"),
	MATCHSET(TelineInitialAlloc, uint, g0,
		"Temporary entities' buffer is initially allocated by this size",
		"���o���ʗp�̃o�b�t�@�͍ŏ��ɂ��ꂾ���m�ۂ����"),
	MATCHSET(TelineExpandUnit, uint, ge0,
		"Temporary entities' buffer is expanded by this number as appropriate",
		"���o���ʗp�̃o�b�t�@�͕K�v�ȂƂ��ɂ��ꂾ���ǉ��m�ۂ����"),
	MATCHSET(TelineAutoExpand, bool, na,
		"[true or false] Whether expand by TelineCount when effects count hit max",
		"[true or false] �ǉ��m�ۂ��邩�ۂ�"),
	MATCHSET(TefboxMaxCount, uint, ge0,
		"Temporary entities' maximum buffer size, set 0 to disable the feature",
		"���o���ʗp�̃o�b�t�@�̍ő吔�B0���w�肷��Ό��ʂ͎g���Ȃ�"),
	MATCHSET(TefboxInitialAlloc, uint, g0,
		"Temporary entities' buffer is initially allocated by this size",
		"���o���ʗp�̃o�b�t�@�͍ŏ��ɂ��ꂾ���m�ۂ����"),
	MATCHSET(TefboxExpandUnit, uint, ge0,
		"Temporary entities' buffer is expanded by this number as appropriate",
		"���o���ʗp�̃o�b�t�@�͕K�v�ȂƂ��ɂ��ꂾ���ǉ��m�ۂ����"),
	MATCHSET(TefboxAutoExpand, bool, na,
		"[true or false] Whether expand by TefboxCount when effects count hit max",
		"[true or false] �ǉ��m�ۂ��邩�ۂ�"),
	MATCHSET(TefpolMaxCount, uint, ge0,
		"Temporary entities' maximum buffer size, set 0 to disable the feature",
		"���o���ʗp�̃o�b�t�@�̍ő吔�B0���w�肷��Ό��ʂ͎g���Ȃ�"),
	MATCHSET(TefpolInitialAlloc, uint, g0,
		"Temporary entities' buffer is initially allocated by this size",
		"���o���ʗp�̃o�b�t�@�͍ŏ��ɂ��ꂾ���m�ۂ����"),
	MATCHSET(TefpolExpandUnit, uint, ge0,
		"Temporary entities' buffer is expanded by this number as appropriate",
		"���o���ʗp�̃o�b�t�@�͕K�v�ȂƂ��ɂ��ꂾ���ǉ��m�ۂ����"),
	MATCHSET(TefpolAutoExpand, bool, na,
		"[true or false] Whether expand by TefboxCount when effects count hit max",
		"[true or false] �ǉ��m�ۂ��邩�ۂ�"),
	MATCHSET(PolygonVertexBufferSize, uint, ge0,
		"Size of memory pool for polygon or polyline's vertices",
		"���p�`�̒��_�p�̃������v�[���̃T�C�Y"),
	MATCHSETM(GuidanceLevel, uint, g0,
		"0 - No guides, 1 - Lines, 2 - Land prediction, 3 - Lines and prediction",
		"0 - �K�C�h�Ȃ�, 1 - ��, 2 - �����_�\��, 3 - ���Ɨ\��"),
	{"ClearAllBlocksOnGameOver", &((struct init*)NULL)->clearall, &cdc_bool, NULL, BYLANG(
		"[true or false] Whether issue fireworks on game over",
		"[true or false] �Q�[���I�[�o�[���Ƀu���b�N���󂷂��ۂ�"), 1},
	MATCHSETM(EnableFadeScreen, bool, na,
		"[true or false] Whether enable offscreen fading effect buffer which loads heavily, enable only on a fast computer",
		"[true or false] �w�i�̗������ʂ�L���ɂ��邩�ۂ��A�����}�V���ɂ̂ݐ���"),
	MATCHSETM(EnableTransBuffer, bool, na,
		"[true or false] Whether enable offscreen shimmering effect buffer, only enabled when FadeScreen is also on",
		"[true or false] �w�i�̂�炮���ʂ�L���ɂ��邩�ۂ��AFadeScreen���L���Ȏ��̂ݗL��"),
	MATCHSETM(EnableSuicideKey, bool, na,
		"[true or false] Whether enable suicide key (s)",
		"[true or false] ���E�L�[��L���ɂ��邩�ۂ� (s)"),
#if TRANSFUZZY
	MATCHSETM(TransBufferFuzzyness, uint, ge0,
		"[0-2] Fuzzy level of the shimmering effect",
		"[0-2] ��炮���ʂ̃X���[�W���O"),
#endif
	MATCHSETM(DrawPause, bool, na,
		"[true or false] Whether draw screen (but not calculate motions) when paused. If set, color cycle and random colors are in effect.",
		"[true or false] �|�[�Y���ɓ����͌v�Z�����ɍĕ`�悾�����J��Ԃ����ۂ��Btrue�Ȃ�A�F��]����у����_���F�͕`�悵�Ȃ������"),
	MATCHSETM(DrawPauseString, bool, na,
		"[true or false] Whether draw \"PAUSED\" string on the screen when paused, turn off if a screenshot is needed",
		"[true or false] ������\"PAUSED\"���|�[�Y���ɕ`�悷�邩�ۂ��B�X�N���[���V���b�g���Ƃ�Ȃ�false�ɂ���ƕ֗��B"),
	MATCHSETM(DrawGameOver, bool, na,
		"[true or false] Whether draw GAME OVER string on the screen, turn off if a screenshot is needed",
		"[true or false] �Q�[���I�[�o�[�������`�悷�邩�ۂ��B�X�N���[���V���b�g���Ƃ�Ȃ�false�ɂ���ƕ֗��B"),
	MATCHSETM(BlockFollowShadowLife, double, ge0, 
		"[seconds] Life of currently falling block's following shadow effect. set 0 to disable the feature.",
		"[�b] �u���b�N�̌o�H�Ɏc����ʂ̎����B0�Ō��ʂ͖����ɂȂ�B"),
	MATCHSETM(EnableMMX, bool, na, 
		"[true or false] Whether use MMX technology for raster operations. If MMX is available, graphics generally draw faster using it.",
		"[true or false] ���X�^������MMX���g�����ۂ��B MMX���g�p�ł���ꍇ�́A�ʏ�g�p���������`�悪�����ɂȂ�"),
	MATCHSETM(EnableMultiThread, bool, na, 
		"[true or false] Whether use parallel processing for raster operations. If multiple processors are available or the processor has HyperThreading technology, graphics might draw faster using them.",
		"[true or false] ���X�^�����ɕ��񏈗����g�����ۂ��B �v���Z�b�T���������邩�A HyperThreading �e�N�m���W�[�����ڂ��ꂽCPU���������̃��b�`�ȕ��̏ꍇ�́A�g�p���������`�悪�����ɂȂ�\��������"),
	MATCHSETM(BlockDrawMethod, uint, ge0, 
		"[0-2] Method to draw blocks, 0 - Border only, 1 - Fill Interior, 2 - Additive Interior",
		"[0-2] �u���b�N��`�悷����@�B 0 - �g�̂�, 1 - ������h��Ԃ�, 2 - �����𔼓����ɂ���"),
};

int LoadInitFile(struct init *in, const char *fname){
	FILE *fp = fopen(fname, "r");
	if(fp){
		char buf[128];
		byte read[sizeof initset / sizeof *initset];
		int l = 0;
		{
			int i;
			for(i = 0; i < numof(read); i++)
				read[i] = 0;
		}
		while(!feof(fp) && !ferror(fp)){
			char c, *p = buf;
			const char *mt[sizeof initset / sizeof *initset];
			int i;
			if(NULL == fgets(buf, sizeof buf, fp)) continue;
			l++;
			if(buf[strlen(buf)-1] != '\n') while((c = (char)getc(fp)) != '\n' && !(c == EOF && feof(fp)));
			while(isspace(*p)) if(*p++ == '\n') goto gcon;
			if(*p == ';') continue;
			for(i = 0; i < sizeof initset / sizeof *initset; i++)
				mt[i] = initset[i].str;
			for(; *p != '\n'; p++){
				if(*p == ';' || *p == '\n') goto gcon;
				if(!isalnum(*p)) break;
				for(i = 0; i < sizeof initset / sizeof *initset; i++){
					if(!mt[i]) continue;
					if(tolower(*mt[i]) == tolower(*p)){ /* ignore case */
						mt[i]++;
						if(mt[i] == '\0'){
						}
					}
					else
						mt[i] = NULL;
				}
			}
			{
			int erange;
			for(i = 0; i < sizeof initset / sizeof *initset; i++) if(mt[i] && *mt[i] == '\0'){
				char *p1;
				void *dst = &((char*)in)[(int)initset[i].dst];
				read[i] = 1;
				if(NULL == (p = strchr(p, '=')) || (NULL != (p1 = strpbrk(p, ";\n")) ?
				 *p1 = '\0' : 0, !initset[i].cdc->parser(dst, p+1))){
					erange = 0;
					goto error;
				}
				if(initset[i].vrc && !initset[i].vrc(dst)){
					erange = 1;
					goto error;
				}
#ifndef NDEBUG
				dprintf("%s=", initset[i].str);
	//			dprintencoder(initset[i].dst, initset[i].cdc->encoder, in);
	//			initset[i].encoder(*g_dprint_logfpp, initset[i].dst);
				dprintf("\n");
#endif
				goto gcon;
			}
			if(i == numof(initset))
				dprintf("%s(%d): Unknown key name\n", fname, l);
gcon:		continue;
error:
#ifndef NDEBUG
			dprintf(erange ?
				"%s(%d): Range Check Error for %s\n" :
				"%s(%d): Couldn't parse initializer for %s\n", fname, l, initset[i].str);
			exit(0);
#else
			sprintf(buf, erange ?
				BYLANG("%s(%d): Range Check Error for %s", "%s(%d): �������l��ɓ����Ă��܂���: %s") :
				BYLANG("%s(%d): Parse Error for %s", "%s(%d): ��̓G���["), fname, l, initset[i].str);
			MessageBox(NULL, buf, PROJECT, MB_OK | MB_ICONERROR);
			exit(0);
#endif
			}
		}
		fclose(fp);
#if RESTORE_MISSING_KEYS
		{
		int n;
		dprintf("%u entries total\n", numof(read));
		{
		int i;
		for(i = 0, n = 0; i < numof(read); i++) if(!read[i]){
			dprintf("%s key was not set\n", initset[i].str);
			n++;
		}
		}
		if(n){
			int ret;
			char s[256];
			sprintf(s, BYLANG(
				"Some keys are missing in \"%s\". Will you add the missing keys to the end of the file with default values?",
				"\"%s\"���̐ݒ薼�ɑ���Ȃ����̂�����܂��B�t�@�C���̖����Ɋ���l��ǉ����܂����H"), fname);
			dprintf(n == 1 ? "%d key was not set\n" : "%d keys were not set\n", n);
			ret = MessageBox(NULL, s, PROJECT, MB_YESNOCANCEL | MB_ICONQUESTION);
			switch(ret){
				case IDYES:
					if(NULL == (fp = fopen(fname, "a")))
						break;
					{
					int i;
					void *dst = &((char*)in)[(int)initset[i].dst];
					time_t tm;
					time(&tm);
					fprintf(fp, "\n; RealTetris " BYLANG("build","�r���h") ": "__DATE__" "__TIME__", "BYLANG("write","�����o��")": %s", ctime(&tm));
					fputs(BYLANG(
						"; Keys below were automatically created with default values.\n\n",
						"; �ȉ��̐ݒ薼�͎����I�Ɋ���l�Œǉ����ꂽ���̂ł��B\n\n"), fp);
					for(i = 0; i < numof(initset); i++) if(!read[i]){
						fprintf(fp, "%s = ", initset[i].str);
						initset[i].cdc->encoder(fp, dst);
						if(initset[i].desc)
							fprintf(fp, " ; %s\n", desc);
						else
							putc('\n', fp);
					}
					fclose(fp);
					break;
						}
				case IDCANCEL: return(0);
				case IDNO: break;
			}
		}
		}
#endif
	}
	else{ /* inifile does not exist */
		int ret;
		char s[256];
		sprintf(s, BYLANG(
			"Initializer file \"%s\" is missing. Will you create one with default values?",
			"�ݒ�t�@�C��\"%s\"������܂���B����l�̂��̂��쐬���܂����H"), fname);
		ret = MessageBox(NULL, s, PROJECT, MB_YESNOCANCEL | MB_ICONQUESTION);
		switch(ret){
			case IDYES:{
				SaveInitFile(in, fname, 1);
				/*
				if(NULL == (fp = fopen(INIFILE, "w")))
					break;
				{
				int i;
				time_t tm;
				time(&tm);
				fprintf(fp, "\n; RealTetris "BYLANG("build","�r���h")": "__DATE__" "__TIME__", "BYLANG("write","�����o��")": %s", ctime(&tm));
				fputs(BYLANG(
					"; This file was automatically created with default values.\n"
					"; Tune values to tweak the program's performance.\n\n",
					"; ���̃t�@�C���͋K��l�Ŏ����I�ɍ쐬���ꂽ���̂ł��B\n"
					"; �l�𒲐߂��ăv���O�����̃p�t�H�[�}���X�����P���Ă�������\n\n"), fp);
				for(i = 0; i < numof(initset); i++){
					fprintf(fp, "%s = ", initset[i].str);
					initset[i].cdc->encoder(fp, initset[i].dst);
					if(initset[i].desc)
						fprintf(fp, " ; %s\n", initset[i].desc);
					else
						putc('\n', fp);
				}
				fclose(fp);*/
				break;
					   }
			case IDCANCEL: return(0);
			case IDNO: break;
		}
	}

	/* verify the contents. Basic value range checks are provided by the matchset,
	  here checks inter-related values. */
	if(in->BlockDebriLifeMax < in->BlockDebriLifeMin){
		char s[256];
		sprintf(s, "%s: \n" BYLANG(
			"Invalid: DebriLifeMin is greater than DebriLifeMax",
			"����: DebriLifeMin �� DebriLifeMax ���傫���Ȃ��Ă��܂�"), fname);
		MessageBox(NULL, s, PROJECT, MB_OK | MB_ICONERROR);
		exit(0);
	}
	if(!in->TelineMaxCount)
		in->TelineInitialAlloc = in->TelineExpandUnit = 0, dprintf("Note: Teline disabled\n");
	else{
		if(in->TelineMaxCount < in->TelineInitialAlloc)
			in->TelineInitialAlloc = in->TelineMaxCount,
			dprintf("Note: Rerolled TelineInitialAlloc to TelineMaxCount = %u", in->TelineInitialAlloc);
		if(in->TelineMaxCount < in->TelineInitialAlloc + in->TelineExpandUnit)
			in->TelineExpandUnit = in->TelineMaxCount - in->TelineInitialAlloc,
			dprintf("Note: Rerolled TelineExpandUnit to TelineMaxCount - TelineInitialAlloc = %u", in->TelineExpandUnit);
	}
	if(!in->TefboxMaxCount)
		in->TefboxInitialAlloc = in->TefboxExpandUnit = 0, dprintf("Note: Tefbox disabled\n");
	else{
		if(in->TefboxMaxCount < in->TefboxInitialAlloc)
			in->TefboxInitialAlloc = in->TefboxMaxCount,
			dprintf("Note: Rerolled TefboxInitialAlloc to TefboxMaxCount = %u", in->TefboxInitialAlloc);
		if(in->TefboxMaxCount < in->TefboxInitialAlloc + in->TefboxExpandUnit)
			in->TefboxExpandUnit = in->TefboxMaxCount - in->TefboxInitialAlloc,
			dprintf("Note: Rerolled TefboxExpandUnit to TefboxMaxCount - TefboxInitialAlloc = %u", in->TefboxExpandUnit);
	}
	return 1;
}

int SaveInitFile(const struct init *in, const char *fname, int def){
	FILE *fp;
	if(NULL == (fp = fopen(fname, "w")))
		return 0;
	{
	int i;
	time_t tm;
	time(&tm);
	fprintf(fp, "\n; RealTetris "BYLANG("build","�r���h")": "__DATE__" "__TIME__", "BYLANG("write","�����o��")": %s", ctime(&tm));
	fputs(def ? BYLANG(
		"; This file was automatically created with default values.\n"
		"; Tune values to tweak the program's performance.\n\n",
		"; ���̃t�@�C���͋K��l�Ŏ����I�ɍ쐬���ꂽ���̂ł��B\n"
		"; �l�𒲐߂��ăv���O�����̃p�t�H�[�}���X�����P���Ă�������\n\n")
		: BYLANG(
		"; This file was automatically created.\n"
		"; Tune values to tweak the program's performance.\n\n",
		"; ���̃t�@�C���͎����I�ɍ쐬���ꂽ���̂ł��B\n"
		"; �l�𒲐߂��ăv���O�����̃p�t�H�[�}���X�����P���Ă�������\n\n")
		, fp);
	for(i = 0; i < numof(initset); i++){
		fprintf(fp, "%s = ", initset[i].str);
		initset[i].cdc->encoder(fp, &((char*)in)[(int)initset[i].dst]);
		if(initset[i].desc)
			fprintf(fp, " ; %s\n", initset[i].desc);
		else
			putc('\n', fp);
	}
	fclose(fp);
	}
	return 1;
}

int g_initsetsize = numof(initset);
