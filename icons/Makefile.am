# $Id: Makefile.am,v 1.1 2006/11/13 00:30:41 udifuchs Exp $

noinst_HEADERS = ufraw_icons.h
icon_DATA = ufraw.png
icondir = $(datadir)/pixmaps

UFRAW_ICONS = ufraw.png exposure-24.png film-24.png digital-24.png \
    restore-lch-24.png restore-hsv-24.png
EXTRA_DIST = $(UFRAW_ICONS) exposure.svg film.svg digital.svg \
    restore-lch.svg restore-hsv.svg

ufraw_icons.h: $(UFRAW_ICONS)
	gdk-pixbuf-csource --build-list  ufraw_icon ufraw.png \
		ufraw_exposure_24 exposure-24.png \
		ufraw_film_24 film-24.png \
		ufraw_digital_24 digital-24.png \
		ufraw_restore_lch_24 restore-lch-24.png \
		ufraw_restore_hsv_24 restore-hsv-24.png \
		> $@