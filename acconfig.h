/* $IdPath$ */

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

/* combined test for fork/way/msg* */
#undef USE_FORKWAITMSG

/* Check for GNU C Library */
#undef HAVE_GNU_C_LIBRARY

@BOTTOM@

#endif /* YASM_CONFIG_H */
