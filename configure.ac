AC_PREREQ(2.60)

m4_define([ddk_version_major], [1])
m4_define([ddk_version_minor], [17])
m4_define([ddk_version_release], [4948957])

m4_define([lt_cur], m4_eval(ddk_version_major + ddk_version_minor))
m4_define([project_version], [ddk_version_major.ddk_version_minor.ddk_version_release])

m4_define([lt_cur], [m4_eval(ddk_version_major + ddk_version_minor)])
m4_define([lt_rev], [ddk_version_release])
m4_define([lt_age], [ddk_version_minor])
version_info="lt_cur:lt_rev:lt_age"
AC_SUBST([version_info])

AC_INIT([sgx-ddk-um], [project_version], [https://github.com/maemo-leste/sgx-ddk-um])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_OUTPUT([
	Makefile
	pvrsrvinit/Makefile]
)
