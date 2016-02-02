typedef struct{const char *s;} st;
static int compar(const void *p1, const void *p2){
	return strcmp((*(st*)p1).s, (*(st*)p2).s);
}

	{
	st strtable[] = {
		{"anyone"},
		{"everyone"},
		{"noone"},
		{"someone"},
	};
	st key = {"noone"};
	char *result;
	qsort(strtable, sizeof strtable / sizeof *strtable, sizeof *strtable, compar);
	result = bsearch(&key, strtable, sizeof strtable / sizeof *strtable, sizeof *strtable, compar);
	if(result)
		printf("%s is found\n", result);
	else
		printf("nothing's found\n");
	}
