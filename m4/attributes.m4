# Functions to check for attributes support in compiler

AC_DEFUN([CC_ATTRIBUTE_CONSTRUCTOR], [
	AC_CACHE_CHECK([if compiler supports __attribute__((constructor))],
		[cc_cv_attribute_constructor],
		[AC_COMPILE_IFELSE([
			void ctor() __attribute__((constructor));
			void ctor() { };
			],
			[cc_cv_attribute_constructor=yes],
			[cc_cv_attribute_constructor=no])
		])
	
	if test "x$cc_cv_attribute_constructor" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_CONSTRUCTOR], 1, [Define this if the compiler supports the constructor attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_FORMAT], [
	AC_CACHE_CHECK([if compiler supports __attribute__((format(printf, n, n)))],
		[cc_cv_attribute_format],
		[AC_COMPILE_IFELSE([
			void __attribute__((format(printf, 1, 2))) printflike(const char *fmt, ...) { }
			],
			[cc_cv_attribute_format=yes],
			[cc_cv_attribute_format=no])
		])
	
	if test "x$cc_cv_attribute_format" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_FORMAT], 1, [Define this if the compiler supports the format attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_FORMAT_ARG], [
	AC_CACHE_CHECK([if compiler supports __attribute__((format_arg(printf)))],
		[cc_cv_attribute_format_arg],
		[AC_COMPILE_IFELSE([
			void __attribute__((format_arg(printf, 1))) gettextlike(const char *fmt) { }
			],
			[cc_cv_attribute_format_arg=yes],
			[cc_cv_attribute_format_arg=no])
		])
	
	if test "x$cc_cv_attribute_format_arg" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_FORMAT_ARG], 1, [Define this if the compiler supports the format_arg attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_VISIBILITY], [
	AC_CACHE_CHECK([if compiler supports __attribute__((visibility("...")))],
		[cc_cv_attribute_visibility],
		[AC_COMPILE_IFELSE([
			void __attribute__((visibility("internal"))) internal_function() { }
			void __attribute__((visibility("hidden"))) hidden_function() { }
			void __attribute__((visibility("default"))) external_function() { }
			],
			[cc_cv_attribute_visibility=yes],
			[cc_cv_attribute_visibility=no])
		])
	
	if test "x$cc_cv_attribute_visibility" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_VISIBILITY], 1, [Define this if the compiler supports the visibility attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_FLAG_VISIBILITY], [
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
	
	if test "x$cc_cv_flag_visibility" = "xyes"; then
		AC_DEFINE([SUPPORT_FLAG_VISIBILITY], 1, [Define this if the compiler supports the -fvisibility flag])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_NONNULL], [
	AC_CACHE_CHECK([if compiler supports __attribute__((nonnull()))],
		[cc_cv_attribute_nonnull],
		[AC_COMPILE_IFELSE([
			void some_function(void *foo, void *bar) __attribute__((nonnull()));
			void some_function(void *foo, void *bar) { }
			],
			[cc_cv_attribute_nonnull=yes],
			[cc_cv_attribute_nonnull=no])
		])
	
	if test "x$cc_cv_attribute_nonnull" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_NONNULL], 1, [Define this if the compiler supports the nonnull attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_UNUSED], [
	AC_CACHE_CHECK([if compiler supports __attribute__((unused))],
		[cc_cv_attribute_unused],
		[AC_COMPILE_IFELSE([
			void some_function(void *foo, __attribute__((unused)) void *bar);
			],
			[cc_cv_attribute_unused=yes],
			[cc_cv_attribute_unused=no])
		])
	
	if test "x$cc_cv_attribute_unused" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_UNUSED], 1, [Define this if the compiler supports the unused attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_FUNC_EXPECT], [
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
	
	if test "x$cc_cv_func_expect" = "xyes"; then
		AC_DEFINE([SUPPORT__BUILTIN_EXPECT], 1, [Define this if the compiler supports __builtin_expect() function])
		$1
	else
		true
		$2
	fi
])
