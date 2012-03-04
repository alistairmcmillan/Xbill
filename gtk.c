#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include "util.h"

#include "gtk.h"

#include "Game.h"
#include "UI.h"

struct MCursor {
	GdkCursor *cursor;
};

struct Picture {
	gint width, height;
	GdkPixmap *pix;
	GdkBitmap *mask;
	GdkGC *gc;
};

static const char *pictdir;

static GtkWidget *toplevel, *base, *menubar, *field;
static GtkWidget *dialogs[DIALOG_MAX + 1];
static GtkWidget *pausebutton;
static guint timer;
static GdkGC *stdgc;
static GdkPixmap *offscreen;
static PangoLayout *layout;
static GdkColor white, black;
static int screensize;

/*
 * Callback functions
 */

static void
gtk_ui_popup_dialog(int index) {
	GtkWidget *popup;
	int tx, ty, tw, th;
	int px, py, pw, ph;

	popup = dialogs[index];

	gdk_window_get_origin(gtk_widget_get_window(toplevel), &tx, &ty);
	tw = gdk_window_get_width(gtk_widget_get_window(toplevel));
	th = gdk_window_get_height(gtk_widget_get_window(toplevel));
	pw = gdk_window_get_width(gtk_widget_get_window(popup));
	ph = gdk_window_get_height(gtk_widget_get_window(popup));
	px = tx + (tw - pw) / 2;
	py = ty + (th - ph) / 2;
	gtk_window_set_position(GTK_WINDOW(popup), GTK_WIN_POS_NONE);
	gtk_widget_set_uposition(popup, px, py);
	gtk_widget_show_all(popup);
	gtk_main();
}

static void
popdown(void) {
	gtk_main_quit();
}

static void
new_game(void) {
	Game_start(1);
}

static void
quit_game(void) {
	Game_quit();
}

static void
warp_apply(GtkWidget *text) {
	const char *str;
	char *endp;
	int newlevel;

	str = gtk_entry_get_text(GTK_ENTRY(text));
	newlevel = strtol(str, &endp, 10);
	if (*endp != '\0')
		return;
	Game_warp_to_level(newlevel);
}

static void
enter_name(GtkWidget *text) {
	const char *str;

	str = gtk_entry_get_text(GTK_ENTRY(text));
	Game_add_high_score(str);
}

/*
 * Event handlers
 */

static gboolean
leave_window(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	UNUSED(widget);
	UNUSED(event);
	UNUSED(user_data);

	UI_pause_game();
	return FALSE;
}

static gboolean
enter_window(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	UNUSED(widget);
	UNUSED(event);
	UNUSED(user_data);

	UI_resume_game();
	return FALSE;
}

static gboolean
redraw_window(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	UNUSED(widget);
	UNUSED(event);
	UNUSED(user_data);

	UI_refresh();
	return FALSE;
}

static gboolean
button_press(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GdkEventButton *buttonevent = (GdkEventButton *) event;

	UNUSED(widget);
	UNUSED(user_data);

	Game_button_press((int)buttonevent->x, (int)buttonevent->y);
	return FALSE;
}

static gboolean
button_release(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GdkEventButton *buttonevent = (GdkEventButton *) event;

	UNUSED(widget);
	UNUSED(user_data);

	Game_button_release((int)buttonevent->x, (int)buttonevent->y);
	return FALSE;
}

static int
timer_tick(gpointer arg) {
	UNUSED(arg);

	UI_restart_timer();
	Game_update();
	return TRUE;
}

/*
 * Cursor handling
 */

#include "bitmaps/apple.xbm"
#include "bitmaps/bsd.xbm"
#include "bitmaps/hurd.xbm"
#include "bitmaps/linux.xbm"
#include "bitmaps/next.xbm"
#include "bitmaps/os2.xbm"
#include "bitmaps/palm.xbm"
#include "bitmaps/redhat.xbm"
#include "bitmaps/sgi.xbm"
#include "bitmaps/sun.xbm"
#include "bitmaps/bucket.xbm"
#include "bitmaps/hand_down.xbm"
#include "bitmaps/hand_down_mask.xbm"
#include "bitmaps/hand_up.xbm"
#include "bitmaps/hand_up_mask.xbm"

typedef struct cursormap {
	const char *name;
	int width, height;
	const char *data, *maskdata;
} cursormap;

#define CURSOR_ADD(x) \
	{#x, x ## _width, x ## _height, x ## _bits, NULL}

#define CURSOR_ADD_MASKED(x) \
	{#x, x ## _width, x ## _height, x ## _bits, x ## _mask_bits}

static cursormap cursors[] = {
	CURSOR_ADD(apple), CURSOR_ADD(bsd), CURSOR_ADD(hurd),
	CURSOR_ADD(linux), CURSOR_ADD(next), CURSOR_ADD(os2), CURSOR_ADD(palm),
	CURSOR_ADD(redhat), CURSOR_ADD(sgi), CURSOR_ADD(sun),
	CURSOR_ADD(bucket),
	CURSOR_ADD_MASKED(hand_up), CURSOR_ADD_MASKED(hand_down),
	{NULL, 0, 0, NULL, NULL},
};

static void
gtk_ui_set_cursor(MCursor *cursor) {
	gdk_window_set_cursor(gtk_widget_get_window(field), cursor->cursor);
}

static void
gtk_ui_load_cursor(const char *name, int masked, MCursor **cursorp) {
	MCursor *cursor;
	GdkBitmap *bitmap, *mask;
	cursormap *c;

	cursor = xalloc(sizeof *cursor);

	for (c = cursors; c->name != NULL; c++)
		if (strcmp(name, c->name) == 0)
			break;
	if (c->name == NULL)
		fatal("couldn't load cursor: %s", name);
	bitmap = (GdkBitmap *)gdk_bitmap_create_from_data(gtk_widget_get_window(field), c->data,
					     c->width, c->height);

	if (masked == CURSOR_SEP_MASK)
		mask = (GdkBitmap *)gdk_bitmap_create_from_data(gtk_widget_get_window(field), c->maskdata,
						   c->width, c->height);
	else
		mask = bitmap;
	cursor->cursor = (GdkCursor *)gdk_cursor_new_from_pixmap(bitmap, mask, 
						    &black, &white,
						    c->width/2, c->height/2);
	*cursorp = cursor;
}

/*
 * Pixmap handling
 */

static void
gtk_ui_load_picture(const char *name, int trans, Picture **pictp) {
	Picture *pict;
	char file[255];
	GdkBitmap *mask;

	UNUSED(trans);

	pict = xalloc(sizeof *pict);

	sprintf(file, "%s/pixmaps/%s.xpm", pictdir, name);
	pict->pix = (GdkPixmap *)gdk_pixmap_create_from_xpm(gtk_widget_get_window(toplevel), &mask,
					       NULL, file);
	if (pict->pix == NULL)
		fatal("error reading %s", file);
	pict->mask = mask;
	pict->gc = (GdkGC *)gdk_gc_new(gtk_widget_get_window(toplevel));
	gdk_gc_set_exposures(pict->gc, FALSE);
	gdk_gc_set_clip_mask(pict->gc, mask);
	gdk_pixmap_get_size(pict->pix, &pict->width, &pict->height);

	*pictp = pict;
}

static void
gtk_ui_set_icon(Picture *icon) {
	gdk_window_set_icon(gtk_widget_get_window(toplevel), NULL, icon->pix, icon->mask);
}

static int
gtk_ui_picture_width(Picture *pict) {
	return (pict->width);
}

static int
gtk_ui_picture_height(Picture *pict) {
	return (pict->height);
}

/*
 * Graphics operations
 */

static void
gtk_ui_clear_window(void) {
	gdk_draw_rectangle(offscreen, gtk_widget_get_style(field)->white_gc, TRUE, 0, 0,
			   screensize, screensize);
}

static void
gtk_ui_refresh_window(void) {
	gdk_draw_drawable(gtk_widget_get_window(field), stdgc, offscreen, 0, 0, 0, 0,
			screensize, screensize);
}

static void
gtk_ui_draw_image(Picture *pict, int x, int y) {
	gdk_gc_set_clip_origin(pict->gc, x, y);
	gdk_draw_drawable(offscreen, pict->gc, pict->pix, 0, 0, x, y,
			pict->width, pict->height);
}

static void
gtk_ui_draw_line(int x1, int y1, int x2, int y2) {
	gdk_draw_line(offscreen, stdgc, x1, y1, x2, y2);
}

static void
gtk_ui_draw_string(const char *str, int x, int y) {
	layout = gtk_widget_create_pango_layout(GTK_WIDGET(field), str);
	gdk_draw_layout(offscreen, stdgc, x, y-15, layout);
	g_object_unref(layout);
}

/*
 * Timer operations
 */

static void
gtk_ui_start_timer(int ms) {
	if (timer == 0)
		timer = g_timeout_add(ms, timer_tick, NULL);
}

static void
gtk_ui_stop_timer(void) {
	if (timer != 0)
		g_source_remove(timer);
	timer = 0;
}

static int
gtk_ui_timer_active(void) {
	return (!!timer);
}

/*
 * Main Loop 
 */
static void
gtk_ui_main_loop(void) {
	gtk_main();
}

/*
 * Initialization
 */
static void
gtk_ui_initialize(int *argc, char **argv) {
	struct stat stats;

	gtk_init(argc, &argv);
	toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	timer = 0;
	gtk_window_set_title(GTK_WINDOW(toplevel), "XBill");

	g_signal_connect(G_OBJECT(toplevel), "delete_event",
			   G_CALLBACK(quit_game), NULL);

	if (stat(IMAGES, &stats) == 0)
		pictdir = IMAGES;
	else
		pictdir = ".";
}

static GtkWidget *
new_menu_item(GtkWidget *menu, int dialog) {
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label(UI_menu_string(dialog));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
				  G_CALLBACK(gtk_ui_popup_dialog), (gpointer) dialog);
	return (menu_item);
}

static GtkWidget *
CreateMenuBar(void) {
	GtkWidget *menubar;
	GtkWidget *game_item, *game_menu;
	GtkWidget *info_item, *info_menu;

	menubar = gtk_menu_bar_new();

	game_item = gtk_menu_item_new_with_label("Game");
	game_menu = gtk_menu_new();

	new_menu_item(game_menu, DIALOG_NEWGAME);
	pausebutton = new_menu_item(game_menu, DIALOG_PAUSEGAME);
	new_menu_item(game_menu, DIALOG_WARPLEVEL);
	new_menu_item(game_menu, DIALOG_HIGHSCORE);
	new_menu_item(game_menu, DIALOG_QUITGAME);

	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), game_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(game_item), game_menu);

	info_item = gtk_menu_item_new_with_label("Info");
	info_menu = gtk_menu_new();

	new_menu_item(info_menu, DIALOG_STORY);
	new_menu_item(info_menu, DIALOG_RULES);
	new_menu_item(info_menu, DIALOG_ABOUT);

	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), info_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(info_item), info_menu);

	return menubar;
}

static GtkWidget *
CreateDrawingArea(int width, int height) {
	GtkWidget *w = gtk_drawing_area_new();
	gtk_widget_set_size_request(w, width, height);
	return w;
}

static void
gtk_ui_make_main_window(int size) {
	GdkWindowHints flags;
	GdkGeometry geom;
	gint winwidth, winheight;

	screensize = size;

	base = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(toplevel), base);

	menubar = CreateMenuBar();
	gtk_box_pack_start(GTK_BOX(base), menubar, FALSE, FALSE, 0);

	field = CreateDrawingArea(size, size);
	gtk_box_pack_start(GTK_BOX(base), field, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(field), "button-press-event",
			   G_CALLBACK(button_press), NULL);
	g_signal_connect(G_OBJECT(field), "button-release-event",
			   G_CALLBACK(button_release), NULL);
	g_signal_connect(G_OBJECT(field), "enter-notify-event",
			   G_CALLBACK(enter_window), NULL);
	g_signal_connect(G_OBJECT(field), "leave-notify-event",
			   G_CALLBACK(leave_window), NULL);
	g_signal_connect(G_OBJECT(field), "expose-event",
			   G_CALLBACK(redraw_window), NULL);
	gtk_widget_set_events(field, GDK_BUTTON_PRESS_MASK |
			      GDK_BUTTON_RELEASE_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_LEAVE_NOTIFY_MASK |
			      GDK_EXPOSURE_MASK);

	gtk_widget_show_all(toplevel);

	winwidth = gdk_window_get_width(gtk_widget_get_window(toplevel));
	winheight = gdk_window_get_height(gtk_widget_get_window(toplevel));
	geom.min_width = geom.max_width = geom.base_width = winwidth;
	geom.min_height = geom.max_height = geom.base_height = winheight;
	geom.width_inc = geom.height_inc = 0;
	flags = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_BASE_SIZE |
		GDK_HINT_RESIZE_INC;
	gdk_window_set_geometry_hints(gtk_widget_get_window(toplevel), &geom, flags);

	gdk_color_parse("white", &white);
	gdk_color_parse("black", &black);
}

static void
gtk_ui_graphics_init(void) {
	offscreen = (GdkPixmap *)gdk_pixmap_new(gtk_widget_get_window(field), screensize, screensize, -1);
	stdgc = (GdkGC *)gdk_gc_new(offscreen);
	gdk_gc_set_exposures(stdgc, FALSE);
	gdk_gc_set_line_attributes(stdgc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND,
				   GDK_JOIN_MITER);
}

static GtkWidget *
new_button(GtkWidget *dialog, const char *text, GCallback func,
	   GObject *obj)
{
	GtkWidget *button = gtk_button_new_with_label(text);
	if (func != NULL)
		g_signal_connect_swapped(G_OBJECT(button), "clicked",
					  func, obj);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
				  G_CALLBACK(gtk_widget_hide),
				  G_OBJECT(dialog));
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
				  G_CALLBACK(popdown), NULL);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_action_area(GTK_DIALOG(dialog))),
			  button);
	gtk_widget_show(button);
	return button;
}

static void
CreateDialog(int index, int hascancel, Picture *icon,
	     const char *buttonlabel, GCallback func)
{
	GtkWidget *dialog, *pixmap, *label, *hbox;

	dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	hbox = gtk_hbox_new(FALSE, 0);

	if (icon != NULL) {
		pixmap = (GtkWidget *)gtk_pixmap_new(icon->pix, icon->mask);
		gtk_container_add(GTK_CONTAINER(hbox), pixmap);
	}

	label = gtk_label_new(UI_dialog_string(index));
	gtk_container_add(GTK_CONTAINER(hbox), label);

	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox);
	gtk_widget_show_all(hbox);

	if (buttonlabel == NULL)
		buttonlabel = "OK";
	new_button(dialog, buttonlabel, func, NULL);

	if (hascancel)
		new_button(dialog, "Cancel", NULL, NULL);

	gtk_widget_realize(dialog); 
	dialogs[index] = dialog;
}

static void
CreateEnterText(int index, GCallback func) {
	GtkWidget *dialog, *label, *entry;

	dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	label = gtk_label_new(UI_dialog_string(index));
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 20);
	g_signal_connect_swapped(G_OBJECT(entry), "activate",
				  func, G_OBJECT(entry));
	g_signal_connect_swapped(G_OBJECT(entry), "activate",
				  G_CALLBACK(gtk_widget_hide),
				  G_OBJECT(dialog));
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
	gtk_widget_show(entry);

	new_button(dialog, "OK", func, G_OBJECT(entry));

	gtk_widget_realize(dialog); 
	dialogs[index] = dialog;
}

static void
CreatePixmapBox(int index, Picture *logo, Picture *pix) {
	GtkWidget *dialog, *pixmap, *label;
	const char *text = UI_dialog_string(index);

	dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	pixmap = (GtkWidget *)gtk_pixmap_new(logo->pix, logo->mask);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), pixmap);
	gtk_widget_show(pixmap);

	if (pix != NULL) {
		pixmap = (GtkWidget *)gtk_pixmap_new(pix->pix, pix->mask);
		gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
				  pixmap);
		gtk_widget_show(pixmap);
	}

	if (text != NULL) {
		label = gtk_label_new(text);
		gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
				  label);
		gtk_widget_show(label);
	}

	new_button(dialog, "OK", NULL, NULL);

	gtk_widget_realize(dialog); 
	dialogs[index] = dialog;
}

static void
gtk_ui_create_dialogs(Picture *logo, Picture *icon, Picture *about) {
	CreateDialog(DIALOG_NEWGAME, 1, NULL, NULL, new_game);
	CreateDialog(DIALOG_PAUSEGAME, 0, icon, "Continue", NULL);
	CreateEnterText(DIALOG_WARPLEVEL, G_CALLBACK(warp_apply));
	CreateDialog(DIALOG_HIGHSCORE, 0, NULL, NULL, NULL);
	CreateDialog(DIALOG_QUITGAME, 1, NULL, NULL, quit_game);

	CreatePixmapBox(DIALOG_STORY, logo, NULL);
	CreatePixmapBox(DIALOG_RULES, logo, NULL);
	CreatePixmapBox(DIALOG_ABOUT, logo, about);

	CreateDialog(DIALOG_SCORE, 0, NULL, NULL, NULL);
	CreateDialog(DIALOG_ENDGAME, 0, NULL, "Nuts!", NULL);
	CreateEnterText(DIALOG_ENTERNAME, G_CALLBACK(enter_name));
}

static void
set_label(GtkWidget *dialog, const char *str) {
	GList *list;
	GtkWidget *hbox = NULL;

	list = (GList *)gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))));
	while (list != NULL) {
		GtkWidget *w = (GtkWidget *) list->data;
		list = g_list_next(list);
		if (GTK_IS_HBOX(w)) {
			hbox = w;
			break;
		}
	}
	if (hbox == NULL)
		return;
	list = (GList *)gtk_container_get_children(GTK_CONTAINER(hbox));
	while (list != NULL) {
		GtkWidget *w = (GtkWidget *) list->data;
		list = g_list_next(list);
		if (GTK_IS_LABEL(w)) {
			gtk_label_set_text(GTK_LABEL(w), str);
			return;
		}
	}
}

static void
gtk_ui_update_dialog(int index, const char *str) {
	set_label(dialogs[index], str);
}

static void
gtk_ui_set_pausebutton(int active) {
	if (pausebutton != NULL)
		gtk_widget_set_sensitive(pausebutton, active);
}

static struct UI_methods gtk_methods = {
	gtk_ui_set_cursor,
	gtk_ui_load_cursor,
	gtk_ui_load_picture,
	gtk_ui_set_icon,
	gtk_ui_picture_width,
	gtk_ui_picture_height,
	gtk_ui_graphics_init,
	gtk_ui_clear_window,
	gtk_ui_refresh_window,
	gtk_ui_draw_image,
	gtk_ui_draw_line,
	gtk_ui_draw_string,
	gtk_ui_start_timer,
	gtk_ui_stop_timer,
	gtk_ui_timer_active,
	gtk_ui_popup_dialog,
	gtk_ui_main_loop,
	gtk_ui_initialize,
	gtk_ui_make_main_window,
	gtk_ui_create_dialogs,
	gtk_ui_set_pausebutton,
	gtk_ui_update_dialog,
};

void
gtk_ui_setmethods(UI_methods **methodsp) {
	*methodsp = &gtk_methods;
}
