# $Id: Makefile.am,v 1.65 2015/05/09 04:00:25 nkbj Exp $

SUBDIRS = po icons

if MAKE_EXTRAS
  bin_PROGRAMS = ufraw-batch dcraw nikon-curve
else
  bin_PROGRAMS = ufraw-batch
endif

if MAKE_GTK
  gtk_PROGRAMS = ufraw
  gtkdir = $(bindir)
endif

if MAKE_GIMP
  gimpbin_PROGRAMS = ufraw-gimp
  gimpbindir = $(GIMP_LIBDIR)/plug-ins
endif

POD2MAN = @POD2MAN@
man_MANS = ufraw.1

noinst_LIBRARIES = libufraw.a

MAINTAINERCLEANFILES = ufraw.1

CLEANFILES = ufraw.schemas ufraw_icon.opc ufraw-setup.bmp

if INSTALL_MIME
  app_DATA = ufraw.desktop
  appdir = $(datadir)/applications

  appdata_DATA = ufraw.appdata.xml
  appdatadir = $(datadir)/appdata

#  Not needed since it is contained in shared-mime-info 0.21
#  mime_DATA = ufraw-mime.xml
#  mimedir = $(datadir)/mime/packages

  schemas_DATA = ufraw.schemas
  schemasdir = $(datadir)/gconf/schemas
endif

if UFRAW_WIN32
  UFRAW_ICON = ufraw_icon.opc
else
  UFRAW_ICON =
endif

EXTRA_DIST = ufraw.desktop ufraw.appdata.xml ufraw_icon.ico ufraw_icon.rc \
    ufraw-mime.xml generate_schemas.sh ac_openmp.m4 Doxyfile \
    ufraw-setup.jpg autogen.sh mkinstalldirs MANIFEST ufraw.pod ufraw.1

AM_CPPFLAGS = $(UFRAW_CPPFLAGS) -DDCRAW_NOMAIN \
		-DUFRAW_LOCALEDIR=\"$(datadir)/locale\"
LDADD = libufraw.a $(UFRAW_ICON) $(UFRAW_LDADD)
LINK = $(CXXLINK)

if MAKE_GTK
  libufraw_a_SOURCES = \
    dcraw.cc ufraw_ufraw.c ufraw_routines.c ufraw_colorspaces.c \
    ufraw_colorspaces.h ufraw_developer.c ufraw_conf.c ufraw_writer.c \
    ufraw_embedded.c ufraw_message.c ufraw.h ufobject.cc ufobject.h \
    ufraw_settings.cc ufraw_lensfun.cc wb_presets.c dcraw_api.cc dcraw_api.h \
    dcraw_indi.c dcraw.h nikon_curve.c nikon_curve.h uf_progress.h \
    uf_glib.h uf_gtk.cc uf_gtk.h ufraw_exiv2.cc iccjpeg.c iccjpeg.h \
    ufraw_preview.c ufraw_saver.c ufraw_delete.c ufraw_chooser.c \
    ufraw_icons.c icons/ufraw_icons.h curveeditor_widget.c \
    curveeditor_widget.h ufraw_lens_ui.c ufraw_ui.h
else
  libufraw_a_SOURCES = \
    dcraw.cc ufraw_ufraw.c ufraw_routines.c ufraw_colorspaces.c \
    ufraw_colorspaces.h ufraw_developer.c ufraw_conf.c ufraw_writer.c \
    ufraw_embedded.c ufraw_message.c ufraw.h ufobject.cc ufobject.h \
    ufraw_settings.cc ufraw_lensfun.cc wb_presets.c dcraw_api.cc dcraw_api.h \
    dcraw_indi.c dcraw.h nikon_curve.c nikon_curve.h uf_progress.h \
    uf_glib.h ufraw_exiv2.cc iccjpeg.c iccjpeg.h
endif

ufraw_batch_LINK = $(CXXLINK) @CONSOLE@

if MAKE_GTK
  ufraw_SOURCES = ufraw.c
  ufraw_CPPFLAGS = $(UFRAW_CPPFLAGS)
  ufraw_LDADD = $(LDADD) $(GTK_LIBS)
endif

ufraw_batch_SOURCES = ufraw-batch.c
if MAKE_GIMP
  ufraw_gimp_SOURCES = ufraw-gimp.c
  ufraw_gimp_CPPFLAGS = $(AM_CPPFLAGS) $(GIMP_CFLAGS) 
  ufraw_gimp_LDADD = $(LDADD) $(GIMP_LIBS) $(GTK_LIBS)
endif

if MAKE_EXTRAS
  dcraw_SOURCES = dcraw.cc
  dcraw_CPPFLAGS = $(UFRAW_CPPFLAGS)
  dcraw_LDFLAGS = @CONSOLE@
  dcraw_LDADD = $(UFRAW_LDADD)

  nikon_curve_SOURCES = nikon_curve.c
  nikon_curve_CPPFLAGS = $(UFRAW_CPPFLAGS) -D_STAND_ALONE_
  nikon_curve_LDFLAGS = @CONSOLE@
  nikon_curve_LDADD = $(UFRAW_LDADD)
  nikon_curve_LINK = $(CXXLINK) @CONSOLE@
endif

#ufraw_icon.ico: icons/ufraw.png
#	{ pngtopnm $^ | pnmquant 256; pngtopnm -alpha $^; } | ppmtowinicon -andpgms -output $@ - -

ufraw_icon.opc: ufraw_icon.rc ufraw_icon.ico
	$(WINDRES) --include-dir=$(srcdir) --input $< --output $@

.pod.1:
	$(POD2MAN) --section 1 --center "" --release UFRAW $< $@

ufraw.schemas: generate_schemas.sh
	$(srcdir)/generate_schemas.sh $(prefix) $@

install-windows: windows-installer
	$(WINE) ./ufraw-$(VERSION)-setup.exe

windows-installer: ufraw-$(VERSION)-setup.exe

ufraw-$(VERSION)-setup.exe: ufraw-setup.iss ufraw.exe ufraw-batch.exe \
    ufraw-gimp.exe ufraw-setup.bmp ufraw_icon.bmp ufraw_icon.ico
	strip ufraw.exe ufraw-batch.exe ufraw-gimp.exe
	$(WINE) $(ISCC) ufraw-setup.iss

ufraw-setup.bmp: ufraw-setup.jpg
	montage -geometry +0+0 $< $@

ufraw_icon.bmp: icons/ufraw.png
	montage -geometry +0+0 $< $@
