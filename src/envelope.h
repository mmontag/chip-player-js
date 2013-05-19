#ifndef LIBXMP_ENVELOPE_H
#define LIBXMP_ENVELOPE_H

/* Envelope */

int get_envelope(struct xmp_envelope *, int, int, int *);
int update_envelope(struct xmp_envelope *, int, int);
int check_envelope_fade(struct xmp_envelope *, int);

#endif
