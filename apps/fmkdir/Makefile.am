
# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = fmkdir

INCLUDES = -I$(top_builddir)/lib -I$(top_srcdir)/lib                                               \
	                                \
	                                   \
	-I. 						\
	-I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src	\
	-I$(top_builddir)				\
        -I$(includedir)                                 \
	@GLIB_CFLAGS@ \
	@CFLAGS@

fmkdir_SOURCES = fmkdir.cpp
fmkdir_LDADD = -lpopt @LIBFERRIS_LA@ 
fmkdir_LDFLAGS = $(GLIB_LIBS) 

