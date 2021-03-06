AM_CPPFLAGS = \
    -I$(top_builddir)/config \
    -I$(top_srcdir)	\
    -L$(top_srcdir)/src \
    -DSUSHI_LIBDIR=\"$(libdir)\" \
    -DSUSHI_DATADIR=\"$(datadir)\" \
    -DSUSHI_PKGDATADIR=\"$(pkgdatadir)\" \
    -DSUSHI_TOP_SRCDIR=\"$(abs_top_srcdir)\" \
    -DSUSHI_BUILDDIR=\"$(abs_top_builddir)\"

girdir = $(pkgdatadir)/gir-1.0
typelibdir = $(pkglibdir)/girepository-1.0

BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
pkglib_LTLIBRARIES =
INTROSPECTION_GIRS =
gir_DATA =
typelib_DATA =

# convenience command for doing Makefile variable substitutions in non-Makefile
# files (scripts, service files, etc.)
do_subst = sed -e 's|@abs_top_srcdir[@]|$(abs_top_srcdir)|g' \
               -e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
               -e 's|@localedir[@]|$(localedir)|g' \
               -e 's|@bindir[@]|$(bindir)|g' \
               -e 's|@libdir[@]|$(libdir)|g' \
               -e 's|@libexecdir[@]|$(libexecdir)|g' \
               -e 's|@pkglibdir[@]|$(pkglibdir)|g' \
               -e 's|@pkgdatadir[@]|$(pkgdatadir)|g'

include $(INTROSPECTION_MAKEFILE)

include Makefile-js.am
include Makefile-sushi.am

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/org.gnome.Sushi.gresource.xml)
sushi-resources.c: org.gnome.Sushi.gresource.xml $(resource_files)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name sushi $(srcdir)/org.gnome.Sushi.gresource.xml
sushi-resources.h: org.gnome.Sushi.gresource.xml $(resource_files)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --c-name sushi $(srcdir)/org.gnome.Sushi.gresource.xml
EXTRA_DIST += org.gnome.Sushi.gresource.xml $(resource_files)
BUILT_SOURCES += \
	sushi-resources.h \
	sushi-resources.c \
	$(NULL)

gir_DATA += $(INTROSPECTION_GIRS)
typelib_DATA += $(gir_DATA:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

libexec_PROGRAMS = sushi-start

sushi_start_SOURCES = \
	main.c \
	sushi-resources.h \
	sushi-resources.c \
	$(NULL)

sushi_start_LDADD = \
    libsushi-1.0.la \
    $(SUSHI_LIBS)
sushi_start_CPPFLAGS = \
    -DGST_USE_UNSTABLE_API \
    $(SUSHI_CFLAGS) \
    $(AM_CPPFLAGS) \
    -DLOCALEDIR=\"$(datadir)/locale\"

bin_SCRIPTS = sushi

sushi: sushi.in
	$(AM_V_GEN) $(do_subst) $< > $@
	chmod +x $@

CLEANFILES += sushi
EXTRA_DIST += sushi.in

-include $(top_srcdir)/git.mk
