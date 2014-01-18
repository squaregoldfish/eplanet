#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_config_filesel.h"

typedef struct _E_Filesel_Data E_Filesel_Data;
struct _E_Filesel_Data
{
   Evas_Object *bg_obj, *content_obj, *buttons_obj;
   Evas_Object *filesel;
};

/* Local Function Prototypes */

/*
 * Process Cancel button on viewpos file selection dialog
 */
static void _filesel_cancel(void *data, void *data2);

/*
 * Process OK button press on viewpos file selection dialog
 */
static void _filesel_ok(void *data, void *data2);

/*
 * Display a popup dialog with an error message
 */
static void _show_error(char *message);

/*
 * Show the file selection dialog for the viewpos file location.
 */
E_Win *
e_mod_config_filesel(E_Config_Dialog *parent, const char *path)
{
   E_Border *existing_filesel;
   E_Win *win;
   Evas *evas;
   Evas_Object *o = NULL;
   E_Filesel_Data *filesel_data;
   int width, height;

   // If the filesel dialog has already been created, show it
   existing_filesel = _find_existing_filesel();
   if (existing_filesel)
   {
      e_border_raise(existing_filesel);
      e_border_focus_set(existing_filesel, 1, 1);
      return NULL;
   }

   // ...otherwise create the dialog from scratch
   if (parent) win = e_win_new(parent->con);
   else win = e_win_new(e_container_current_get(e_manager_current_get()));
   if (!win) return NULL;

   filesel_data = E_NEW(E_Filesel_Data, 1);

   // Main window properties
   e_win_title_set(win, D_("Select File"));
   e_win_name_class_set(win, "E", FILESEL_WIN_CLASS);
   e_win_delete_callback_set(win, _filesel_cancel);
   e_win_dialog_set(win, 1);

   evas = e_win_evas_get(win);
   o = edje_object_add(evas);
   filesel_data->bg_obj = o;
   e_theme_edje_object_set(o, "base/theme/dialog", "e/widgets/dialog/main");
   evas_object_move(o, 0, 0);
   evas_object_show(o);

   // Main dialog content
   o = e_widget_table_add(evas, 0);
   filesel_data->content_obj = o;

   // File selecter
   if (!path || strlen(path) == 0)
      o = e_widget_fsel_add(evas, "/", e_user_homedir_get(), NULL, NULL, NULL, NULL, NULL, NULL, 0);
   else
      o = e_widget_fsel_add(evas, "/", ecore_file_dir_get(path),
            NULL, NULL, NULL, NULL, NULL, NULL, 0);

   e_widget_table_object_append(filesel_data->content_obj, o, 0, 0, 1, 1, 1, 1,
         1, 1);
   filesel_data->filesel = o;


   o = filesel_data->content_obj;
   e_widget_size_min_get(o, &width, &height);
   edje_extern_object_min_size_set(o, width, height);
   edje_object_part_swallow(filesel_data->bg_obj, "e.swallow.content", o);

   // Buttons
   o = e_widget_list_add(evas, 1, 1);
   filesel_data->buttons_obj = o;

   // OK button
   o = e_widget_button_add(evas, D_("OK"), NULL, _filesel_ok, win, parent);
   e_widget_list_object_append(filesel_data->buttons_obj, o, 1, 0, 0.5);

   // Cancel button
   o = e_widget_button_add(evas, D_("Cancel"), NULL, _filesel_cancel, win, parent);
   e_widget_list_object_append(filesel_data->buttons_obj, o, 1, 0, 0.5);

   // Add buttons to canvas
   o = filesel_data->buttons_obj;
   e_widget_size_min_get(o, &width, &height);
   edje_extern_object_min_size_set(o, width, height);
   edje_object_part_swallow(filesel_data->bg_obj, "e.swallow.buttons", o);

   // Calculate window position and size
   e_win_centered_set(win, 1);

   edje_object_size_min_calc(filesel_data->bg_obj, &width, &height);
   evas_object_resize(filesel_data->bg_obj, width, height);
   e_win_resize(win, width, height);
   e_win_size_min_set(win, width, height);
   e_win_size_max_set(win, width, height);

   // Show window and set focus
   e_win_show(win);

   if (!e_widget_focus_get(filesel_data->bg_obj)) e_widget_focus_set(
         filesel_data->buttons_obj, 1);

   win->data = filesel_data;

   return win;
}

/*
 * Process Cancel button on viewpos file selection dialog
 */
static void _filesel_cancel(void *data, void *data2)
{
   E_Object *win;
   E_Filesel_Data *filesel_data;

   win = data;
   filesel_data = win->data;

   // Destroy window and its data
   if (filesel_data)
   {
      E_FREE(filesel_data);
      win->data = NULL;
   }

   e_object_del(win);
}

/*
 * Process OK button press on viewpos file selection dialog
 */
static void _filesel_ok(void *data, void *data2)
{
   E_Win *win;
   E_Filesel_Data *filesel_data;
   E_Config_Dialog *parent;
   const char *path;

   win = data;
   parent = data2;
   filesel_data = win->data;

   // Get the file from the config dialog, and verify it.
   path = e_widget_fsel_selection_path_get(filesel_data->filesel);

   if (!path)
      _show_error("You must select a file.");
   else if (ecore_file_is_dir(path))
      _show_error("Selected file is a directory.");
   else
   {
      // Add the selected file to the main config dialog and close the window
      e_win_hide(win);
      set_viewpos_path(parent, e_widget_fsel_selection_path_get(filesel_data->filesel));
      _filesel_cancel(win, NULL);
   }
}

/*
 * Display a popup dialog with an error message
 */
static void _show_error(char *message)
{
   E_Dialog *error_popup;

   error_popup = e_dialog_new(e_container_current_get(
         e_manager_current_get()), "eplanet_error", "eplanet/error");
   e_dialog_title_set(error_popup, "Configuration error");
   e_dialog_text_set(error_popup, message);
   e_dialog_button_add(error_popup, D_("OK"), NULL, NULL, NULL);
   e_dialog_show(error_popup);

}

/*
 * See if a file selection dialog has already been registered.
 */
E_Border *_find_existing_filesel()
{
   const Eina_List *l;
   E_Border *bd = NULL;
   int found = 0;

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
   {
      if (bd->client.icccm.name)
      {
         if (!strcmp(bd->client.icccm.name, FILESEL_WIN_CLASS)) found = 1;
      }
      if (bd->client.icccm.class)
      {
         if (!strcmp(bd->client.icccm.class, FILESEL_WIN_CLASS)) found = 1;
      }
   }

   return bd;
}
