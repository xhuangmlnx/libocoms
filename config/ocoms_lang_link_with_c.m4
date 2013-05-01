dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2006      Los Alamos National Security, LLC.  All rights
dnl                         reserved. 
dnl Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
dnl Copyright (c) 2011-2013 UT-Battelle, LLC. All rights reserved.
dnl Copyright (C) 2013      Mellanox Technologies Ltd. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# OCOMS_LANG_LINK_WITH_C(language)
# -------------------------------
# Try to link a small test program against a C object file to make
# sure the compiler for the given language is compatible with the C
# compiler.
AC_DEFUN([OCOMS_LANG_LINK_WITH_C], [
  AS_VAR_PUSHDEF([lang_var], [ocoms_cv_c_link_$1])

  AC_CACHE_CHECK([if C and $1 are link compatible],
    lang_var,
    [m4_if([$1], [Fortran], 
       [m4_define([ocoms_lang_link_with_c_fortran], 1)],
       [m4_if([$1], [Fortran 77],
          [m4_define([ocoms_lang_link_with_c_fortran], 1)],
          [m4_define([ocoms_lang_link_with_c_fortran], 0)])])

     # Write out C part
     AC_LANG_PUSH(C)
     rm -f conftest_c.$ac_ext
      cat > conftest_c.$ac_ext << EOF
int testfunc(int a);
int testfunc(int a) { return a; }
EOF

     # Now compile both parts
     OCOMS_LOG_COMMAND([$CC -c $CFLAGS $CPPFLAGS conftest_c.$ac_ext],
       [AC_LANG_PUSH($1)
        ocoms_lang_link_with_c_libs="$LIBS"
        LIBS="conftest_c.o $LIBS"
        m4_if(ocoms_lang_link_with_c_fortran, 1, 
          [AC_LINK_IFELSE([AC_LANG_PROGRAM([], [
       external testfunc
       call testfunc(1)
	 ])],
     [AS_VAR_SET(lang_var, ["yes"])], [AS_VAR_SET(lang_var, ["no"])])],
     [AC_LINK_IFELSE([AC_LANG_PROGRAM([
#if defined(c_plusplus) || defined(__cplusplus)
extern "C" int testfunc(int);
#else
extern int testfunc(int);
#endif
], 
             [return testfunc(0);])],
             [AS_VAR_SET(lang_var, ["yes"])], [AS_VAR_SET(lang_var, ["no"])])])
        LIBS="$ocoms_lang_link_with_c_libs"
        AC_LANG_POP($1)],
       [AS_VAR_SET(lang_var, ["no"])])
     rm -f conftest_c.$ac_ext
     AC_LANG_POP(C)])

  AS_VAR_IF(lang_var, [yes], [$2], [$3])
  AS_VAR_POPDEF([lang_var])dnl
])
