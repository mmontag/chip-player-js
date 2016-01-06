/*
 * Modeling file for Coverity Scan
 */
typedef unsigned long size_t;
typedef void FILE;

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	// do nothing
}

int fgetc(FILE *stream)
{
	// do nothing
}
