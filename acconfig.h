/* $Id: acconfig.h,v 1.2 2001/08/30 03:45:16 peter Exp $ */

#ifndef YASM_CONFIG_H
#define YASM_CONFIG_H

/* Generated automatically from acconfig.h by autoheader. */
/* Please make your changes there */

@TOP@

/* Workaround for bad <sys/queue.h> implementations. */
#undef HAVE_BOGUS_SYS_QUEUE_H

/* gettext tests */
#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_LC_MESSAGES
#undef HAVE_STPCPY

@BOTTOM@

#endif /* YASM_CONFIG_H */
