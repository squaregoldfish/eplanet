#include <stdlib.h>
#include <regex.h>
#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_config_preview.h"
#include "e_mod_config_filesel.h"
#include "sys/types.h"
#include "sys/sysinfo.h"

struct _E_Config_Dialog_Data
{
   int delay;
   int startDelay;
   int taskPriority;
   double loadLimit;
   int minRamLimit;
   int disableOnBattery;

   struct
   {
      Evas_Object *o_body_ilist, *o_source_ilist;
      Evas_Object *o_origin_toggle, *o_projection_toggle;
      Evas_Object *o_viewpos_default, *o_viewpos_latlon, *o_viewpos_random, *o_viewpos_file;
      Evas_Object *o_viewpos_lat_label, *o_viewpos_lon_label;
      Evas_Object *o_viewpos_lat, *o_viewpos_lon;
      Evas_Object *o_viewpos_file_val, *o_viewpos_file_button;
      Evas_Object *o_use_localtime, *o_localtime;
      Evas_Object *o_show_label, *o_label_text;
      Evas_Object *o_label_time_local_toggle, *o_label_time_gmt_toggle;
      Evas_Object *o_label_pos_tl_toggle, *o_label_pos_tr_toggle;
      Evas_Object *o_label_pos_bl_toggle, *o_label_pos_br_toggle;
      Evas_Object *o_label_pos_other_toggle;
      Evas_Object *o_label_pos_other_text;
      Evas_Object *o_config_check, *o_config_name;
      Evas_Object *o_extra_options;
      int show_label, use_config, localtime;
   } gui;

   int current_config;
   Xplanet_Config local_xplanet;

   int config_updated;

   // Temp vars for selecting list items
   int body_item;
   int source_item;

   // File selection dialog
   E_Dialog *viewpos_file_select_dialog;
};

/* Local Function Prototypes */
static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static void _fill_data(E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas,
      E_Config_Dialog_Data *cfdata);
static int _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

static void _populate_xplanet_page(E_Config_Dialog_Data *cfdata,
      int config_index);
static void _fill_planet_ilist(Evas_Object *ilist, const char *value,
      void(*callback)(void *data), E_Config_Dialog_Data *cfdata, int *store_item);
static void _fill_projection_ilist(Evas_Object *ilist, const char *value,
      E_Config_Dialog_Data *cfdata, int *store_item);
static void _cb_source_type(void *data, Evas_Object *obj, void *event_info);
static void _cb_target(void *data);
static void _cb_source(void *data);
static void _cb_viewpos(void *data, Evas_Object *obj, void *event_info);
static void _cb_label_pos(void *data, Evas_Object *obj, void *event_info);
static void _cb_show_label(void *data, Evas_Object *obj, void *event_info);
static void _cb_set_localtime(void *data, Evas_Object *obj, void *event_info);
static void _add_ilist_entry(Evas_Object *ilist, const char *label,
      const char *list_value, const char *check_value, void(*callback)(
            void *data), E_Config_Dialog_Data *cfdata, int count, int *store_item);
static void _show_preview(void *data, void *data2);
static int parse_config_gui(E_Config_Dialog_Data *cfdata);
static Eina_Bool _select_list_items(void *data);
static void _select_viewpos_file(void *data, void *data2);

/* External Functions */

/* Function for calling our personal dialog menu */
E_Config_Dialog *
e_int_config_eplanet_module(E_Container *con, const char *params)
{
   E_Config_Dialog *cfd = NULL;
   E_Config_Dialog_View *v = NULL;
   char buf[4096];

   /* is this config dialog already visible ? */
   if (e_config_dialog_find("EPlanet", CONFIG_WIN_CLASS)) return NULL;

   // Disable the timer so xplanet won't run while the config dialog is open
   if (eplanet_conf->bg_set_timer)
   {
      ecore_timer_del(eplanet_conf->bg_set_timer);
      eplanet_conf->bg_set_timer = NULL;
   }

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return NULL;

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.create_widgets = _basic_create;
   v->basic.apply_cfdata = _basic_apply;

   /* Icon in the theme */
   snprintf(buf, sizeof(buf), "%s/e-module-eplanet.edj",
         eplanet_conf->module->dir);

   /* create our config dialog */
   cfd = e_config_dialog_new(con, D_("EPlanet Configuration"), "EPlanet",
         "advanced/eplanet", buf, 0, v, NULL);

   e_dialog_resizable_set(cfd->dia, 1);
   eplanet_conf->cfd = cfd;

   return cfd;
}

/*
 * Create and populate a new config data object
 */
static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata = NULL;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   _fill_data(cfdata);
   return cfdata;
}

/*
 * Tidy up a config data object ready to destroy it
 */
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   int timer_time = cfdata->delay * 60;

   // Reinstate the bg set timer
   if (cfdata->config_updated) timer_time = 1;

   eplanet_conf->bg_set_timer = ecore_timer_add(timer_time,
         _start_background_update, NULL);

   eplanet_conf->cfd = NULL;
   E_FREE(cfdata);
}

/*
 * Populate a config data object from the user's preferences
 * which are stored in the main module object as eplanet_conf
 */
static void _fill_data(E_Config_Dialog_Data *cfdata)
{
   /* load a temp copy of the config variables */
   cfdata->delay = eplanet_conf->delay;
   cfdata->startDelay = eplanet_conf->startDelay;
   cfdata->taskPriority = eplanet_conf->taskPriority;
   cfdata->loadLimit = eplanet_conf->loadLimit;
   cfdata->minRamLimit = eplanet_conf->minRamLimit;
   cfdata->disableOnBattery = eplanet_conf->disableOnBattery;

   cfdata->current_config = 0;
   cfdata->config_updated = 0;
}

/*
 * Build the config dialog
 */
static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *list = NULL, *table = NULL, *subtable = NULL,
         *framelist = NULL, *widget = NULL, *toolbook = NULL;
   E_Radio_Group *rg;
   struct sysinfo meminfo;
   long long memSize;

   // Remove the update timer - has the effect of
   // aborting any current updates
   if (eplanet_conf->bg_set_timer) {
	   ecore_timer_del(eplanet_conf->bg_set_timer);
   }

   // Get total physical RAM for free mem slider
   // Limit to 1024Mb
   sysinfo(&meminfo);
   memSize = meminfo.totalram;
   memSize = memSize / 1024 / 1024;
   if (memSize > 1024)
      memSize = 1024;

   toolbook = e_widget_toolbook_add(evas, 48 * e_scale, 48 * e_scale);

   /* Behaviour Tab */
   list = e_widget_list_add(evas, 0, 0);
   widget = e_widget_label_add(evas, D_("Update background every"));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
   widget = e_widget_slider_add(evas, 1, 0, ("%2.0f minutes"), 1, 60, 1, 0,
         NULL, &(cfdata->delay), 200);
   e_widget_list_object_append(list, widget, 1, 1, 0.5);

   widget = e_widget_label_add(evas, D_("Delay first background change for"));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
   widget = e_widget_slider_add(evas, 1, 0, ("%3.0f seconds"), 1, 120, 1, 0,
         NULL, &(cfdata->startDelay), 200);
   e_widget_list_object_append(list, widget, 1, 1, 0.5);

   widget = e_widget_label_add(evas,
         D_("Run XPlanet using priority"));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
   widget = e_widget_slider_add(evas, 1, 0, ("%1.0f"), ecore_exe_run_priority_get(), 19, 1, 0,
         NULL, &(cfdata->taskPriority), 200);
   e_widget_list_object_append(list, widget, 1, 1, 0.5);

   widget = e_widget_label_add(evas,
         D_("Don't run if system load is greater than"));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
   widget = e_widget_slider_add(evas, 1, 0, ("%3.1f"), 1, 32, 0.5, 0,
         &(cfdata->loadLimit), NULL, 200);
   e_widget_list_object_append(list, widget, 1, 1, 0.5);

   widget = e_widget_label_add(evas,
         D_("Don't run if available RAM is less than"));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
   widget = e_widget_slider_add(evas, 1, 0, ("%3.0f Mb"), 0, memSize, 16, 0, NULL,
         &(cfdata->minRamLimit), 200);
   e_widget_list_object_append(list, widget, 1, 1, 0.5);

/*
   widget = e_widget_check_add(evas,
         D_("Do not change background if on battery power"),
         &(cfdata->disableOnBattery));
   e_widget_list_object_append(list, widget, 1, 1, 0.5);
*/

   // Add behaviour tab to dialog
   e_widget_toolbook_page_append(toolbook, NULL, D_("Behaviour"), list, 0, 0,
         0, 0, 0.5, 0.0);

   /* XPlanet Tab */
   table = e_widget_table_add(evas, 0);

   // Target
   framelist = e_widget_framelist_add(evas, D_("Target"), 0);

   widget = e_widget_ilist_add(evas, 16, 16, NULL);
   cfdata->gui.o_body_ilist = widget;
   e_widget_ilist_multi_select_set(widget, 0);
   e_widget_size_min_set(widget, 160, 150);

   e_widget_framelist_object_append(framelist, widget);
   e_widget_table_object_append(table, framelist, 0, 0, 1, 1, 1, 1, 0, 1);

   // View Type
   framelist = e_widget_framelist_add(evas, D_("View Type"), 0);

   rg = e_widget_radio_group_new(&(cfdata->local_xplanet.source_type));
   widget = e_widget_radio_add(evas, D_("Origin"), SOURCE_ORIGIN, rg);
   cfdata->gui.o_origin_toggle = widget;
   evas_object_smart_callback_add(widget, "changed", _cb_source_type, cfdata);
   e_widget_framelist_object_append(framelist, widget);

   widget = e_widget_radio_add(evas, D_("Projection"), SOURCE_PROJECTION, rg);
   cfdata->gui.o_projection_toggle = widget;
   evas_object_smart_callback_add(widget, "changed", _cb_source_type, cfdata);
   e_widget_framelist_object_append(framelist, widget);

   widget = e_widget_ilist_add(evas, 16, 16, NULL);
   cfdata->gui.o_source_ilist = widget;
   e_widget_ilist_multi_select_set(widget, 0);
   e_widget_size_min_set(widget, 160, 150);
   e_widget_framelist_object_append(framelist, widget);

   e_widget_table_object_append(table, framelist, 1, 0, 1, 1, 1, 1, 0, 1);

   // Label
   framelist = e_widget_framelist_add(evas, D_("Label"), 0);
   subtable = e_widget_table_add(evas, 0);

   widget = e_widget_check_add(evas, D_("Show label"),
         &(cfdata->gui.show_label));
   evas_object_smart_callback_add(widget, "changed", _cb_show_label, cfdata);
   cfdata->gui.o_show_label = widget;
   e_widget_table_object_append(subtable, widget, 0, 0, 3, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Label text:"));
   e_widget_table_object_append(subtable, widget, 0, 1, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_label_text = widget;
   e_widget_table_object_append(subtable, widget, 1, 1, 2, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Time:"));
   e_widget_table_object_append(subtable, widget, 0, 2, 1, 1, 1, 1, 0, 1);

   rg = e_widget_radio_group_new(&(cfdata->local_xplanet.label_time));
   widget = e_widget_radio_add(evas, D_("Local"), LABEL_TIME_LOCAL, rg);
   cfdata->gui.o_label_time_local_toggle = widget;
   e_widget_table_object_append(subtable, widget, 1, 2, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("GMT"), LABEL_TIME_GMT, rg);
   cfdata->gui.o_label_time_gmt_toggle = widget;
   e_widget_table_object_append(subtable, widget, 2, 2, 1, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Position:"));
   e_widget_table_object_append(subtable, widget, 0, 3, 1, 1, 1, 1, 0, 1);

   rg = e_widget_radio_group_new(&(cfdata->local_xplanet.label_pos));
   widget = e_widget_radio_add(evas, D_("Top left"), LABEL_POS_TL, rg);
   evas_object_smart_callback_add(widget, "changed", _cb_label_pos, cfdata);
   cfdata->gui.o_label_pos_tl_toggle = widget;
   e_widget_table_object_append(subtable, widget, 1, 3, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("Top right"), LABEL_POS_TR, rg);
   evas_object_smart_callback_add(widget, "changed", _cb_label_pos, cfdata);
   cfdata->gui.o_label_pos_tr_toggle = widget;
   e_widget_table_object_append(subtable, widget, 2, 3, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("Bottom left"), LABEL_POS_BL, rg);
   evas_object_smart_callback_add(widget, "changed", _cb_label_pos, cfdata);
   cfdata->gui.o_label_pos_bl_toggle = widget;
   e_widget_table_object_append(subtable, widget, 1, 4, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("Bottom right"), LABEL_POS_BR, rg);
   evas_object_smart_callback_add(widget, "changed", _cb_label_pos, cfdata);
   cfdata->gui.o_label_pos_br_toggle = widget;
   e_widget_table_object_append(subtable, widget, 2, 4, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("Other"), LABEL_POS_OTHER, rg);
   evas_object_smart_callback_add(widget, "changed", _cb_label_pos, cfdata);
   cfdata->gui.o_label_pos_other_toggle = widget;
   e_widget_table_object_append(subtable, widget, 1, 5, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_label_pos_other_text = widget;
   e_widget_table_object_append(subtable, widget, 2, 5, 2, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Font:"));
   e_widget_table_object_append(subtable, widget, 0, 6, 1, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Not yet implemented"));
   e_widget_table_object_append(subtable, widget, 1, 6, 2, 1, 1, 1, 0, 1);

   e_widget_framelist_object_append(framelist, subtable);

   e_widget_table_object_append(table, framelist, 2, 0, 1, 1, 1, 1, 1, 1);

   // Viewing Position
   framelist = e_widget_framelist_add(evas, D_("Viewing Position"), 0);
   subtable = e_widget_table_add(evas, 0);

   rg = e_widget_radio_group_new(&(cfdata->local_xplanet.viewpos_type));

   widget = e_widget_radio_add(evas, D_("Random"), VIEWPOS_RANDOM, rg);
   cfdata->gui.o_viewpos_random = widget;
   evas_object_smart_callback_add(widget, "changed", _cb_viewpos, cfdata);
   e_widget_table_object_append(subtable, widget, 0, 0, 1, 1, 1, 1, 0, 1);

   widget = e_widget_radio_add(evas, D_("Fixed"), VIEWPOS_LATLON, rg);
   cfdata->gui.o_viewpos_latlon = widget;
   evas_object_smart_callback_add(widget, "changed", _cb_viewpos, cfdata);
   e_widget_table_object_append(subtable, widget, 0, 1, 1, 1, 1, 1, 0, 1);

   widget = e_widget_label_add(evas, D_("Latitude:"));
   cfdata->gui.o_viewpos_lat_label = widget;
   e_widget_table_object_append(subtable, widget, 1, 1, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_viewpos_lat = widget;
   e_widget_table_object_append(subtable, widget, 2, 1, 1, 1, 1, 1, 1, 1);

   widget = e_widget_label_add(evas, D_("Longitude:"));
   cfdata->gui.o_viewpos_lon_label = widget;
   e_widget_table_object_append(subtable, widget, 3, 1, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_viewpos_lon = widget;
   e_widget_table_object_append(subtable, widget, 4, 1, 1, 1, 1, 1, 1, 1);

   widget = e_widget_radio_add(evas, D_("From file"), VIEWPOS_FILE, rg);
   cfdata->gui.o_viewpos_file = widget;
   evas_object_smart_callback_add(widget, "changed", _cb_viewpos, cfdata);
   e_widget_table_object_append(subtable, widget, 0, 2, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_viewpos_file_val = widget;
   e_widget_table_object_append(subtable, widget, 1, 2, 4, 1, 1, 1, 0, 1);

   widget = e_widget_button_add(evas, D_("..."), NULL, _select_viewpos_file,
         cfd, cfdata);
   cfdata->gui.o_viewpos_file_button = widget;
   e_widget_table_object_append(subtable, widget, 5, 2, 1, 1, 1, 1, 0, 1);

   widget = e_widget_check_add(evas, D_("Set local time:"),
         &(cfdata->local_xplanet.use_localtime));
   evas_object_smart_callback_add(widget, "changed", _cb_set_localtime, cfdata);
   cfdata->gui.o_use_localtime = widget;
   e_widget_table_object_append(subtable, widget, 0, 3, 2, 1, 1, 1, 0, 1);

   widget = e_widget_slider_add(evas, 1, 0, ("%02.0f:00"), 0, 23, 1, 0, NULL,
         &(cfdata->gui.localtime), NULL);
   cfdata->gui.o_localtime = widget;
   e_widget_table_object_append(subtable, widget, 2, 3, 3, 1, 1, 1, 0, 1);

   e_widget_framelist_object_append(framelist, subtable);

   e_widget_table_object_append(table, framelist, 0, 1, 2, 1, 1, 1, 0, 1);

   // Other settings
   framelist = e_widget_framelist_add(evas, D_("Other Settings"), 0);
   subtable = e_widget_table_add(evas, 0);

   widget = e_widget_check_add(evas, D_("Use config file:"),
         &(cfdata->gui.use_config));
   cfdata->gui.o_config_check = widget;
   e_widget_table_object_append(subtable, widget, 0, 0, 1, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_config_name = widget;
   e_widget_table_object_append(subtable, widget, 1, 0, 1, 1, 1, 1, 1, 1);

   widget = e_widget_label_add(evas, D_("Other command line parameters:"));
   e_widget_table_object_append(subtable, widget, 0, 1, 2, 1, 1, 1, 0, 1);

   widget = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   cfdata->gui.o_extra_options = widget;
   e_widget_table_object_append(subtable, widget, 0, 2, 2, 1, 1, 1, 0, 1);

   e_widget_framelist_object_append(framelist, subtable);
   e_widget_table_object_append(table, framelist, 2, 1, 1, 1, 1, 1, 0, 1);

   // Preview button
//   widget = e_widget_button_add(evas, D_("Preview"), NULL, _show_preview,
//         cfd, cfdata);
//   e_widget_table_object_append(table, widget, 0, 2, 3, 1, 1, 1, 0, 1);

   e_widget_toolbook_page_append(toolbook, NULL, D_("XPlanet"), table, 0, 0, 0,
         0, 0.5, 0.0);
   e_widget_toolbook_page_show(toolbook, 0);

   _populate_xplanet_page(cfdata, 0);

   return toolbook;
}

/*
 * Store changes to the configuration made in the dialog.
 * These go back to the main config object.
 */
static int _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   Xplanet_Config *xplanet_config;
   int config_ok;

   // Pull out the value from the source ilist
   config_ok = parse_config_gui(cfdata);

   if (config_ok)
   {
	  printf("Applying config\n");

	  printf("Delay %d\n",cfdata->delay);
	  printf("Label text: %s\n", cfdata->local_xplanet.label_text);

      eplanet_conf->delay = cfdata->delay;
      eplanet_conf->startDelay = cfdata->startDelay;
      eplanet_conf->taskPriority = cfdata->taskPriority;
      eplanet_conf->loadLimit = cfdata->loadLimit;
      eplanet_conf->minRamLimit = cfdata->minRamLimit;
      eplanet_conf->disableOnBattery = cfdata->disableOnBattery;

      xplanet_config = eina_list_nth(eplanet_conf->xplanet_configs,
            cfdata->current_config);
      xplanet_config->body = cfdata->local_xplanet.body;
      xplanet_config->source_type = cfdata->local_xplanet.source_type;
      xplanet_config->origin = cfdata->local_xplanet.origin;
      xplanet_config->projection = cfdata->local_xplanet.projection;
      xplanet_config->viewpos_type = cfdata->local_xplanet.viewpos_type;
      xplanet_config->viewpos_lat = cfdata->local_xplanet.viewpos_lat;
      xplanet_config->viewpos_lon = cfdata->local_xplanet.viewpos_lon;
      xplanet_config->use_localtime = cfdata->local_xplanet.use_localtime;
      e_widget_slider_value_int_get(cfdata->gui.o_localtime, &(xplanet_config->localtime));
      xplanet_config->viewpos_file = cfdata->local_xplanet.viewpos_file;
      xplanet_config->show_label = cfdata->local_xplanet.show_label;
      xplanet_config->label_text = strdup(cfdata->local_xplanet.label_text);
      xplanet_config->label_time = cfdata->local_xplanet.label_time;
      xplanet_config->label_pos = cfdata->local_xplanet.label_pos;
      xplanet_config->label_pos_other = strdup(cfdata->local_xplanet.label_pos_other);
      xplanet_config->use_config = cfdata->local_xplanet.use_config;
      xplanet_config->config_name = strdup(cfdata->local_xplanet.config_name);
      xplanet_config->extra_options = strdup(cfdata->local_xplanet.extra_options);

      e_config_save_queue();

      cfdata->config_updated = 1;
   }
   return config_ok;
}

/*
 * Load the data for the XPlanet tab.
 * Currently there's only one config allowed, but this will
 * be increased in future.
 */
static void _populate_xplanet_page(E_Config_Dialog_Data *cfdata,
      int config_index)
{
   Xplanet_Config *xplanet_config;
   char text_value[256];

   xplanet_config = eina_list_nth(eplanet_conf->xplanet_configs, config_index);
   if (xplanet_config)
   {
      cfdata->local_xplanet.body = xplanet_config->body;
      cfdata->local_xplanet.origin = xplanet_config->origin;
      cfdata->local_xplanet.source_type = xplanet_config->source_type;
      cfdata->local_xplanet.projection = xplanet_config->projection;
      cfdata->local_xplanet.viewpos_type = xplanet_config->viewpos_type;
      cfdata->local_xplanet.viewpos_lat = xplanet_config->viewpos_lat;
      cfdata->local_xplanet.viewpos_lon = xplanet_config->viewpos_lon;
      cfdata->local_xplanet.use_localtime = xplanet_config->use_localtime;
      cfdata->local_xplanet.localtime = xplanet_config->localtime;
      cfdata->local_xplanet.viewpos_file = xplanet_config->viewpos_file;
      cfdata->local_xplanet.show_label = xplanet_config->show_label;
      cfdata->local_xplanet.label_text = xplanet_config->label_text;
      cfdata->local_xplanet.label_time = xplanet_config->label_time;
      cfdata->local_xplanet.label_pos = xplanet_config->label_pos;
      cfdata->local_xplanet.label_pos_other = xplanet_config->label_pos_other;
      cfdata->local_xplanet.use_config = xplanet_config->use_config;
      cfdata->local_xplanet.config_name = xplanet_config->config_name;
      cfdata->local_xplanet.extra_options = xplanet_config->extra_options;

      if (cfdata->local_xplanet.localtime > 23)
         cfdata->local_xplanet.localtime = 12;

      _fill_planet_ilist(cfdata->gui.o_body_ilist, cfdata->local_xplanet.body,
            _cb_target, cfdata, &(cfdata->body_item));

      if (cfdata->local_xplanet.source_type == SOURCE_ORIGIN)
      {
         _fill_planet_ilist(cfdata->gui.o_source_ilist,
               cfdata->local_xplanet.origin, _cb_source, cfdata, &(cfdata->source_item));
         e_widget_radio_toggle_set(cfdata->gui.o_origin_toggle, 1);
         e_widget_radio_toggle_set(cfdata->gui.o_projection_toggle, 0);
      }
      else
      {
         _fill_projection_ilist(cfdata->gui.o_source_ilist,
               cfdata->local_xplanet.projection, cfdata, &(cfdata->source_item));
         e_widget_radio_toggle_set(cfdata->gui.o_origin_toggle, 0);
         e_widget_radio_toggle_set(cfdata->gui.o_projection_toggle, 1);
      }

      e_widget_radio_toggle_set(cfdata->gui.o_viewpos_latlon,
            (cfdata->local_xplanet.viewpos_type == VIEWPOS_LATLON));
      e_widget_radio_toggle_set(cfdata->gui.o_viewpos_random,
            (cfdata->local_xplanet.viewpos_type == VIEWPOS_RANDOM));
      e_widget_radio_toggle_set(cfdata->gui.o_viewpos_file,
            (cfdata->local_xplanet.viewpos_type == VIEWPOS_FILE));

      sprintf(text_value, "%1g", cfdata->local_xplanet.viewpos_lat);
      e_widget_entry_text_set(cfdata->gui.o_viewpos_lat, text_value);

      sprintf(text_value, "%1g", cfdata->local_xplanet.viewpos_lon);
      e_widget_entry_text_set(cfdata->gui.o_viewpos_lon, text_value);

      sprintf(text_value, "%s", cfdata->local_xplanet.viewpos_file);
      e_widget_entry_text_set(cfdata->gui.o_viewpos_file_val, text_value);

      e_widget_check_checked_set(cfdata->gui.o_use_localtime, cfdata->local_xplanet.use_localtime);
      e_widget_slider_value_int_set(cfdata->gui.o_localtime, cfdata->local_xplanet.localtime);

      cfdata->gui.show_label = cfdata->local_xplanet.show_label;
      e_widget_check_checked_set(cfdata->gui.o_show_label,
            cfdata->gui.show_label);

      if (cfdata->local_xplanet.label_text) sprintf(text_value, "%s",
            cfdata->local_xplanet.label_text);
      else sprintf(text_value, "%s", "");

      e_widget_entry_text_set(cfdata->gui.o_label_text, text_value);

      e_widget_radio_toggle_set(cfdata->gui.o_label_time_local_toggle,
            (cfdata->local_xplanet.label_time == LABEL_TIME_LOCAL));
      e_widget_radio_toggle_set(cfdata->gui.o_label_time_gmt_toggle,
            (cfdata->local_xplanet.label_time == LABEL_TIME_GMT));

      e_widget_radio_toggle_set(cfdata->gui.o_label_pos_tl_toggle,
            (cfdata->local_xplanet.label_pos == LABEL_POS_TL));
      e_widget_radio_toggle_set(cfdata->gui.o_label_pos_tr_toggle,
            (cfdata->local_xplanet.label_pos == LABEL_POS_TR));
      e_widget_radio_toggle_set(cfdata->gui.o_label_pos_bl_toggle,
            (cfdata->local_xplanet.label_pos == LABEL_POS_BL));
      e_widget_radio_toggle_set(cfdata->gui.o_label_pos_br_toggle,
            (cfdata->local_xplanet.label_pos == LABEL_POS_BR));
      e_widget_radio_toggle_set(cfdata->gui.o_label_pos_other_toggle,
            (cfdata->local_xplanet.label_pos == LABEL_POS_OTHER));

      if (cfdata->local_xplanet.label_pos_other) sprintf(text_value, "%s",
            cfdata->local_xplanet.label_pos_other);
      else sprintf(text_value, "%s", "");

      e_widget_entry_text_set(cfdata->gui.o_label_pos_other_text, text_value);

      e_widget_check_checked_set(cfdata->gui.o_config_check,
            cfdata->local_xplanet.use_config);
      if (cfdata->local_xplanet.config_name) sprintf(text_value, "%s",
            cfdata->local_xplanet.config_name);
      else sprintf(text_value, "%s", "");

      e_widget_entry_text_set(cfdata->gui.o_config_name, text_value);

      if (cfdata->local_xplanet.extra_options) sprintf(text_value, "%s",
            cfdata->local_xplanet.extra_options);
      else sprintf(text_value, "%s", "");

      e_widget_entry_text_set(cfdata->gui.o_extra_options, text_value);

      _cb_source_type(cfdata, NULL, NULL);
      _cb_show_label(cfdata, NULL, NULL);
      _cb_label_pos(cfdata, NULL, NULL);
      _cb_set_localtime(cfdata, NULL, NULL);
   }
}

/*
 * Populate the list of planets
 * Note that this list is the same as the projection list -
 * its contents are replaced according to context.
 */
static void _fill_planet_ilist(Evas_Object *ilist, const char *value,
      void(*callback)(void *data), E_Config_Dialog_Data *cfdata, int *store_item)
{
   Evas *evas;
   int count = -1;

   evas = evas_object_evas_get(ilist);
   evas_event_freeze(evas);
   edje_freeze();
   e_widget_ilist_freeze(ilist);
   e_widget_ilist_clear(ilist);

   _add_ilist_entry(ilist, "Sun", "sun", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Mercury", "mercury", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Venus", "venus", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Earth", "earth", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Moon", "moon", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Mars", "mars", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Phobos", "phobos", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Deimos", "deimos", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Jupiter", "jupiter", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Io", "io", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Europa", "europa", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Ganymede", "ganymede", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Callisto", "callisto", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Saturn", "saturn", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Mimas", "mimas", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Enceladus", "enceladus", value, callback,
         cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Tethys", "tethys", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Dione", "dione", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Rhea", "rhea", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Titan", "titan", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Hyperion", "hyperion", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Iapetus", "iapetus", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Phoebe", "phoebe", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Uranus", "uranus", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Miranda", "miranda", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Ariel", "ariel", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Umbriel", "umbriel", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Titania", "titania", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Oberon", "oberon", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Neptune", "neptune", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Triton", "triton", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "   Nereid", "nereid", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Pluto", "pluto", value, callback, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "   Charon", "charon", value, callback, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Random", "random", value, callback, cfdata, ++count, store_item);

   e_widget_ilist_go(ilist);
   e_widget_ilist_thaw(ilist);
   edje_thaw();
   evas_event_thaw(evas);
}

/*
 * Populate the list of projections.
 * Note that this list is the same as the planet list -
 * its contents are replaced according to context.
 */
static void _fill_projection_ilist(Evas_Object *ilist, const char *value,
      E_Config_Dialog_Data *cfdata, int *store_item)
{
   Evas *evas;
   int count = -1;

   evas = evas_object_evas_get(ilist);
   evas_event_freeze(evas);
   edje_freeze();
   e_widget_ilist_freeze(ilist);
   e_widget_ilist_clear(ilist);

   _add_ilist_entry(ilist, "Ancient", "ancient", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Azimuthal", "azimuthal", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Bonne", "bonne", value, _cb_source, cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Equal Area", "equalarea", value, _cb_source,
         cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Gnomonic", "gnomonic", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Hemisphere", "hemisphere", value, _cb_source,
         cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Icosagnomonic", "icosagnomonic", value, _cb_source,
         cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Lambert", "lambert", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Mercator", "mercator", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Mollweide", "mollweide", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Orthographic", "orthographic", value, _cb_source,
         cfdata, ++count, store_item);
   _add_ilist_entry(ilist, "Peters", "peters", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Polyconic", "polyconic", value, _cb_source, cfdata,
         ++count, store_item);
   _add_ilist_entry(ilist, "Rectangular", "rectangular", value, _cb_source,
         cfdata, ++count, store_item);

   e_widget_ilist_go(ilist);
   e_widget_ilist_thaw(ilist);
   edje_thaw();
   evas_event_thaw(evas);
}

/*
 * Handler for changing the view source type
 */
static void _cb_source_type(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = data;

   // If the source type is another planet, populate the list with
   // planets and disable the viewing position stuff.
   if (cfdata->local_xplanet.source_type == SOURCE_ORIGIN)
   {
      if (e_widget_ilist_count(cfdata->gui.o_source_ilist) > 0) cfdata->local_xplanet.projection
            = eina_stringshare_add(e_widget_ilist_selected_value_get(
                  cfdata->gui.o_source_ilist));

      _fill_planet_ilist(cfdata->gui.o_source_ilist,
            cfdata->local_xplanet.origin, _cb_source, cfdata, &(cfdata->source_item));

      e_widget_disabled_set(cfdata->gui.o_viewpos_default, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_latlon, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_random, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_lat_label, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_lat, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_lon_label, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_lon, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_file, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_file_val, 1);
      e_widget_disabled_set(cfdata->gui.o_viewpos_file_button, 1);
      e_widget_disabled_set(cfdata->gui.o_use_localtime, 1);
      e_widget_disabled_set(cfdata->gui.o_localtime, 1);
   }
   else
   {
      // Populate the list with projections and enable the viewing position stuff.

      if (e_widget_ilist_count(cfdata->gui.o_source_ilist) > 0) cfdata->local_xplanet.origin
            = eina_stringshare_add(e_widget_ilist_selected_value_get(
                  cfdata->gui.o_source_ilist));

      _fill_projection_ilist(cfdata->gui.o_source_ilist,
            cfdata->local_xplanet.projection, cfdata, &(cfdata->source_item));

      e_widget_disabled_set(cfdata->gui.o_viewpos_latlon, 0);
      e_widget_disabled_set(cfdata->gui.o_viewpos_random, 0);
      e_widget_disabled_set(cfdata->gui.o_viewpos_file, 0);
      _cb_viewpos(cfdata, NULL, NULL);
      e_widget_disabled_set(cfdata->gui.o_use_localtime, 0);
      e_widget_disabled_set(cfdata->gui.o_localtime, 0);
   }

   _select_list_items(cfdata);
}

/*
 * Utility function for adding an individual entry to a list
 */
static void _add_ilist_entry(Evas_Object *ilist, const char *label,
      const char *list_value, const char *check_value, void(*callback)(
            void *data), E_Config_Dialog_Data *cfdata, int count, int *store_item)
{
   e_widget_ilist_append(ilist, NULL, label, callback, cfdata, list_value);
   if (check_value && !strcmp(list_value, check_value))
      *store_item = count;
}

/*
 * Create and display the preview dialog
 */
static void _show_preview(void *data, void *data2)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_Data *cfdata;
   E_Win *preview_dialog;
   int config_ok;

   cfd = data;
   cfdata = data2;

   // Check that the entered config is OK before proceeding.
   config_ok = parse_config_gui(cfdata);

   if (config_ok)
   {
      // Create and display the preview dialog,
      // and initiate drawing of the preview.
      preview_dialog = e_mod_config_preview(cfd);
      if (preview_dialog)
         do_generate_preview(preview_dialog, &(cfdata->local_xplanet));
   }
}

/*
 * Handle selection of a new viewing target body
 */
static void _cb_target(void *data)
{
   E_Config_Dialog_Data *cfdata;

   // Update the local config object
   cfdata = data;
   cfdata->local_xplanet.body = strdup(e_widget_ilist_selected_value_get(
         cfdata->gui.o_body_ilist));
}

/*
 * Handle a change of viewing source
 */
static void _cb_source(void *data)
{
   E_Config_Dialog_Data *cfdata;

   // Update the local config object, according to the type of viewing source
   cfdata = data;
   if (cfdata->local_xplanet.source_type == SOURCE_PROJECTION) cfdata->local_xplanet.projection
         = strdup(e_widget_ilist_selected_value_get(cfdata->gui.o_source_ilist));
   else cfdata->local_xplanet.origin = strdup(
         e_widget_ilist_selected_value_get(cfdata->gui.o_source_ilist));
}

/*
 * Handle change of viewing position
 */
static void _cb_viewpos(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Dialog_Data *cfdata;

   // Enable/disable controls according to the selection
   cfdata = data;
   e_widget_disabled_set(cfdata->gui.o_viewpos_lat,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_LATLON));
   e_widget_disabled_set(cfdata->gui.o_viewpos_lat_label,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_LATLON));
   e_widget_disabled_set(cfdata->gui.o_viewpos_lon,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_LATLON));
   e_widget_disabled_set(cfdata->gui.o_viewpos_lon_label,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_LATLON));
   e_widget_disabled_set(cfdata->gui.o_viewpos_file_val,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_FILE));
   e_widget_disabled_set(cfdata->gui.o_viewpos_file_button,
         !(cfdata->local_xplanet.viewpos_type == VIEWPOS_FILE));
}

/*
 * Handle change of label position radio group
 */
static void _cb_label_pos(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = data;

   // Disable/enable 'Other' text entry box
   e_widget_disabled_set(cfdata->gui.o_label_pos_other_text,
         !e_widget_check_checked_get(cfdata->gui.o_show_label) ||
         cfdata->local_xplanet.label_pos != LABEL_POS_OTHER);
}

/*
 * Handle toggle of Show Label checkbox. Disable/enable
 * config options as appropriate.
 */
static void _cb_show_label(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = data;
   e_widget_disabled_set(cfdata->gui.o_label_text, !e_widget_check_checked_get(
         cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_time_local_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_time_gmt_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_pos_tl_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_pos_tr_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_pos_bl_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_pos_br_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));
   e_widget_disabled_set(cfdata->gui.o_label_pos_other_toggle,
         !e_widget_check_checked_get(cfdata->gui.o_show_label));

   if (!e_widget_check_checked_get(cfdata->gui.o_show_label))
      e_widget_disabled_set(cfdata->gui.o_label_pos_other_text, 1);

   else _cb_label_pos(cfdata, NULL, NULL);
}

/*
 * Handle toggle of Set local time checkbox. Disable/enable
 * config options as appropriate.
 */
static void _cb_set_localtime(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = data;
   e_widget_disabled_set(cfdata->gui.o_localtime,
         !e_widget_check_checked_get(cfdata->gui.o_use_localtime));
}


/*
 * Validate the entered configuration. Display and error message
 * if required.
 */
static int parse_config_gui(E_Config_Dialog_Data *cfdata)
{
   E_Dialog *error_popup;
   char error_message[1024];
   int config_ok = 1;
   char *end_pointer;
   float parsed_float;
   const char *text_value;
   regex_t *reg_expression;
   int regex_result;
   char *viewpos_file;
   float lat, lon;

   sprintf(error_message, "The configuration you have entered is invalid:<br>");

   if (cfdata->local_xplanet.source_type == SOURCE_ORIGIN)
      cfdata->local_xplanet.origin = strdup(e_widget_ilist_selected_value_get(cfdata->gui.o_source_ilist));
   else
      cfdata->local_xplanet.projection = strdup(e_widget_ilist_selected_value_get(cfdata->gui.o_source_ilist));

   if (cfdata->local_xplanet.source_type == SOURCE_ORIGIN)
   {
      if (strcmp(cfdata->local_xplanet.body, "random") && !strcmp(cfdata->local_xplanet.body, cfdata->local_xplanet.origin))
      {
         sprintf(error_message + strlen(error_message),
            "<br>* Target and origin cannot be the same.");
         config_ok = 0;
      }
   }

   switch (cfdata->local_xplanet.viewpos_type)
   {
   case VIEWPOS_LATLON:
   {
      end_pointer = (char *)e_widget_entry_text_get(cfdata->gui.o_viewpos_lat);
      if (strlen(end_pointer) == 0)
      {
         sprintf(error_message + strlen(error_message),
               "<br>* You must enter a latitude.");
         config_ok = 0;
      }
      else
      {
         parsed_float = strtof(e_widget_entry_text_get(cfdata->gui.o_viewpos_lat),
               &end_pointer);
         if (*end_pointer != '\0' || parsed_float < -90.0 || parsed_float > 90.0)
         {
            sprintf(error_message + strlen(error_message),
                  "<br>* The entered latitude is invalid - must be in the range -90 to 90.");
            config_ok = 0;
         }
         else cfdata->local_xplanet.viewpos_lat = parsed_float;
      }

      end_pointer = (char *)e_widget_entry_text_get(cfdata->gui.o_viewpos_lon);
      if (strlen(end_pointer) == 0)
      {
         sprintf(error_message + strlen(error_message),
               "<br>* You must enter a longitude.");
         config_ok = 0;
      }
      else
      {
         parsed_float = strtof(e_widget_entry_text_get(cfdata->gui.o_viewpos_lon),
               &end_pointer);
         if (*end_pointer != '\0' || parsed_float < -180.0 || parsed_float > 360.0)
         {
            sprintf(error_message + strlen(error_message),
                  "<br>* The entered longitude is invalid - must be in the range 0 to 360 or -180 to 180");
            config_ok = 0;
         }
         else cfdata->local_xplanet.viewpos_lon = parsed_float;
      }
      break;
   }
   case VIEWPOS_FILE:
   {
      lat = INVALID_COORD;
      lon = INVALID_COORD;

      viewpos_file = (char *)e_widget_entry_text_get(cfdata->gui.o_viewpos_file_val);
      switch(read_viewpos_file(viewpos_file, &lat, &lon))
      {
      case VIEWPOS_FILE_NO_PERM:
      {
         sprintf(error_message + strlen(error_message),
               "<br>* No permissions to read viewing position file");
         config_ok = 0;
         break;
      }
      case VIEWPOS_FILE_NOT_FOUND:
      {
         sprintf(error_message + strlen(error_message),
               "<br>* The viewing position file cannot be found");
         config_ok = 0;
         break;
      }
      case VIEWPOS_FILE_IS_DIR:
      {
         sprintf(error_message + strlen(error_message),
               "<br>* The viewing position file is a directory");
         config_ok = 0;
         break;
      }
      case VIEWPOS_FILE_FORMAT_ERROR:
      {
         sprintf(error_message + strlen(error_message),
               "<br>* The viewing position file is in the wrong format (must be 2 numbers, lat then lon)");
         config_ok = 0;
         break;
      }
      case VIEWPOS_FILE_OK:
         cfdata->local_xplanet.viewpos_file = (char *)eina_stringshare_add(viewpos_file);
         break;
      }
      break;
   }
   default:
      break;
   }

   cfdata->local_xplanet.use_localtime = e_widget_check_checked_get(cfdata->gui.o_use_localtime);
   cfdata->local_xplanet.localtime = cfdata->gui.o_localtime;

   cfdata->local_xplanet.show_label = e_widget_check_checked_get(cfdata->gui.o_show_label);
   cfdata->local_xplanet.label_text = strdup(e_widget_entry_text_get(cfdata->gui.o_label_text));

   text_value = strdup(e_widget_entry_text_get(cfdata->gui.o_label_pos_other_text));
   if (!text_value || strlen(text_value) == 0)
   {
      cfdata->local_xplanet.label_pos_other = "";
      if (cfdata->local_xplanet.label_pos == LABEL_POS_OTHER)
      {
         sprintf(error_message + strlen(error_message),
               "<br>* You have not entered a label position");
         config_ok = 0;
      }
   }
   else
   {
      reg_expression = E_NEW(regex_t, 1);
      regcomp(reg_expression, "^[+-][0-9][0-9]*[+-][0-9][0-9]*$", 0);
      regex_result = regexec(reg_expression, text_value, 0, NULL, 0);
      if (regex_result)
      {
         sprintf(error_message + strlen(error_message),
               "<br>* The entered label position is invalid - must be of the form -15+15");
         config_ok = 0;
      }
      else cfdata->local_xplanet.label_pos_other = text_value;

      regfree(reg_expression);
      free(reg_expression);
   }

   cfdata->local_xplanet.use_config = e_widget_check_checked_get(cfdata->gui.o_config_check);
   cfdata->local_xplanet.config_name = strdup(e_widget_entry_text_get(cfdata->gui.o_config_name));

   cfdata->local_xplanet.extra_options = strdup(e_widget_entry_text_get(cfdata->gui.o_extra_options));

   if (!config_ok)
   {
      error_popup = e_dialog_new(e_container_current_get(
            e_manager_current_get()), "eplanet_error", "eplanet/error");
      e_dialog_title_set(error_popup, "Configuration error");
      e_dialog_text_set(error_popup, (const char *) &error_message);
      e_dialog_button_add(error_popup, D_("OK"), NULL, NULL, NULL);
      e_dialog_show(error_popup);
   }

   return config_ok;
}

/*
 * Select list items according to the loaded configuration
 */
static Eina_Bool _select_list_items(void *data) {
   E_Config_Dialog_Data *cfdata;

   cfdata = data;

   e_widget_ilist_selected_set(cfdata->gui.o_body_ilist, cfdata->body_item);
   e_widget_ilist_nth_show(cfdata->gui.o_body_ilist, cfdata->body_item, 1);

   e_widget_ilist_selected_set(cfdata->gui.o_source_ilist, cfdata->source_item);
   e_widget_ilist_nth_show(cfdata->gui.o_source_ilist, cfdata->source_item, 1);

   return ECORE_CALLBACK_PASS_ON;
}

/*
 * Display the file selection dialog for selecting the viewing position file.
 */
static void _select_viewpos_file(void *data, void *data2)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_Data *cfdata;
   E_Win *filesel_dialog = NULL;

   cfd = data;
   cfdata = data2;

   filesel_dialog = e_mod_config_filesel(cfd, e_widget_entry_text_get(cfdata->gui.o_viewpos_file_val));
}

/*
 * Set the viewing position file path.
 */
void set_viewpos_path(E_Config_Dialog *cfd, const char *path)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = cfd->cfdata;
   e_widget_entry_text_set(cfdata->gui.o_viewpos_file_val, path);
}
