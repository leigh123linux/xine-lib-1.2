#!/bin/sh

# Making releases:
#   1. Increment XINE_VERSION_SUB
#   2. Remove .cvsversion before running make dist
#   3. Adjust the values of XINE_LT_CURRENT, XINE_LT_REVISION, and XINE_LT_AGE
#      according to the following rules:
#
#          If the interface is totally unchanged from the previous release,
#          increment XINE_LT_REVISION by one.  Otherwise:
#            1. XINE_LT_REVISION=0
#            2. Increment XINE_LT_CURRENT by one.
#            3. If any interfaces have been ADDED since the last release,
#               increment XINE_LT_AGE by one.  If any interfaces have been
#               REMOVED or incompatibly changed, XINE_LT_AGE=0
#
# Regarding libtool versioning, here are some details, but see the info page for
# libtool for the whole story.  The most important thing to keep in mind is that
# the libtool version numbers DO NOT MATCH the xine-lib version numbers, and you
# should NEVER try to make them match.
#
#   XINE_LT_CURRENT     the current API version
#   XINE_LT_REVISION    an internal revision number that is increased when the
#                       API does not change in any way
#   XINE_LT_AGE         the number of previous API versions still supported by
#                       this version

XINE_VERSION_MAJOR=1
XINE_VERSION_MINOR=1
XINE_VERSION_SUB=90

XINE_LT_CURRENT=19
XINE_LT_REVISION=0
XINE_LT_AGE=17

test -f "`dirname $0`/.cvsversion" && XINE_VERSION_SUFFIX="hg"
XINE_VERSION_SPEC="${XINE_VERSION_MAJOR}.${XINE_VERSION_MINOR}.${XINE_VERSION_SUB}${XINE_VERSION_SUFFIX}"

####
####    You should not need to touch anything beyond this point
####

echo "m4_define([XINE_VERSION_MAJOR],  [${XINE_VERSION_MAJOR}])dnl"
echo "m4_define([XINE_VERSION_MINOR],  [${XINE_VERSION_MINOR}])dnl"
echo "m4_define([XINE_VERSION_SUB],    [${XINE_VERSION_SUB}])dnl"
echo "m4_define([XINE_VERSION_SUFFIX], [${XINE_VERSION_SUFFIX}])dnl"
echo "m4_define([XINE_VERSION_SPEC],   [${XINE_VERSION_SPEC}])dnl"
echo "m4_define([__XINE_LT_CURRENT],   [${XINE_LT_CURRENT}])dnl"
echo "m4_define([__XINE_LT_REVISION],  [${XINE_LT_REVISION}])dnl"
echo "m4_define([__XINE_LT_AGE],       [${XINE_LT_AGE}])dnl"
