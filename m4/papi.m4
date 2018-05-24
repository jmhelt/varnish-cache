# From: https://github.com/spcl/liblsb/blob/66e7b5fd4b581c2b5391f5af70a320c0ea5e20c0/m4/papi.m4
AC_DEFUN([AX_PAPI],
    [AC_ARG_WITH(papi, AC_HELP_STRING([--with-papi], [compile with PAPI support (ARG can be the path to the location where PAPI was installed)]))
    
    AC_SUBST([PAPI_CFLAGS], [])
    AC_SUBST([PAPI_LDFLAGS], [])
	
    if test x"${with_papi}" == xyes; then
        AC_CHECK_HEADER(papi.h, [
	    AC_DEFINE(HAVE_PAPI, 1, enables PAPI)
            AC_MSG_NOTICE([PAPI support enabled])
    	    AC_SUBST([PAPI_CFLAGS], [])
	    AC_SUBST([PAPI_LDFLAGS], [-lpapi])
	], [AC_MSG_ERROR([PAPI support selected but headers not available!])])
    elif test x"${with_papi}" != x; then
        AC_CHECK_HEADER(${with_papi}/include/papi.h, [
	    AC_DEFINE(HAVE_PAPI, 1, enables PAPI)
            AC_MSG_NOTICE([PAPI support enabled])
	    AC_SUBST([PAPI_CFLAGS], [-I${with_papi}/include])
	    AC_SUBST([PAPI_LDFLAGS], [-lpapi -L${with_papi}/lib -L${with_papi}/lib64])
	], [AC_MSG_ERROR([Can't find the PAPI header files in ${with_papi}/include/])])
    fi
    ]
)

