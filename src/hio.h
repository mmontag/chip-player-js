#ifndef XMP_HIO_H
#define XMP_HIO_H

typedef struct {
#define HANDLE_TYPE_FILE	0
#define HANDLE_TYPE_MEMORY	1
	int type;
	FILE *f;
	uint8 *p;
	uint8 *start;
} HANDLE;

int8	hread_8s	(HANDLE *);
uint8	hread_8		(HANDLE *);
uint16	hread_16l	(HANDLE *);
uint16	hread_16b	(HANDLE *);
uint32	hread_24l	(HANDLE *);
uint32	hread_24b	(HANDLE *);
uint32	hread_32l	(HANDLE *);
uint32	hread_32b	(HANDLE *);
int	hread		(void *, int, int, HANDLE *);	
int	hseek		(HANDLE *, long, int);
long	htell		(HANDLE *);
int	heof		(HANDLE *);
HANDLE *hopen		(void *, int);
int	hclose		(HANDLE *);

#endif
