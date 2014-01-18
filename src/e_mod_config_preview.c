#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config_preview.h"

typedef struct _E_Preview_Data E_Preview_Data;
struct _E_Preview_Data
{
   Evas_Object *bg_obj, *content_obj, *buttons_obj;
   Evas_Object *cmd_textblock, *preview_image, *wait_message;
};

/* Local Function Prototypes */
static void _calculate_preview_size(int *width, int *height);
static void _close_preview(void *data, void *data2);

/*
 * Create and/or show the preview dialog
 */
E_Win *
e_mod_config_preview(E_Config_Dialog *parent)
{
   E_Border *existing_preview;
   E_Win *win;
   Evas *evas;
   Evas_Object *o = NULL;
   E_Preview_Data *preview_data;
   int preview_width = 800;
   int preview_height = 600;
   int width, height;

   // See if the preview window already exists. If so, show it.
   existing_preview = _find_existing_preview();
   if (existing_preview)
   {
      e_border_raise(existing_preview);
      e_border_focus_set(existing_preview, 1, 1);
      return NULL;
   }

   // ...otherwise create it from scratch
   if (parent) win = e_win_new(parent->con);
   else win = e_win_new(e_container_current_get(e_manager_current_get()));
   if (!win) return NULL;

   preview_data = E_NEW(E_Preview_Data, 1);

   // Main window properties
   e_win_title_set(win, D_("EPlanet Preview"));
   e_win_name_class_set(win, "E", PREV_WIN_CLASS);
   e_win_delete_callback_set(win, _close_preview);
   e_win_dialog_set(win, 1);

   evas = e_win_evas_get(win);
   o = edje_object_add(evas);
   preview_data->bg_obj = o;
   e_theme_edje_object_set(o, "base/theme/dialog", "e/widgets/dialog/main");
   evas_object_move(o, 0, 0);
   evas_object_show(o);

   // Main dialog content
   o = e_widget_table_add(evas, 0);
   preview_data->content_obj = o;
   o = e_widget_label_add(evas, D_("XPlanet command line:"));
   e_widget_table_object_append(preview_data->content_obj, o, 0, 0, 1, 1, 1, 1,
         1, 1);

   // XPlanet command display
   o = e_widget_textblock_add(evas);
   e_widget_size_min_set(o, 0, 45);
   preview_data->cmd_textblock = o;
   e_widget_table_object_append(preview_data->content_obj, o, 0, 1, 2, 1, 1, 1,
         1, 1);

   // Preview image display
   o = e_widget_label_add(evas, D_("Preview image:"));
   e_widget_table_object_append(preview_data->content_obj, o, 0, 2, 2, 1, 1, 1,
         1, 1);

   _calculate_preview_size(&preview_width, &preview_height);
   o = e_widget_image_add_from_file(evas, NULL, preview_width, preview_height);
   preview_data->preview_image = o;
   e_widget_table_object_append(preview_data->content_obj, o, 0, 3, 2, 1, 1, 1,
         1, 1);

   // To begin with, show a holding message while the preview image is created
   o = e_widget_label_add(evas, D_("Generating preview..."));
   e_widget_size_min_set(o, preview_width, preview_height);
   e_widget_table_object_append(preview_data->content_obj, o, 0, 3, 2, 1, 1, 1,
         1, 1);
   preview_data->wait_message = o;

   // Add preview pane to canvas
   o = preview_data->content_obj;
   e_widget_size_min_get(o, &width, &height);
   edje_extern_object_min_size_set(o, width, height);
   edje_object_part_swallow(preview_data->bg_obj, "e.swallow.content", o);

   // Buttons
   o = e_widget_list_add(evas, 1, 1);
   preview_data->buttons_obj = o;

   // Close button
   o = e_widget_button_add(evas, D_("Close"), NULL, _close_preview, win, NULL);
   e_widget_list_object_append(preview_data->buttons_obj, o, 1, 0, 0.5);

   // Add buttons to canvas
   o = preview_data->buttons_obj;
   e_widget_size_min_get(o, &width, &height);
   edje_extern_object_min_size_set(o, width, height);
   edje_object_part_swallow(preview_data->bg_obj, "e.swallow.buttons", o);

   e_win_centered_set(win, 1);

   // Calculate window properties and show
   edje_object_size_min_calc(preview_data->bg_obj, &width, &height);
   evas_object_resize(preview_data->bg_obj, width, height);
   e_win_resize(win, width, height);
   e_win_size_min_set(win, width, height);
   e_win_size_max_set(win, width, height);
   e_win_show(win);

   if (!e_widget_focus_get(preview_data->bg_obj)) e_widget_focus_set(
         preview_data->buttons_obj, 1);

   win->data = preview_data;

   return win;
}

/*
 * Close the preview window
 */
static void _close_preview(void *data, void *data2)
{
   E_Win *win;

   win = data;

   // If the preview has been created (or at least started)
   // its values will be in the main config. Discard them.
   if (eplanet_conf->xplanet_filename)
   {
      ecore_file_unlink(eplanet_conf->xplanet_filename);
      eplanet_conf->xplanet_filename = NULL;
   }

   eplanet_conf->preview_pid = -1;

   // Free data
   if (win->data)
   {
      E_FREE(win->data);
      win->data = NULL;
   }

   // Destroy the window
   e_object_del((E_Object *) win);
}

/*
 * Generate the preview image
 */
void do_generate_preview(E_Win *win, Xplanet_Config *xplanet_config)
{
   E_Preview_Data *preview_data;
   int width, height;
   char cmd[PATH_MAX];
   char filename[PATH_MAX];
   Eina_List *ignored = NULL;
   char *tmpdir;
   char *lastChar;

   preview_data = win->data;

   /* Retrieve the temporary directory location */
   if (!tmpdir) {
	   // I suspect this method of getting the dir name will get me spanked.
	   tmpdir = strdup(getenv("E_IPC_SOCKET"));

	   // Find the last / character
	   lastChar = strrchr(tmpdir, '/');

	   // Replace it with null to terminate the string
	   *lastChar = '\0';
	}


   // Build the XPlanet command
   _calculate_preview_size(&width, &height);
   sprintf(filename, "%s/eplanet_preview.png", tmpdir);
   _build_xplanet_command(xplanet_config, width, height, cmd, filename, ignored);
   e_widget_textblock_plain_set(preview_data->cmd_textblock, cmd);

   // Register xplanet exit handler and run
   if (!eplanet_conf->preview_exe_del)
      eplanet_conf->preview_exe_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
         _draw_preview, preview_data);

   _run_xplanet(cmd, filename, PREVIEW_MODE);
}

/*
 * Calculate the size of the preview image.
 * The preview has the same aspect ratio as the screen it's on,
 * smaller than the screen and a maximum of 800 pixels
 */
static void _calculate_preview_size(int *width, int *height)
{
   E_Container *container;
   E_Zone *zone;
   int calc_width = 800;
   int calc_height = 800;
   float calc_aspect, zone_aspect;

   container = e_container_current_get(e_manager_current_get());
   zone = e_zone_current_get(container);
   if ((zone->w - 100) < calc_width) calc_width = zone->w - 100;

   if ((zone->h - 300) < calc_height) calc_height = zone->h - 300;

   zone_aspect = (float) zone->w / (float) zone->h;
   calc_aspect = (float) calc_width / (float) calc_height;
   if (calc_aspect > zone_aspect) calc_width = calc_height * zone_aspect;
   else calc_height = calc_width / zone_aspect;

   *width = calc_width;
   *height = calc_height;
}

/*
 * Draw the preview image in the dialog
 */
Eina_Bool
_draw_preview(void *data, int type, void *event)
{
   E_Preview_Data *preview_data;
   Ecore_Exe_Event_Del *ev;
   pid_t end_pid;

   preview_data = data;
   ev = event;
   end_pid = ecore_exe_pid_get(ev->exe);

   if (end_pid == eplanet_conf->preview_pid)
   {
      e_widget_image_file_set(preview_data->preview_image,
            eplanet_conf->xplanet_filename);
      e_widget_label_text_set(preview_data->wait_message, "");

      // Remove the pid from config
      eplanet_conf->preview_pid = -1;

      return ECORE_CALLBACK_DONE;
   }

   return ECORE_CALLBACK_PASS_ON;
}

/*
 * Locate an existing preview dialog
 */
E_Border *_find_existing_preview()
{
   const Eina_List *l;
   E_Border *bd = NULL;
   int found = 0;

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
   {
      if (bd->client.icccm.name)
      {
         if (!strcmp(bd->client.icccm.name, PREV_WIN_CLASS)) found = 1;
      }
      if (bd->client.icccm.class)
      {
         if (!strcmp(bd->client.icccm.class, PREV_WIN_CLASS)) found = 1;
      }
   }

   return bd;
}
