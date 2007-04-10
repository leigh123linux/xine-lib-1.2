dnl Macros to check the presence of generic (non-typed) symbols.
dnl Copyright (c) 2006-2007 Diego Pettenò <flameeyes@gmail.com>
dnl Copyright (c) 2006-2007 xine project
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
dnl 02110-1301, USA.
dnl
dnl As a special exception, the copyright owners of the
dnl macro gives unlimited permission to copy, distribute and modify the
dnl configure scripts that are the output of Autoconf when processing the
dnl Macro. You need not follow the terms of the GNU General Public
dnl License when using or distributing such scripts, even though portions
dnl of the text of the Macro appear in them. The GNU General Public
dnl License (GPL) does govern all other use of the material that
dnl constitutes the Autoconf Macro.
dnl 
dnl This special exception to the GPL applies to versions of the
dnl Autoconf Macro released by this project. When you make and
dnl distribute a modified version of the Autoconf Macro, you may extend
dnl this special exception to the GPL to apply to your modified version as
dnl well.

AC_DEFUN([CC_CHECK_CFLAGS], [
  AC_CACHE_CHECK([if $CC supports $1 flag],
    AS_TR_SH([cc_cv_cflags_$1]),
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $1"
     AC_COMPILE_IFELSE([int a;],
       [eval "AS_TR_SH([cc_cv_cflags_$1])='yes'"],
       [eval "AS_TR_SH([cc_cv_cflags_$1])="])
     CFLAGS="$ac_save_CFLAGS"
    ])

  if eval test [x$]AS_TR_SH([cc_cv_cflags_$1]) = xyes; then
    ifelse([$2], , [:], [$2])
  else
    ifelse([$3], , [:], [$3])
  fi
])

dnl Check for a -Werror flag or equivalent. -Werror is the GCC
dnl and ICC flag that tells the compiler to treat all the warnings
dnl as fatal. We usually need this option to make sure that some
dnl constructs (like attributes) are not simply ignored.
dnl
dnl Other compilers don't support -Werror per se, but they support
dnl an equivalent flag:
dnl  - Sun Studio compiler supports -errwarn=%all
AC_DEFUN([CC_CHECK_WERROR], [
  AC_CACHE_VAL([cc_cv_werror],
    [CC_CHECK_CFLAGS([-Werror], [cc_cv_werror=-Werror],
      [CC_CHECK_CFLAGS([-errwarn=%all], [cc_cv_werror=-errwarn=%all])])
    ])
])

AC_DEFUN([CC_ATTRIBUTE_CONSTRUCTOR], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((constructor))],
		[cc_cv_attribute_constructor],
		[AC_COMPILE_IFELSE([
			void ctor() __attribute__((constructor));
			void ctor() { int a; };
			],
			[cc_cv_attribute_constructor=yes],
			[cc_cv_attribute_constructor=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_constructor" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_CONSTRUCTOR], 1, [Define this if the compiler supports the constructor attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_FORMAT], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((format(printf, n, n)))],
		[cc_cv_attribute_format],
		[AC_COMPILE_IFELSE([
			void __attribute__((format(printf, 1, 2))) printflike(const char *fmt, ...) { fmt = (void *)0; }
			],
			[cc_cv_attribute_format=yes],
			[cc_cv_attribute_format=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_format" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_FORMAT], 1, [Define this if the compiler supports the format attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_FORMAT_ARG], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((format_arg(printf)))],
		[cc_cv_attribute_format_arg],
		[AC_COMPILE_IFELSE([
			char *__attribute__((format_arg(1))) gettextlike(const char *fmt) { fmt = (void *)0; }
			],
			[cc_cv_attribute_format_arg=yes],
			[cc_cv_attribute_format_arg=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_format_arg" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_FORMAT_ARG], 1, [Define this if the compiler supports the format_arg attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_VISIBILITY], [
  AC_REQUIRE([CC_CHECK_WERROR])
  AC_CACHE_CHECK([if $CC supports __attribute__((visibility("$1")))],
    AS_TR_SH([cc_cv_attribute_visibility_$1]),
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $cc_cv_werror"
     AC_COMPILE_IFELSE([void __attribute__((visibility("$1"))) $1_function() { }],
       [eval "AS_TR_SH([cc_cv_attribute_visibility_$1])='yes'"],
       [eval "AS_TR_SH([cc_cv_attribute_visibility_$1])='no'"])
     CFLAGS="$ac_save_CFLAGS"
    ])

  if eval test [x$]AS_TR_SH([cc_cv_attribute_visibility_$1]) = xyes; then
    AC_DEFINE(AS_TR_CPP([SUPPORT_ATTRIBUTE_VISIBILITY_$1]), 1, [Define this if the compiler supports __attribute__((visibility("$1")))])
    ifelse([$2], , [:], [$2])
  else
    ifelse([$3], , [:], [$3])
  fi
])

AC_DEFUN([CC_FLAG_VISIBILITY], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports -fvisibility=hidden],
		[cc_cv_flag_visibility],
		[
		save_CFLAGS=$CFLAGS
		CFLAGS="$CFLAGS -fvisibility=hidden"
		AC_COMPILE_IFELSE([int a;],
			[cc_cv_flag_visibility=yes],
			[cc_cv_flag_visibility=no])
		CFLAGS="$save_CFLAGS"
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_flag_visibility" = "xyes"; then
		AC_DEFINE([SUPPORT_FLAG_VISIBILITY], 1, [Define this if the compiler supports the -fvisibility flag])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_NONNULL], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((nonnull()))],
		[cc_cv_attribute_nonnull],
		[AC_COMPILE_IFELSE([
			void some_function(void *foo, void *bar) __attribute__((nonnull()));
			void some_function(void *foo, void *bar) { foo = (void *)0; bar = (void *)0; }
			],
			[cc_cv_attribute_nonnull=yes],
			[cc_cv_attribute_nonnull=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_nonnull" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_NONNULL], 1, [Define this if the compiler supports the nonnull attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_UNUSED], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((unused))],
		[cc_cv_attribute_unused],
		[AC_COMPILE_IFELSE([
			void some_function(void *foo, __attribute__((unused)) void *bar);
			],
			[cc_cv_attribute_unused=yes],
			[cc_cv_attribute_unused=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_unused" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_UNUSED], 1, [Define this if the compiler supports the unused attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_FUNC_EXPECT], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler has __builtin_expect function],
		[cc_cv_func_expect],
		[AC_COMPILE_IFELSE([
			int some_function()
			{
				int a = 3;
				return (int)__builtin_expect(a, 3);
			}
			],
			[cc_cv_func_expect=yes],
			[cc_cv_func_expect=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_func_expect" = "xyes"; then
		AC_DEFINE([SUPPORT__BUILTIN_EXPECT], 1, [Define this if the compiler supports __builtin_expect() function])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_SENTINEL], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((sentinel))],
		[cc_cv_attribute_sentinel],
		[AC_COMPILE_IFELSE([
			void some_function(void *foo, ...) __attribute__((sentinel));
			],
			[cc_cv_attribute_sentinel=yes],
			[cc_cv_attribute_sentinel=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_sentinel" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_SENTINEL], 1, [Define this if the compiler supports the sentinel attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_ALIAS], [
	AC_REQUIRE([CC_CHECK_WERROR])
	ac_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $cc_cv_werror"
	AC_CACHE_CHECK([if compiler supports __attribute__((weak, alias))],
		[cc_cv_attribute_alias],
		[AC_COMPILE_IFELSE([
			void other_function(void *foo) { }
			void some_function(void *foo) __attribute__((weak, alias("other_function")));
			],
			[cc_cv_attribute_alias=yes],
			[cc_cv_attribute_alias=no])
		])
	CFLAGS="$ac_save_CFLAGS"
	
	if test "x$cc_cv_attribute_alias" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_ALIAS], 1, [Define this if the compiler supports the alias attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_ALIGNED], [
  AC_REQUIRE([CC_CHECK_WERROR])
  AC_CACHE_CHECK([highest __attribute__ ((aligned ())) supported],
    [cc_cv_attribute_aligned],
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $cc_cv_werror"
     for cc_attribute_align_try in 64 32 16 8 4 2; do
        AC_COMPILE_IFELSE([
          int main() {
            static char c __attribute__ ((aligned($cc_attribute_align_try))) = 0;
            return c;
          }], [cc_cv_attribute_aligned=$cc_attribute_align_try; break])
     done
     CFLAGS="$ac_save_CFLAGS"
  ])

  if test "x$cc_cv_attribute_aligned" != "x"; then
     AC_DEFINE_UNQUOTED([ATTRIBUTE_ALIGNED_MAX], [$cc_cv_attribute_aligned],
       [Define the highest alignment supported])
  fi
])

AC_DEFUN([CC_ATTRIBUTE_PACKED], [
  AC_REQUIRE([CC_CHECK_WERROR])
  AC_CACHE_CHECK([if $CC supports __attribute__((packed))],
    [cc_cv_attribute_packed],
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $cc_cv_werror"
     AC_COMPILE_IFELSE([struct { char a; short b; int c; } __attribute__((packed)) foo;],
       [cc_cv_attribute_packed=yes],
       [cc_cv_attribute_packed=no])
     CFLAGS="$ac_save_CFLAGS"
    ])

  if test x$cc_cv_attribute_packed = xyes; then
    AC_DEFINE([SUPPORT_ATTRIBUTE_PACKED], 1, [Define this if the compiler supports __attribute__((packed))])
    ifelse([$1], , [:], [$1])
  else
    ifelse([$2], , [:], [$2])
  fi
])
