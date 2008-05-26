/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_lens_ui.c - User interface for interaction with lensfun,
 *                   a lens defect correction library.
 * Copyright 2007 by Andrew Zabolotny
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation. You should have received
 * a copy of the license along with this program.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "ufraw.h"
#include "ufraw_ui.h"
#include "uf_gtk.h"

#ifdef HAVE_LENSFUN

static void delete_children (GtkWidget *widget, gpointer data)
{
    (void)data;
    gtk_widget_destroy (widget);
}

/**
 * Add a labeled GtkComboBoxEntry to a table or to a box.
 */
static GtkComboBoxEntry *combo_entry_text (
    GtkWidget *container, guint x, guint y, gchar *lbl, gchar *tip)
{
    GtkWidget *label, *combo;

    label = gtk_label_new(lbl);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    if (GTK_IS_TABLE (container))
        gtk_table_attach (GTK_TABLE (container), label, x, x + 1, y, y + 1, 0, 0, 2, 0);
    else if (GTK_IS_BOX (container))
        gtk_box_pack_start (GTK_BOX (container), label, FALSE, FALSE, 2);
    uf_widget_set_tooltip (label, tip);

    combo = gtk_combo_box_entry_new_text ();
    if (GTK_IS_TABLE (container))
        gtk_table_attach (GTK_TABLE (container), combo, x+1, x+2, y, y+1, 0, 0, 2, 0);
    else if (GTK_IS_BOX (container))
        gtk_box_pack_start (GTK_BOX (container), combo, FALSE, FALSE, 2);
    uf_widget_set_tooltip (combo, tip);

    return GTK_COMBO_BOX_ENTRY (combo);
}

/* simple function to compute the floating-point precision
   which is enough for "normal use". The criteria is to have
   about 3 leading digits after the initial zeros.  */
static int precision (double x, double adj)
{
    x *= adj;
    if (x < 1.0)
        if (x < 0.1)
            if (x < 0.01)
                return 5;
            else
                return 4;
        else
            return 3;
    else
        if (x < 100.0)
            if (x < 10.0)
                return 2;
            else
                return 1;
        else
            return 0;
}

static GtkComboBoxEntry *combo_entry_numeric (
    GtkWidget *container, guint x, guint y, gchar *lbl, gchar *tip,
    gdouble val, gdouble precadj, gdouble *values, int nvalues)
{
    int i;
    char txt [30];
    GtkEntry *entry;
    GtkComboBoxEntry *combo;

    combo = combo_entry_text (container, x, y, lbl, tip);
    entry = GTK_ENTRY (GTK_BIN (combo)->child);

    gtk_entry_set_width_chars (entry, 4);

    snprintf (txt, sizeof (txt), "%.*f", precision (val, precadj), val);
    gtk_entry_set_text (entry, txt);

    for (i = 0; i < nvalues; i++)
    {
        gdouble v = values [i];
        snprintf (txt, sizeof (txt), "%.*f", precision (v, precadj), v);
        gtk_combo_box_append_text (GTK_COMBO_BOX (combo), txt);
    }

    return combo;
}

static GtkComboBoxEntry *combo_entry_numeric_log (
    GtkWidget *container, guint x, guint y, gchar *lbl, gchar *tip,
    gdouble val, gdouble min, gdouble max, gdouble step, gdouble precadj)
{
    int phase, nvalues;
    gdouble *values = NULL;
    for (phase = 0; phase < 2; phase++)
    {
        nvalues = 0;
        gboolean done = FALSE;
        gdouble v = min;
        while (!done)
        {
            if (v > max)
            {
                v = max;
                done = TRUE;
            }

            if (values)
                values [nvalues++] = v;
            else
                nvalues++;

            v *= step;
        }
        if (!values)
            values = g_new (gdouble, nvalues);
    }

    GtkComboBoxEntry *cbe = combo_entry_numeric (
        container, x, y, lbl, tip, val, precadj, values, nvalues);
    g_free (values);
    return cbe;
}

static void camera_set (preview_data *data, const lfCamera *cam)
{
    gchar *fm;
    const char *maker, *model, *variant;

    lf_camera_copy (CFG->camera, cam);
    if (!cam)
    {
        gtk_entry_set_text(GTK_ENTRY(data->CameraModel), "");
        uf_widget_set_tooltip(data->CameraModel, NULL);
        return;
    }

    if (cam->Model)
    {
        if (cam->Maker)
            fm = g_strdup_printf ("%s | %s", cam->Maker, cam->Model);
        else
            fm = g_strdup_printf ("%s", cam->Model);
        gtk_entry_set_text(GTK_ENTRY(data->CameraModel), fm);
        g_free (fm);
    }

    maker = lf_mlstr_get (cam->Maker);
    model = lf_mlstr_get (cam->Model);
    variant = lf_mlstr_get (cam->Variant);

    char _variant [100];
    if (variant)
        snprintf (_variant, sizeof (_variant), " (%s)", variant);
    else
        _variant [0] = 0;

    fm = g_strdup_printf (_("Maker:\t\t%s\n"
                            "Model:\t\t%s%s\n"
                            "Mount:\t\t%s\n"
                            "Crop factor:\t%.1f"),
                          maker, model, _variant,
                          cam->Mount, cam->CropFactor);
    uf_widget_set_tooltip(data->CameraModel, fm);
    g_free (fm);
}

static void camera_menu_select (
    GtkMenuItem *menuitem, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;
    camera_set (data, (lfCamera *)g_object_get_data(G_OBJECT(menuitem), "lfCamera"));
}

static void camera_menu_fill (
    preview_data *data, const lfCamera *const *camlist)
{
    unsigned i;
    GPtrArray *makers, *submenus;

    if (data->CameraMenu)
    {
        gtk_widget_destroy (data->CameraMenu);
        data->CameraMenu = NULL;
    }

    /* Count all existing camera makers and create a sorted list */
    makers = g_ptr_array_new ();
    submenus = g_ptr_array_new ();
    for (i = 0; camlist [i]; i++)
    {
        GtkWidget *submenu, *item;
        const char *m = lf_mlstr_get (camlist [i]->Maker);
        int idx = ptr_array_find_sorted (makers, m, (GCompareFunc)g_utf8_collate);
        if (idx < 0)
        {
            /* No such maker yet, insert it into the array */
            idx = ptr_array_insert_sorted (makers, m, (GCompareFunc)g_utf8_collate);
            /* Create a submenu for cameras by this maker */
            submenu = gtk_menu_new ();
            ptr_array_insert_index (submenus, submenu, idx);
        }

        submenu = g_ptr_array_index (submenus, idx);
        /* Append current camera name to the submenu */
        m = lf_mlstr_get (camlist [i]->Model);
        if (!camlist [i]->Variant)
            item = gtk_menu_item_new_with_label (m);
        else
        {
            gchar *fm = g_strdup_printf ("%s (%s)", m, camlist [i]->Variant);
            item = gtk_menu_item_new_with_label (fm);
            g_free (fm);
        }
        gtk_widget_show (item);
        g_object_set_data(G_OBJECT(item), "lfCamera", (void *)camlist [i]);
        g_signal_connect(G_OBJECT(item), "activate",
                         G_CALLBACK(camera_menu_select), data);
        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }

    data->CameraMenu = gtk_menu_new ();
    for (i = 0; i < makers->len; i++)
    {
        GtkWidget *item = gtk_menu_item_new_with_label (g_ptr_array_index (makers, i));
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (data->CameraMenu), item);
        gtk_menu_item_set_submenu (
            GTK_MENU_ITEM (item), (GtkWidget *)g_ptr_array_index (submenus, i));
    }

    g_ptr_array_free (submenus, TRUE);
    g_ptr_array_free (makers, TRUE);
}

static void parse_maker_model (
    const char *txt, char *make, size_t sz_make, char *model, size_t sz_model)
{
    const gchar *sep;

    while (txt [0] && isspace (txt [0]))
        txt++;
    sep = strchr (txt, '|');
    if (sep)
    {
        int len = sep - txt;
        if (len > sz_make - 1)
            len = sz_make - 1;
        memcpy (make, txt, len);
        make [len] = 0;

        while (*++sep && isspace (sep [0]))
            ;
        len = strlen (sep);
        if (len > sz_model - 1)
            len = sz_model - 1;
        memcpy (model, sep, len);
        model [len] = 0;
    }
    else
    {
        int len = strlen (txt);
        if (len > sz_model - 1)
            len = sz_model - 1;
        memcpy (model, txt, len);
        model [len] = 0;
        make [0] = 0;
    }
}

static void camera_search_clicked(
    GtkWidget *button, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;
    const lfCamera **camlist;
    char make [200], model [200];
    const gchar *txt = gtk_entry_get_text(GTK_ENTRY(data->CameraModel));

    (void)button;

    parse_maker_model (txt, make, sizeof (make), model, sizeof (model));

    camlist = lf_db_find_cameras (CFG->lensdb, make, model);
    if (!camlist)
        return;

    camera_menu_fill (data, camlist);
    lf_free (camlist);

    gtk_menu_popup (GTK_MENU (data->CameraMenu), NULL, NULL, NULL, NULL,
                    0, gtk_get_current_event_time ());
}

static void camera_list_clicked(
    GtkWidget *button, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;
    const lfCamera *const *camlist;

    (void)button;

    camlist = lf_db_get_cameras (CFG->lensdb);
    if (!camlist)
        return;

    camera_menu_fill (data, camlist);

    gtk_menu_popup (GTK_MENU (data->CameraMenu), NULL, NULL, NULL, NULL,
                    0, gtk_get_current_event_time ());
}

/* Update all lens model-related controls to reflect current model */
static void lens_update_controls (preview_data *data)
{
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensDistortionModel),
                              CFG->lens_distortion.Model - LF_DIST_MODEL_NONE);
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensTCAModel),
                              CFG->lens_tca.Model - LF_TCA_MODEL_NONE);
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensVignettingModel),
                              CFG->lens_vignetting.Model - LF_VIGNETTING_MODEL_NONE);
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensFromGeometrySel),
                              CFG->lens->Type);
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensToGeometrySel),
                              CFG->cur_lens_type);
}

static void lens_interpolate (preview_data *data, const lfLens *lens)
{
    lfDistortionModel old_dist_model = CFG->lens_distortion.Model;
    lfTCAModel old_tca_model = CFG->lens_tca.Model;
    lfVignettingModel old_vignetting_model = CFG->lens_vignetting.Model;

    /* Interpolate all models and set the temp values accordingly */
    if (!lf_lens_interpolate_distortion (lens, CFG->focal_len, &CFG->lens_distortion))
        memset (&CFG->lens_distortion, 0, sizeof (CFG->lens_distortion));
    if (!lf_lens_interpolate_tca (lens, CFG->focal_len, &CFG->lens_tca))
        memset (&CFG->lens_tca, 0, sizeof (CFG->lens_tca));
    if (!lf_lens_interpolate_vignetting (lens, CFG->focal_len, CFG->aperture,
                                         CFG->subject_distance, &CFG->lens_vignetting))
        memset (&CFG->lens_vignetting, 0, sizeof (CFG->lens_vignetting));

    lens_update_controls (data);

    /* If the model does not change, the parameter sliders won't be updated.
     * To solve this, we'll call the "changed" callback manually.
     */
    if (CFG->lens_distortion.Model != LF_DIST_MODEL_NONE &&
        old_dist_model == CFG->lens_distortion.Model)
        g_signal_emit_by_name (GTK_COMBO_BOX (data->LensDistortionModel),
                               "changed", NULL, NULL);
    if (CFG->lens_tca.Model != LF_TCA_MODEL_NONE &&
        old_tca_model == CFG->lens_tca.Model)
        g_signal_emit_by_name (GTK_COMBO_BOX (data->LensTCAModel),
                               "changed", NULL, NULL);
    if (CFG->lens_vignetting.Model != LF_VIGNETTING_MODEL_NONE &&
        old_vignetting_model == CFG->lens_vignetting.Model)
        g_signal_emit_by_name (GTK_COMBO_BOX (data->LensVignettingModel),
                               "changed", NULL, NULL);

    if (data->UF->postproc_ops & LF_MODIFY_VIGNETTING)
        preview_invalidate_layer (data, ufraw_develop_phase);
    else
        preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void lens_comboentry_update (GtkComboBox *widget, float *valuep)
{
    preview_data *data = get_preview_data (widget);
    if (sscanf (gtk_combo_box_get_active_text (widget), "%f", valuep) == 1)
        lens_interpolate (data, CFG->lens);
}

static void lens_set (preview_data *data, const lfLens *lens)
{
    gchar *fm;
    const char *maker, *model;
    GtkComboBoxEntry *cbe;
    int i;
    static gdouble focal_values [] =
    {
        4.5, 8, 10, 12, 14, 15, 16, 18, 20, 24, 28, 30, 31, 35, 38, 40, 43,
        45, 50, 55, 60, 70, 75, 77, 80, 85, 90, 100, 105, 110, 120, 135,
        150, 200, 210, 240, 250, 300, 400, 500, 600, 800, 1000
    };
    static gdouble aperture_values [] =
    {
        1, 1.2, 1.4, 1.7, 2, 2.4, 2.8, 3.4, 4, 4.8, 5.6, 6.7,
        8, 9.5, 11, 13, 16, 19, 22, 27, 32, 38
    };

    if (!lens)
    {
        gtk_entry_set_text(GTK_ENTRY(data->LensModel), "");
        uf_widget_set_tooltip(data->LensModel, NULL);
        return;
    }

    lf_lens_copy (CFG->lens, lens);

    if (lens->Model)
    {
        if (lens->Maker)
            fm = g_strdup_printf ("%s | %s", lens->Maker, lens->Model);
        else
            fm = g_strdup_printf ("%s", lens->Model);
        gtk_entry_set_text(GTK_ENTRY(data->LensModel), fm);
        g_free (fm);
    }

    char focal [100], aperture [100], mounts [200];

    if (lens->MinFocal < lens->MaxFocal)
        snprintf (focal, sizeof (focal), "%g-%gmm", lens->MinFocal, lens->MaxFocal);
    else
        snprintf (focal, sizeof (focal), "%gmm", lens->MinFocal);
    if (lens->MinAperture < lens->MaxAperture)
        snprintf (aperture, sizeof (aperture), "%g-%g", lens->MinAperture, lens->MaxAperture);
    else
        snprintf (aperture, sizeof (aperture), "%g", lens->MinAperture);

    mounts [0] = 0;
    if (lens->Mounts)
        for (i = 0; lens->Mounts [i]; i++)
        {
            if (i > 0)
                g_strlcat (mounts, ", ", sizeof (mounts));
            g_strlcat (mounts, lens->Mounts [i], sizeof (mounts));
        }

    maker = lf_mlstr_get (lens->Maker);
    model = lf_mlstr_get (lens->Model);
    fm = g_strdup_printf (_("Maker:\t\t%s\n"
                            "Model:\t\t%s\n"
                            "Focal range:\t%s\n"
                            "Aperture:\t\t%s\n"
                            "Crop factor:\t%.1f\n"
                            "Type:\t\t%s\n"
                            "Mounts:\t\t%s"),
                          maker ? maker : "?", model ? model : "?",
                          focal, aperture, lens->CropFactor,
                          lf_get_lens_type_desc (lens->Type, NULL), mounts);
    uf_widget_set_tooltip(data->LensModel, fm);
    g_free (fm);

    /* Create the focal/aperture/distance combo boxes */
    gtk_container_foreach (
        GTK_CONTAINER (data->LensParamBox), delete_children, NULL);

    int ffi = 0, fli = -1;
    for (i = 0; i < sizeof (focal_values) / sizeof (gdouble); i++)
    {
        if (focal_values [i] < lens->MinFocal)
            ffi = i + 1;
        if (focal_values [i] > lens->MaxFocal && fli == -1)
            fli = i;
    }
    if (fli < ffi)
        fli = ffi + 1;
    if (lens->MaxFocal == 0)
        fli = sizeof (focal_values) / sizeof (gdouble);
    cbe = combo_entry_numeric (
        data->LensParamBox, 0, 0, _("Focal"), _("Lens focal distance"),
        CFG->focal_len, 10.0, focal_values + ffi, fli - ffi);
    g_signal_connect (G_OBJECT(cbe), "changed",
                      G_CALLBACK(lens_comboentry_update), &CFG->focal_len);

    ffi = 0;
    for (i = 0; i < sizeof (aperture_values) / sizeof (gdouble); i++)
        if (aperture_values [i] < lens->MinAperture)
            ffi = i + 1;
    cbe = combo_entry_numeric (
        data->LensParamBox, 0, 0, _("Aperture"), _("Lens aperture"),
        CFG->aperture, 10.0, aperture_values + ffi, sizeof (aperture_values) / sizeof (gdouble) - ffi);
    g_signal_connect (G_OBJECT(cbe), "changed",
                      G_CALLBACK(lens_comboentry_update), &CFG->aperture);

    cbe = combo_entry_numeric_log (
        data->LensParamBox, 0, 0, _("Distance"), _("Distance to subject"),
        CFG->subject_distance, 0.25, 1000, 1.41421356237309504880, 10.0);
    g_signal_connect (G_OBJECT(cbe), "changed",
                      G_CALLBACK(lens_comboentry_update), &CFG->subject_distance);

    gtk_widget_show_all (data->LensParamBox);

    CFG->cur_lens_type = LF_UNKNOWN;
    CFG->lens_scale = 0.0;

    lens_interpolate (data, lens);
}

static void lens_menu_select (
    GtkMenuItem *menuitem, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;
    lens_set (data, (lfLens *)g_object_get_data(G_OBJECT(menuitem), "lfLens"));
}

static void lens_menu_fill (
    preview_data *data, const lfLens *const *lenslist)
{
    unsigned i;
    GPtrArray *makers, *submenus;

    if (data->LensMenu)
    {
        gtk_widget_destroy (data->LensMenu);
        data->LensMenu = NULL;
    }

    /* Count all existing lens makers and create a sorted list */
    makers = g_ptr_array_new ();
    submenus = g_ptr_array_new ();
    for (i = 0; lenslist [i]; i++)
    {
        GtkWidget *submenu, *item;
        const char *m = lf_mlstr_get (lenslist [i]->Maker);
        int idx = ptr_array_find_sorted (makers, m, (GCompareFunc)g_utf8_collate);
        if (idx < 0)
        {
            /* No such maker yet, insert it into the array */
            idx = ptr_array_insert_sorted (makers, m, (GCompareFunc)g_utf8_collate);
            /* Create a submenu for lenses by this maker */
            submenu = gtk_menu_new ();
            ptr_array_insert_index (submenus, submenu, idx);
        }

        submenu = g_ptr_array_index (submenus, idx);
        /* Append current lens name to the submenu */
        item = gtk_menu_item_new_with_label (lf_mlstr_get (lenslist [i]->Model));
        gtk_widget_show (item);
        g_object_set_data(G_OBJECT(item), "lfLens", (void *)lenslist [i]);
        g_signal_connect(G_OBJECT(item), "activate",
                         G_CALLBACK(lens_menu_select), data);
        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }

    data->LensMenu = gtk_menu_new ();
    for (i = 0; i < makers->len; i++)
    {
        GtkWidget *item = gtk_menu_item_new_with_label (g_ptr_array_index (makers, i));
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (data->LensMenu), item);
        gtk_menu_item_set_submenu (
            GTK_MENU_ITEM (item), (GtkWidget *)g_ptr_array_index (submenus, i));
    }

    g_ptr_array_free (submenus, TRUE);
    g_ptr_array_free (makers, TRUE);
}

static void lens_search_clicked(
    GtkWidget *button, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;
    const lfLens **lenslist;
    char make [200], model [200];
    const gchar *txt = gtk_entry_get_text(GTK_ENTRY(data->LensModel));

    (void)button;

    parse_maker_model (txt, make, sizeof (make), model, sizeof (model));
    lenslist = lf_db_find_lenses_hd (CFG->lensdb, CFG->camera,
                                     make [0] ? make : NULL,
                                     model [0] ? model : NULL);
    if (!lenslist)
        return;

    lens_menu_fill (data, lenslist);
    lf_free (lenslist);

    gtk_menu_popup (GTK_MENU (data->LensMenu), NULL, NULL, NULL, NULL,
                    0, gtk_get_current_event_time ());
}

static void lens_list_clicked(
    GtkWidget *button, gpointer user_data)
{
    preview_data *data = (preview_data *)user_data;

    (void)button;

    if (CFG->camera)
    {
        const lfLens **lenslist = lf_db_find_lenses_hd (
            CFG->lensdb, CFG->camera, NULL, NULL);
        if (!lenslist)
            return;
        lens_menu_fill (data, lenslist);
        lf_free (lenslist);
    }
    else
    {
        const lfLens *const *lenslist = lf_db_get_lenses (CFG->lensdb);
        if (!lenslist)
            return;
        lens_menu_fill (data, lenslist);
    }

    gtk_menu_popup (GTK_MENU (data->LensMenu), NULL, NULL, NULL, NULL,
                    0, gtk_get_current_event_time ());
}

static void reset_adjustment_value (GtkWidget *widget, const lfParameter *param)
{
    GtkAdjustment *adj = (GtkAdjustment *)g_object_get_data (
        G_OBJECT (widget), "Adjustment");

    gtk_adjustment_set_value (adj, param->Default);
}

static GtkAdjustment *append_term (
    GtkWidget *table, int y, const lfParameter *param,
    float *term, GCallback callback)
{
    double step, page;
    double tmp = (param->Max - param->Min) / 100000.0;
    for (step = 0.00001; ; step *= 10.0)
        if (step >= tmp)
            break;

    tmp = (param->Max - param->Min) / 10.0;
    for (page = 0.00001; ; page *= 10.0)
        if (page >= tmp)
            break;

    long accuracy = 0;
    for (tmp = step; tmp < 1.0; tmp *= 10)
        accuracy++;

    GtkAdjustment *adj = adjustment_scale (
        GTK_TABLE (table), 0, y, param->Name, *term, term,
        param->Min, param->Max, step, page, accuracy, NULL, callback);

    GtkWidget *button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button),
                       gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON));
    gtk_table_attach (GTK_TABLE (table), button, 7, 8, y, y + 1, 0, 0, 0, 0);
    g_object_set_data (G_OBJECT(button), "Adjustment", adj);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (reset_adjustment_value), (void *)param);

    return adj;
}

static void lens_scale_update (GtkAdjustment *adj, float *valuep)
{
    preview_data *data = get_preview_data (adj);
    *valuep = gtk_adjustment_get_value (adj);
    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void lens_scale_reset (GtkWidget *button, gpointer user_data)
{
    (void)user_data;
    preview_data *data = get_preview_data (button);
    gtk_adjustment_set_value (data->LensScaleAdjustment, 0.0);
    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void lens_autoscale (GtkWidget *button, gpointer user_data)
{
    (void)user_data;
    preview_data *data = get_preview_data (button);
    if (!data->UF->modifier)
        gtk_adjustment_set_value (data->LensScaleAdjustment, 0.0);
    else
    {
        float cs = pow (2.0, CFG->lens_scale);
        float as = lf_modifier_get_auto_scale (data->UF->modifier, 0);
        gtk_adjustment_set_value (data->LensScaleAdjustment,
                                  log (cs * as) / log (2.0));
        preview_invalidate_layer (data, ufraw_lensfun_phase);
        render_preview (data);
    }
}

/* --- TCA correction page --- */

static void remove_tca_models (preview_data *data, lfTCAModel model)
{
    int i;

    /* Remove calibration data which uses a different model */
    if (CFG->lens->CalibTCA)
        for (i = 0; CFG->lens->CalibTCA [i]; )
            if (CFG->lens->CalibTCA [i]->Model != model)
                lf_lens_remove_calib_tca (CFG->lens, i);
            else
                i++;
}

static void adjustment_update_tca (GtkAdjustment *adj, float *valuep)
{
    preview_data *data = get_preview_data (adj);
    *valuep = gtk_adjustment_get_value (adj);

    CFG->lens_tca.Focal = CFG->focal_len;

    remove_tca_models (data, CFG->lens_tca.Model);
    lf_lens_add_calib_tca (CFG->lens, &CFG->lens_tca);

    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void tca_model_changed (GtkComboBox *widget, preview_data *data)
{
    lfTCAModel model =
        (lfTCAModel) gtk_combo_box_get_active (widget);

    const char *details;
    const lfParameter **params;
    if (!lf_get_tca_model_desc (model, &details, &params))
        return; /* should never happen */

    gtk_container_foreach (
        GTK_CONTAINER (data->LensTCATable), delete_children, NULL);

    gboolean reset_values = (CFG->lens_tca.Model != model);
    if (reset_values)
        remove_tca_models (data, model);

    CFG->lens_tca.Model = model;

    int i;
    if (params)
        for (i = 0; params [i]; i++)
        {
            if (reset_values)
                CFG->lens_tca.Terms [i] = params [i]->Default;

            append_term (data->LensTCATable, i,
                         params [i], &CFG->lens_tca.Terms [i],
                         G_CALLBACK (adjustment_update_tca));
        }

    gtk_label_set_text (GTK_LABEL (data->LensTCADesc), details);
    gtk_widget_show_all (data->LensTCATable);

    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void fill_tca_page (preview_data *data)
{
    GtkWidget *label;
    int i, active_index;

    /* Remove all controls from the page */
    gtk_container_foreach (
        GTK_CONTAINER (data->LensTCAPage), delete_children, NULL);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (data->LensTCAPage), hbox, FALSE, FALSE, 0);

    /* Add the model combobox */
    label = gtk_label_new (_("Model:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

    data->LensTCAModel = gtk_combo_box_new_text ();
    uf_widget_set_tooltip(data->LensTCAModel,
                          _("Chromatic Aberrations mathematical model"));
    active_index = 0;
    for (i = 0; ; i++)
    {
        const char *model_name;
        lfTCAModel model = LF_TCA_MODEL_NONE + i;
        model_name = lf_get_tca_model_desc (model, NULL, NULL);
        if (!model_name)
            break;
        gtk_combo_box_append_text (GTK_COMBO_BOX (data->LensTCAModel), model_name);
        if (model == CFG->lens_tca.Model)
            active_index = i;
    }
    g_signal_connect(G_OBJECT(data->LensTCAModel), "changed",
                     G_CALLBACK(tca_model_changed), data);
    gtk_box_pack_start (GTK_BOX (hbox), data->LensTCAModel, TRUE, TRUE, 0);

    data->LensTCATable = gtk_table_new (10, 1, FALSE);
    GtkWidget *f = gtk_frame_new (_("Parameters"));
    gtk_box_pack_start (GTK_BOX (data->LensTCAPage), f, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(f), data->LensTCATable);

    data->LensTCADesc = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (data->LensTCADesc), TRUE);
    gtk_box_pack_start (GTK_BOX (data->LensTCAPage), data->LensTCADesc, FALSE, FALSE, 0);

    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensTCAModel), active_index);
}

/* --- Vignetting correction page --- */

static void remove_vign_models (preview_data *data, lfVignettingModel model)
{
    int i;

    /* Remove calibration data which uses a different model */
    if (CFG->lens->CalibVignetting)
        for (i = 0; CFG->lens->CalibVignetting [i]; )
            if (CFG->lens->CalibVignetting [i]->Model != model)
                lf_lens_remove_calib_vignetting (CFG->lens, i);
            else
                i++;
}

static void adjustment_update_vign (GtkAdjustment *adj, float *valuep)
{
    preview_data *data = get_preview_data (adj);
    *valuep = gtk_adjustment_get_value (adj);

    CFG->lens_vignetting.Focal = CFG->focal_len;
    CFG->lens_vignetting.Aperture = CFG->aperture;
    CFG->lens_vignetting.Distance = CFG->subject_distance;

    remove_vign_models (data, CFG->lens_vignetting.Model);
    lf_lens_add_calib_vignetting (CFG->lens, &CFG->lens_vignetting);

    preview_invalidate_layer (data, ufraw_develop_phase);
    render_preview (data);
}

static void vignetting_model_changed (GtkComboBox *widget, preview_data *data)
{
    lfVignettingModel model =
        (lfVignettingModel) gtk_combo_box_get_active (widget);

    const char *details;
    const lfParameter **params;
    if (!lf_get_vignetting_model_desc (model, &details, &params))
        return; /* should never happen */

    gtk_container_foreach (
        GTK_CONTAINER (data->LensVignettingTable), delete_children, NULL);

    gboolean reset_values = (CFG->lens_vignetting.Model != model);
    if (reset_values)
        remove_vign_models (data, model);
    CFG->lens_vignetting.Model = model;

    int i;
    if (params)
        for (i = 0; params [i]; i++)
        {
            if (reset_values)
                CFG->lens_vignetting.Terms [i] = params [i]->Default;

            append_term (data->LensVignettingTable, i,
                         params [i], &CFG->lens_vignetting.Terms [i],
                         G_CALLBACK (adjustment_update_vign));
        }

    gtk_label_set_text (GTK_LABEL (data->LensVignettingDesc), details);
    gtk_widget_show_all (data->LensVignettingTable);

    preview_invalidate_layer (data, ufraw_develop_phase);
    render_preview (data);
}

static void fill_vignetting_page (preview_data *data)
{
    GtkWidget *label;
    int i, active_index;

    /* Remove all controls from the page */
    gtk_container_foreach (
        GTK_CONTAINER (data->LensVignettingPage), delete_children, NULL);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (data->LensVignettingPage), hbox, FALSE, FALSE, 0);

    /* Add the model combobox */
    label = gtk_label_new (_("Model:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

    data->LensVignettingModel = gtk_combo_box_new_text ();
    uf_widget_set_tooltip(data->LensVignettingModel,
                          _("Optical vignetting mathematical model"));
    active_index = 0;
    for (i = 0; ; i++)
    {
        const char *model_name;
        lfVignettingModel model = LF_VIGNETTING_MODEL_NONE + i;
        model_name = lf_get_vignetting_model_desc (model, NULL, NULL);
        if (!model_name)
            break;
        gtk_combo_box_append_text (GTK_COMBO_BOX (data->LensVignettingModel), model_name);
        if (model == CFG->lens_vignetting.Model)
            active_index = i;
    }
    g_signal_connect(G_OBJECT(data->LensVignettingModel), "changed",
                     G_CALLBACK(vignetting_model_changed), data);
    gtk_box_pack_start (GTK_BOX (hbox), data->LensVignettingModel, TRUE, TRUE, 0);

    data->LensVignettingTable = gtk_table_new (10, 1, FALSE);
    GtkWidget *f = gtk_frame_new (_("Parameters"));
    gtk_box_pack_start (GTK_BOX (data->LensVignettingPage), f, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(f), data->LensVignettingTable);

    data->LensVignettingDesc = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (data->LensVignettingDesc), TRUE);
    gtk_box_pack_start (GTK_BOX (data->LensVignettingPage), data->LensVignettingDesc, FALSE, FALSE, 0);

    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensVignettingModel), active_index);
}

/* --- Distortion correction page --- */

static void remove_dist_models (preview_data *data, lfDistortionModel model)
{
    int i;

    /* Remove calibration data which uses a different model */
    if (CFG->lens->CalibDistortion)
        for (i = 0; CFG->lens->CalibDistortion [i]; )
            if (CFG->lens->CalibDistortion [i]->Model != model)
                lf_lens_remove_calib_distortion (CFG->lens, i);
            else
                i++;
}

static void adjustment_update_dist (GtkAdjustment *adj, float *valuep)
{
    preview_data *data = get_preview_data (adj);
    *valuep = gtk_adjustment_get_value (adj);

    CFG->lens_distortion.Focal = CFG->focal_len;

    remove_dist_models (data, CFG->lens_distortion.Model);
    lf_lens_add_calib_distortion (CFG->lens, &CFG->lens_distortion);

    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void distortion_model_changed (GtkComboBox *widget, preview_data *data)
{
    lfDistortionModel model =
        (lfDistortionModel) gtk_combo_box_get_active (widget);

    const char *details;
    const lfParameter **params;
    if (!lf_get_distortion_model_desc (model, &details, &params))
        return; // should never happen

    gtk_container_foreach (
        GTK_CONTAINER (data->LensDistortionTable), delete_children, NULL);

    gboolean reset_values = (CFG->lens_distortion.Model != model);
    if (reset_values)
        remove_dist_models (data, model);

    CFG->lens_distortion.Model = model;

    int i;
    if (params)
        for (i = 0; params [i]; i++)
        {
            if (reset_values)
                CFG->lens_distortion.Terms [i] = params [i]->Default;

            append_term (data->LensDistortionTable, i,
                         params [i], &CFG->lens_distortion.Terms [i],
                         G_CALLBACK (adjustment_update_dist));
        }

    gtk_label_set_text (GTK_LABEL (data->LensDistortionDesc), details);
    gtk_widget_show_all (data->LensDistortionTable);

    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void fill_distortion_page (preview_data *data)
{
    GtkWidget *label;
    int i, active_index;

    /* Remove all controls from the page */
    gtk_container_foreach (
        GTK_CONTAINER (data->LensDistortionPage), delete_children, NULL);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (data->LensDistortionPage), hbox, FALSE, FALSE, 0);

    /* Add the model combobox */
    label = gtk_label_new (_("Model:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

    data->LensDistortionModel = gtk_combo_box_new_text ();
    uf_widget_set_tooltip (data->LensDistortionModel,
                           _("Lens distortion mathematical model"));
    active_index = 0;
    for (i = 0; ; i++)
    {
        const char *model_name;
        lfDistortionModel model = LF_DIST_MODEL_NONE + i;
        model_name = lf_get_distortion_model_desc (model, NULL, NULL);
        if (!model_name)
            break;
        gtk_combo_box_append_text (GTK_COMBO_BOX (data->LensDistortionModel), model_name);
        if (model == CFG->lens_distortion.Model)
            active_index = i;
    }
    g_signal_connect (G_OBJECT (data->LensDistortionModel), "changed",
                      G_CALLBACK (distortion_model_changed), data);
    gtk_box_pack_start (GTK_BOX (hbox), data->LensDistortionModel, TRUE, TRUE, 0);

    data->LensDistortionTable = gtk_table_new (10, 1, FALSE);
    GtkWidget *f = gtk_frame_new (_("Parameters"));
    gtk_box_pack_start (GTK_BOX (data->LensDistortionPage), f, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER(f), data->LensDistortionTable);

    data->LensDistortionDesc = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (data->LensDistortionDesc), TRUE);
    gtk_box_pack_start (GTK_BOX (data->LensDistortionPage),
                        data->LensDistortionDesc, FALSE, FALSE, 0);

    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensDistortionModel), active_index);
}

/* --- Lens geometry page --- */

static void geometry_model_changed (GtkComboBox *widget, preview_data *data)
{
    lfLensType type =
        (lfLensType) gtk_combo_box_get_active (widget);

    const char *details;
    if (!lf_get_lens_type_desc (type, &details))
        return; // should never happen

    lfLensType *target = (lfLensType *)g_object_get_data (
        G_OBJECT (widget), "LensType");

    *target = type;

    if (target == &CFG->cur_lens_type)
        gtk_label_set_text (GTK_LABEL (data->LensToGeometryDesc), details);
    else
        gtk_label_set_text (GTK_LABEL (data->LensFromGeometryDesc), details);

    preview_invalidate_layer (data, ufraw_lensfun_phase);
    render_preview (data);
}

static void fill_geometry_page (preview_data *data)
{
    GtkWidget *label;
    int i;

    /* Remove all controls from the page */
    gtk_container_foreach (
        GTK_CONTAINER (data->LensGeometryPage), delete_children, NULL);

    data->LensGeometryTable = gtk_table_new (10, 1, FALSE);
    gtk_box_pack_start (GTK_BOX (data->LensGeometryPage),
                        data->LensGeometryTable, TRUE, TRUE, 0);

    /* Add the model combobox */
    label = gtk_label_new (_("Lens geometry:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), label,
                      0, 1, 0, 1, GTK_FILL, 0, 5, 0);
    data->LensFromGeometrySel = gtk_combo_box_new_text ();
    uf_widget_set_tooltip(data->LensFromGeometrySel, _("The geometry of the lens used to make the shot"));
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), data->LensFromGeometrySel,
                      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    data->LensFromGeometryDesc = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (data->LensFromGeometryDesc), TRUE);
    gtk_misc_set_alignment (GTK_MISC (data->LensFromGeometryDesc), 0.5, 0.5);
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), data->LensFromGeometryDesc,
                      0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 10);

    label = gtk_label_new (_("Target geometry:"));
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), label,
                      0, 1, 2, 3, GTK_FILL, 0, 5, 0);
    data->LensToGeometrySel = gtk_combo_box_new_text ();
    uf_widget_set_tooltip(data->LensToGeometrySel, _("The target geometry for output image"));
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), data->LensToGeometrySel,
                      1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    data->LensToGeometryDesc = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (data->LensToGeometryDesc), TRUE);
    gtk_misc_set_alignment (GTK_MISC (data->LensToGeometryDesc), 0.5, 0.5);
    gtk_table_attach (GTK_TABLE (data->LensGeometryTable), data->LensToGeometryDesc,
                      0, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 10);

    for (i = 0; ; i++)
    {
        lfLensType type = LF_UNKNOWN + i;
        const char *type_name = lf_get_lens_type_desc (type, NULL);
        if (!type_name)
            break;
        gtk_combo_box_append_text (GTK_COMBO_BOX (data->LensFromGeometrySel), type_name);
        gtk_combo_box_append_text (GTK_COMBO_BOX (data->LensToGeometrySel), type_name);
    }
    g_object_set_data (G_OBJECT (data->LensFromGeometrySel), "LensType", &CFG->lens->Type);
    g_signal_connect (G_OBJECT (data->LensFromGeometrySel), "changed",
                      G_CALLBACK(geometry_model_changed), data);
    g_object_set_data (G_OBJECT (data->LensToGeometrySel), "LensType", &CFG->cur_lens_type);
    g_signal_connect (G_OBJECT (data->LensToGeometrySel), "changed",
                      G_CALLBACK(geometry_model_changed), data);

    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensFromGeometrySel), CFG->lens->Type);
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->LensToGeometrySel), CFG->cur_lens_type);
}

/**
 * Fill the "lens correction" page in the main notebook.
 */
void lens_fill_interface (preview_data *data, GtkWidget *page)
{
    GtkTable *table, *subTable;
    GtkWidget *label, *button;

    /* Camera selector */
    table = GTK_TABLE(gtk_table_new(10, 10, FALSE));
    gtk_box_pack_start(GTK_BOX(page), GTK_WIDGET(table), FALSE, FALSE, 0);

    label = gtk_label_new(_("Camera"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 0, 1, GTK_FILL, 0, 2, 0);

    data->CameraModel = gtk_entry_new();
    gtk_table_attach(table, data->CameraModel, 1, 2, 0, 1,
                     GTK_EXPAND|GTK_FILL, 0, 2, 0);

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON));
    uf_widget_set_tooltip(button, _("Search for camera using the entered pattern"));
    gtk_table_attach(table, button, 2, 3, 0, 1, 0, 0, 0, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(camera_search_clicked), data);

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON));
    uf_widget_set_tooltip(button, _("Choose camera from a complete list"));
    gtk_table_attach(table, button, 3, 4, 0, 1, 0, 0, 0, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(camera_list_clicked), data);

    /* Lens selector */
    label = gtk_label_new(_("Lens"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 1, 2, GTK_FILL, 0, 2, 0);

    data->LensModel = gtk_entry_new();
    //gtk_entry_set_text(GTK_ENTRY(data->LensModel), "");
    gtk_table_attach(table, data->LensModel, 1, 2, 1, 2,
                     GTK_EXPAND|GTK_FILL, 0, 2, 0);

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON));
    uf_widget_set_tooltip(button, _("Search for lens using the entered pattern"));
    gtk_table_attach(table, button, 2, 3, 1, 2, 0, 0, 0, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(lens_search_clicked), data);

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON));
    uf_widget_set_tooltip(button, _("Choose lens from a list of possible variants"));
    gtk_table_attach(table, button, 3, 4, 1, 2, 0, 0, 0, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(lens_list_clicked), data);

    data->LensParamBox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (page), data->LensParamBox, FALSE, FALSE, 2);

    subTable = GTK_TABLE (gtk_table_new (10, 10, FALSE));
    gtk_box_pack_start (GTK_BOX (page), GTK_WIDGET (subTable), FALSE, FALSE, 0);

    data->LensScaleAdjustment = adjustment_scale (
        subTable, 0, 0, _("Scale"), CFG->lens_scale, &CFG->lens_scale,
	    -3, 3, 0.001, 0.1, 3, _("Image scale power-of-two"),
        G_CALLBACK (lens_scale_update));

    data->LensScaleResetButton = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (data->LensScaleResetButton),
	    gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON));
    gtk_table_attach (subTable, data->LensScaleResetButton, 7, 8, 0, 1, 0,0,0,0);
    uf_widget_set_tooltip (data->LensScaleResetButton,
                           _("Reset image scale to default"));
    g_signal_connect(G_OBJECT(data->LensScaleResetButton), "clicked",
                     G_CALLBACK (lens_scale_reset), NULL);

    data->LensAutoScaleButton = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (data->LensAutoScaleButton),
	    gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_BUTTON));
    gtk_table_attach (subTable, data->LensAutoScaleButton, 8, 9, 0, 1, 0,0,0,0);
    uf_widget_set_tooltip (data->LensAutoScaleButton,
                           _("Autoscale the image for best fit"));
    g_signal_connect(G_OBJECT(data->LensAutoScaleButton), "clicked",
                     G_CALLBACK (lens_autoscale), NULL);

    GtkNotebook *subnb = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_box_pack_start(GTK_BOX(page), GTK_WIDGET(subnb),
                       TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos (subnb, GTK_POS_LEFT);

    data->LensTCAPage = notebook_page_new(
        subnb, _("Lateral chromatic aberration"), "tca");
    data->LensVignettingPage = notebook_page_new(
        subnb, _("Optical vignetting"), "vignetting");
    data->LensDistortionPage = notebook_page_new(
        subnb, _("Lens distortion"), "distortion");
    //data->LensCCIPage = notebook_page_new(
    //    subnb, _("Color contribution"), NULL);
    data->LensGeometryPage = notebook_page_new(
        subnb, _("Lens geometry"), "geometry");

    /* Create a default lens & camera */
    CFG->lens = lf_lens_new ();
    CFG->camera = lf_camera_new ();
    CFG->cur_lens_type = LF_UNKNOWN;

    /* Set lens and camera from EXIF info, if possible */
    if (CFG->real_make [0] || CFG->real_model [0])
    {
        const lfCamera **cams = lf_db_find_cameras (
            CFG->lensdb, CFG->real_make, CFG->real_model);
        if (cams)
        {
            camera_set (data, cams [0]);
            lf_free (cams);
        }
    }

    fill_tca_page (data);
    fill_vignetting_page (data);
    fill_distortion_page (data);
    fill_geometry_page (data);

    lens_set (data, CFG->lens);
}

#endif /* HAVE_LENSFUN */