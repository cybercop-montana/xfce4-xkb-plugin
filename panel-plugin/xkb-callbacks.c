/* vim: set backspace=2 ts=4 softtabstop=4 sw=4 cinoptions=>4 expandtab autoindent smartindent: */
/* xkb-callbacks.c
 * Copyright (C) 2008 Alexander Iliev <sasoiliev@mamul.org>
 *
 * Parts of this program comes from the XfKC tool:
 * Copyright (C) 2006 Gauvain Pocentek <gauvainpocentek@gmail.com>
 *
 * A part of this file comes from the gnome keyboard capplet (control-center):
 * Copyright (C) 2003 Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "xkb-callbacks.h"
#include "xkb-cairo.h"
#include "xkb-util.h"

static void xkb_plugin_popup_menu (GtkButton *btn,
                                   GdkEventButton *event,
                                   t_xkb *xkb);

void
xkb_plugin_active_window_changed (WnckScreen *screen,
                                  WnckWindow *previously_active_window,
                                  t_xkb *xkb)
{
    WnckWindow *window;
    guint window_id, application_id;

    window = wnck_screen_get_active_window (screen);
    if (!WNCK_IS_WINDOW (window)) return;
    window_id = wnck_window_get_xid (window);
    application_id = wnck_window_get_pid (window);

    xkb_keyboard_window_changed (xkb->keyboard, window_id, application_id);
}

void
xkb_plugin_application_closed (WnckScreen *screen,
                               WnckApplication *app,
                               t_xkb *xkb)
{
    guint application_id;

    application_id = wnck_application_get_pid (app);

    xkb_keyboard_application_closed (xkb->keyboard, application_id);
}

void
xkb_plugin_window_closed (WnckScreen *screen,
                          WnckWindow *window,
                          t_xkb *xkb)
{
    guint window_id;

    window_id = wnck_window_get_xid (window);

    xkb_keyboard_window_closed (xkb->keyboard, window_id);
}

gboolean
xkb_plugin_layout_image_draw (GtkWidget *widget,
                                 cairo_t *cr,
                                 t_xkb *xkb)
{
    const gchar *group_name;
    GtkAllocation allocation;
    GtkStyleContext *style_ctx;
    GtkStateFlags state;
    PangoFontDescription *desc;
    GdkRGBA rgba;
    gint actual_hsize, actual_vsize;
    gint display_type, display_scale;

    display_type = xkb_xfconf_get_display_type (xkb->config);
    display_scale = xkb_xfconf_get_display_scale (xkb->config);

    gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
    actual_hsize = allocation.width;
    actual_vsize = allocation.height;

    state = gtk_widget_get_state_flags (GTK_WIDGET (xkb->btn));
    style_ctx = gtk_widget_get_style_context (GTK_WIDGET (xkb->btn));
    gtk_style_context_get_color (style_ctx, state, &rgba);
    group_name = xkb_keyboard_get_group_name (xkb->keyboard, -1);

    DBG ("img_exposed: actual h/v (%d/%d)",
         actual_hsize, actual_vsize);

    if (display_type == DISPLAY_TYPE_IMAGE)
    {
        xkb_cairo_draw_flag (cr, group_name,
                actual_hsize, actual_vsize,
                xkb_keyboard_variant_index_for_group (xkb->keyboard, -1),
                xkb_keyboard_get_max_group_count (xkb->keyboard),
                display_scale,
                rgba
        );
    }
    else if (display_type == DISPLAY_TYPE_TEXT)
    {
        xkb_cairo_draw_label (cr, group_name,
                actual_hsize, actual_vsize,
                xkb_keyboard_variant_index_for_group (xkb->keyboard, -1),
                display_scale,
                rgba
        );
    }
    else
    {
        gtk_style_context_get (style_ctx, state, "font", &desc, NULL);
        xkb_cairo_draw_label_system (cr, group_name,
                actual_hsize, actual_vsize,
                xkb_keyboard_variant_index_for_group (xkb->keyboard, -1),
                desc, rgba
        );
    }

    return FALSE;
}

gboolean
xkb_plugin_button_clicked (GtkButton *btn,
                           GdkEventButton *event,
                           gpointer data)
{
    t_xkb *xkb;
    gboolean released, display_popup;

    if (event->button == 1)
    {
        xkb = data;
        released = event->type == GDK_BUTTON_RELEASE;
        display_popup = xkb_keyboard_get_group_count (xkb->keyboard) > 2;

        if (display_popup && !released)
        {
            xkb_plugin_popup_menu (btn, event, data);
            return TRUE;
        }

        if (!display_popup && released)
        {
            xkb_keyboard_next_group (xkb->keyboard);
            return FALSE;
        }
    }
    return FALSE;
}

gboolean
xkb_plugin_button_scrolled (GtkWidget *btn,
                            GdkEventScroll *event,
                            gpointer data)
{
    t_xkb *xkb = data;

    switch (event->direction)
    {
      case GDK_SCROLL_UP:
      case GDK_SCROLL_RIGHT:
          xkb_keyboard_next_group (xkb->keyboard);
          return TRUE;
      case GDK_SCROLL_DOWN:
      case GDK_SCROLL_LEFT:
          xkb_keyboard_prev_group (xkb->keyboard);
          return TRUE;
      default:
        return FALSE;
    }

    return FALSE;
}

static void
xkb_plugin_popup_menu (GtkButton *btn,
                       GdkEventButton *event,
                       t_xkb *xkb)
{
    gtk_widget_set_state_flags (GTK_WIDGET (xkb->btn), GTK_STATE_FLAG_CHECKED, FALSE);
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup_at_widget (GTK_MENU (xkb->popup), GTK_WIDGET (btn),
            GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent *) event);
#else
    gtk_menu_popup (GTK_MENU (xkb->popup), NULL, NULL,
            xfce_panel_plugin_position_menu, xkb->plugin,
            0, event->time);
#endif
}

void
xkb_plugin_popup_menu_deactivate (gpointer data,
                                  GtkMenuShell *menu_shell)
{
    t_xkb *xkb = (t_xkb *) data;

    g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

    gtk_widget_unset_state_flags (GTK_WIDGET (xkb->btn), GTK_STATE_FLAG_CHECKED);
}

gboolean
xkb_plugin_set_tooltip (GtkWidget *widget,
                        gint x,
                        gint y,
                        gboolean keyboard_mode,
                        GtkTooltip *tooltip,
                        t_xkb *xkb)
{
    gint group;
    gchar *layout_name;
    GdkPixbuf *pixbuf;

    group = xkb_keyboard_get_current_group (xkb->keyboard);

    if (xkb_xfconf_get_display_tooltip_icon (xkb->config))
    {
        pixbuf = xkb_keyboard_get_tooltip_pixbuf (xkb->keyboard, group);
        gtk_tooltip_set_icon (tooltip, pixbuf);
    }

    layout_name = xkb_keyboard_get_pretty_layout_name (xkb->keyboard, group);

    gtk_tooltip_set_text (tooltip, layout_name);

    return TRUE;
}

