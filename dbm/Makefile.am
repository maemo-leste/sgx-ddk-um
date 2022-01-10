lib_LTLIBRARIES = libdbm.la

libdbm_la_SOURCES = dbm_helpers.c dbm.c
libdbm_la_CFLAGS = $(DRM_CFLAGS) $(GBM_CFLAGS)
libdbm_la_LDFLAGS = -version-info $(version_info) $(DRM_LIBS)

LIBDBM = libdbm.so
LIBDBM_VER = $(LIBDBM).$(ddk_version)
# libtool supports up to 5 digits for release, so we need to hack around
install-exec-hook:
	$(am__cd) $(DESTDIR)/$(libdir) && \
		  $(am__mv) $(LIBDBM_VER).0 $(LIBDBM_VER).$(ddk_release)
	$(RM) $(DESTDIR)/$(libdir)/$(LIBDBM)
	$(am__cd) $(DESTDIR)/$(libdir) && \
		  $(LN_S) $(LIBDBM_VER).$(ddk_release) $(LIBDBM)
	$(RM) $(DESTDIR)/$(libdir)/$(LIBDBM).1
	$(am__cd) $(DESTDIR)/$(libdir) && \
		  $(LN_S) $(LIBDBM_VER).$(ddk_release) $(LIBDBM).1

MAINTAINERCLEANFILES = Makefile.in
