dnl Extracted from automake-1.5 and sligtly modified for Xine usage.
dnl Daniel Caujolle-Bert <segfault@club-internet.fr>

# Figure out how to run the assembler.

# AM_PROG_AS_MOD
AC_DEFUN([AM_PROG_AS_MOD],
[# By default we simply use the C compiler to build assembly code.
AC_REQUIRE([AC_PROG_CC])
: ${CCAS='$(CC)'}
# Set CCASFLAGS if not already set.
: ${CCASFLAGS='$(CFLAGS)'}
# Set ASCOMPILE if not already set.
if test $CCAS = '$'CC; then
: ${CCASCOMPILE='$(LIBTOOL) --mode=compile $(CCAS) $(AM_ASFLAGS) $(CCASFLAGS) -c'}
else
: ${CCASCOMPILE='$(LIBTOOL) --mode=compile $(CCAS) $(AM_ASFLAGS) $(CCASFLAGS)'}
fi
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)
AC_SUBST(CCASCOMPILE)])
