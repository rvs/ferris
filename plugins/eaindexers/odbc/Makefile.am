
plugindir = $(libdir)/ferris/plugins/eaindexers

plugin_LTLIBRARIES = libeaindexodbc.la libeaindexodbc_factory.la
libeaindexodbc_la_SOURCES = libeaindexodbc.cpp
libeaindexodbc_factory_la_SOURCES = libeaindexodbc_factory.cpp

INCLUDES = @DTL_CFLAGS@ @CFLAGS@

libeaindexodbc_la_LDFLAGS = @GLIB_LIBS@    \
	@DTL_LIBS@                       \
	@LIBFERRIS_PLUGIN_LT_LINE@
libeaindexodbc_factory_la_LDFLAGS = @LIBFERRIS_PLUGIN_LT_LINE@

