/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_delete.c - The GUI for choosing what files to delete.
 * Copyright 2004-2007 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation. You should have received
 * a copy of the license along with this program.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include <gtk/gtk.h>
#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#endif
#if !GLIB_CHECK_VERSION(2,10,0)
#include <unistd.h> // For unlink
#endif
#include <glib/gi18n.h>
#include "ufraw.h"

// Response can be any unique positive integer
#define UFRAW_RESPONSE_DELETE_SELECTED 1
#define UFRAW_RESPONSE_DELETE_ALL 2

long ufraw_delete(void *widget, ufraw_data *uf)
{
    if ( !g_file_test(uf->filename, G_FILE_TEST_EXISTS) ) {
	char *ufile = g_filename_to_utf8(uf->filename, -1, NULL, NULL, NULL);
	ufraw_message(UFRAW_ERROR, _("Raw file '%s' missing."), ufile);
	g_free(ufile);
	return UFRAW_ERROR;
    }
    GtkDialog *dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(
	    _("Delete raw file"), GTK_WINDOW(gtk_widget_get_toplevel(widget)),
	    GTK_DIALOG_DESTROY_WITH_PARENT,
	    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	    _("_Delete selected"), UFRAW_RESPONSE_DELETE_SELECTED,
	    _("Delete _All"), UFRAW_RESPONSE_DELETE_ALL, NULL));
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CANCEL);

    GtkBox *box = GTK_BOX(dialog->vbox);
    GtkWidget *label = gtk_label_new(_("Select files to delete"));
    gtk_box_pack_start(box, label, FALSE, FALSE, 0);

    char *path = g_path_get_dirname(uf->filename);
    GDir *dir = g_dir_open(path, 0, NULL);
    if ( dir==NULL ) {
	char *upath = g_filename_to_utf8(path, -1, NULL, NULL, NULL);
	ufraw_message(UFRAW_ERROR, _("Error reading directory '%s'."), upath);
	g_free(upath);
	g_free(path);
	return UFRAW_ERROR;
    }
    g_free(path);

    char *dirname = g_path_get_dirname(uf->filename);
    char *base = g_path_get_basename(uf->filename);
    if ( strcmp(base, uf->filename)==0 ) {
	// Handle empty dirname
	g_free(dirname);
	dirname = g_strdup("");
    }
    char *rawBase = uf_file_set_type(base, ".");
    int rawBaseLen = strlen(rawBase);
    GList *buttonList = NULL;
    GList *fileList = NULL;

    // Check button for raw file
    char *ufile = g_filename_to_utf8(uf->filename, -1, NULL, NULL, NULL);
    GtkWidget *button = gtk_check_button_new_with_label(ufile);
    buttonList = g_list_append(buttonList, button);
    g_free(ufile);
    gtk_box_pack_start(box, button, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    fileList = g_list_append(fileList, g_strdup(uf->filename));

    // Check buttons for associated files
    const char *file;
    while ( (file=g_dir_read_name(dir)) != NULL ) {
	if ( strncmp(file, rawBase, rawBaseLen)==0 &&
	     strcmp(file, base)!=0 ) {
	    char *filename = g_build_filename(dirname, file, NULL);
	    char *ufile = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	    button = gtk_check_button_new_with_label(ufile);
	    buttonList = g_list_append(buttonList, button);
	    g_free(ufile);
	    gtk_box_pack_start(box, button, FALSE, FALSE, 0);
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
	    fileList = g_list_append(fileList, filename);
	}
    }
    g_dir_close(dir);
    gtk_widget_show_all(GTK_WIDGET(dialog));
    long response = gtk_dialog_run(dialog);

    long status = UFRAW_CANCEL;
    /* Delete files as needed */
    while ( fileList!=NULL ) {
	if ( response==UFRAW_RESPONSE_DELETE_ALL ||
	     ( response==UFRAW_RESPONSE_DELETE_SELECTED &&
	       gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(buttonList->data)) ) ) {
	    if ( g_unlink(fileList->data)!=0 ) {
		char *ufile = g_filename_to_utf8(fileList->data,
			-1, NULL, NULL, NULL);
		ufraw_message(UFRAW_ERROR, _("Error deleting '%s'"), ufile);
		g_free(ufile);
	    } else if (strcmp(fileList->data, uf->filename)==0 ) {
		/* Success means deleting the raw file */
		status = UFRAW_SUCCESS;
	    }
	}
	g_free(fileList->data);
	fileList = g_list_next(fileList);
	buttonList = g_list_next(buttonList);
    }
    g_list_free(fileList);
    g_list_free(buttonList);
    g_free(base);
    g_free(dirname);
    gtk_widget_destroy(GTK_WIDGET(dialog));
    return status;
}