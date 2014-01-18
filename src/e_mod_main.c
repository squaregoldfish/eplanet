#include <time.h>
#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "Ecore.h"

/* Local Function Prototypes */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name,
      const char *id, const char *style);
static void _gc_shutdown(E_Gadcon_Client *gcc);
static void _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static char *_gc_label(E_Gadcon_Client_Class *client_class);
static const char *_gc_id_new(E_Gadcon_Client_Class *client_class);
static Evas_Object *_gc_icon(E_Gadcon_Client_Class *client_class, Evas *evas);

static void _eplanet_conf_new(void);
static void _eplanet_conf_free(void);
static Eina_Bool _eplanet_conf_timer(void *data);
static void _eplanet_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj,
      void *event);
static void _eplanet_cb_menu_post(void *data, E_Menu *menu);
static void _eplanet_cb_menu_configure(void *data, E_Menu *mn, E_Menu_Item *mi);
static void _initialise_bg_update();
int _run_allowed();
void _get_free_mem(int *free);

/* Local Structures */
typedef struct _Instance Instance;
struct _Instance
{
   /* An instance of our item (module) with its elements */

   /* pointer to this gadget's container */
   E_Gadcon_Client *gcc;

   /* evas_object used to display */
   Evas_Object *o_eplanet;

   /* popup anyone ? */
   E_Menu *menu;
};

/* Local Variables */
static Eina_List *instances = NULL;
static E_Config_DD *_conf_edd = NULL;
static E_Config_DD *_xplanet_edd = NULL;
Config *eplanet_conf = NULL;
char *tmpdir = NULL;

static const E_Gadcon_Client_Class _gc_class =
{ GADCON_CLIENT_CLASS_VERSION, "eplanet",
{ _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      NULL }, E_GADCON_CLIENT_STYLE_PLAIN };

/* We set the version and the name, check e_mod_main.h for more details */
/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
     "Look - Eplanet"
};

/*
 * Module Functions
 */

/* Function called when the module is initialized */
EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[4096];

   /* Location of message catalogs for localization */
   snprintf(buf, sizeof(buf), "%s/locale", e_module_dir_get(m));
   bindtextdomain(PACKAGE, buf);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   snprintf(buf, sizeof(buf), "%s/e-module-eplanet.edj", e_module_dir_get(m));
   e_configure_registry_category_add("appearance", 10, D_("Look"), NULL, "preferences-look");
   e_configure_registry_item_add("appearance/eplanet", 120, D_("EPlanet"), NULL, buf, e_int_config_eplanet_module);

   _xplanet_edd = E_CONFIG_DD_NEW("Xplanet_Config", Xplanet_Config);
#undef T
#undef D
#define T Xplanet_Config
#define D _xplanet_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, body, STR);
   E_CONFIG_VAL(D, T, source_type, INT);
   E_CONFIG_VAL(D, T, origin, STR);
   E_CONFIG_VAL(D, T, projection, STR);
   E_CONFIG_VAL(D, T, viewpos_type, INT);
   E_CONFIG_VAL(D, T, viewpos_lat, DOUBLE);
   E_CONFIG_VAL(D, T, viewpos_lon, DOUBLE);
   E_CONFIG_VAL(D, T, use_localtime, INT);
   E_CONFIG_VAL(D, T, localtime, INT);
   E_CONFIG_VAL(D, T, viewpos_file, STR);
   E_CONFIG_VAL(D, T, show_label, INT);
   E_CONFIG_VAL(D, T, label_text, STR);
   E_CONFIG_VAL(D, T, label_time, INT);
   E_CONFIG_VAL(D, T, label_pos, INT);
   E_CONFIG_VAL(D, T, label_pos_other, STR);
   E_CONFIG_VAL(D, T, use_config, INT);
   E_CONFIG_VAL(D, T, config_name, STR);
   E_CONFIG_VAL(D, T, extra_options, STR);

   _conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D _conf_edd
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_VAL(D, T, delay, INT);
   E_CONFIG_VAL(D, T, taskPriority, INT);
   E_CONFIG_VAL(D, T, startDelay, INT);
   E_CONFIG_VAL(D, T, loadLimit, DOUBLE);
   E_CONFIG_VAL(D, T, minRamLimit, INT);
   E_CONFIG_VAL(D, T, disableOnBattery, INT);
   E_CONFIG_LIST(D, T, xplanet_configs, _xplanet_edd); /* the list */

   /* Tell E to find any existing module data. First run ? */
   eplanet_conf = e_config_domain_load("module.eplanet", _conf_edd);
   if (eplanet_conf)
   {
      /* Check config version */
      if ((eplanet_conf->version >> 16) < MOD_CONFIG_FILE_EPOCH)
      {
         /* config too old */
         _eplanet_conf_free();
         ecore_timer_add(1.0, _eplanet_conf_timer,
               D_("EPlanet Module Configuration data needed "
                     "upgrading. Your old configuration<br> has been"
                     " wiped and a new set of defaults initialized. "
                     "This<br>will happen regularly during "
                     "development, so don't report a<br>bug. "
                     "This simply means the module needs "
                     "new configuration<br>data by default for "
                     "usable functionality that your old<br>"
                     "configuration simply lacks. This new set of "
                     "defaults will fix<br>that by adding it in. "
                     "You can re-configure things now to your<br>"
                     "liking. Sorry for the inconvenience.<br>"));
      }

      /* Ardvarks */
      else if (eplanet_conf->version > MOD_CONFIG_FILE_VERSION)
      {
         /* config too new...wtf ? */
         _eplanet_conf_free();
         ecore_timer_add(1.0, _eplanet_conf_timer,
               D_("Your EPlanet Module configuration is NEWER "
                     "than the module version. This is "
                     "very<br>strange. This should not happen unless"
                     " you downgraded<br>the module or "
                     "copied the configuration from a place where"
                     "<br>a newer version of the module "
                     "was running. This is bad and<br>as a "
                     "precaution your configuration has been now "
                     "restored to<br>defaults. Sorry for the "
                     "inconvenience.<br>"));
      }

      /* If the list of xplanet configs is empty, then something is wrong */
      else if (eina_list_count(eplanet_conf->xplanet_configs) != 1)
      {
         printf("The list of XPlanet configs is empty! Resetting config...\n");
         _eplanet_conf_free();
         ecore_timer_add(1.0, _eplanet_conf_timer,
               D_("Your EPlanet Module configuration is DAMAGED."
                     "This is bad and<br>as a "
                     "precaution your configuration has been now "
                     "restored to<br>defaults. Sorry for the "
                     "inconvenience.<br>"));
      }
   }

   /* if we don't have a config yet, or it got erased above, 
    * then create a default one */
   if (!eplanet_conf) _eplanet_conf_new();

   /* create a link from the modules config to the module
    * this is not written */
   eplanet_conf->module = m;

   /* Tell any gadget containers (shelves, etc) that we provide a module
    * for the user to enjoy */
   e_gadcon_provider_register(&_gc_class);

   /* Start the delay timer for the first bg set */
   eplanet_conf->bg_set_timer = ecore_timer_add(eplanet_conf->startDelay,
         _start_background_update, NULL);

   // Register process exit event handlers
   eplanet_conf->xplanet_exe_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
         _handle_xplanet_exit, NULL);
   eplanet_conf->edjecc_exe_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
         _handle_edjecc_exit, NULL);

   /* Give E the module */
   return eplanet_conf;
}

/*
 * Function to unload the module
 */
EAPI int e_modapi_shutdown(E_Module *m)
{
   /* Unregister the config dialog from the main panel */
   e_configure_registry_item_del("appearance/eplanet");

   /* Remove the config panel category if we can. E will tell us.
    category stays if other items using it */
   e_configure_registry_category_del("appearance");

   /* Kill the config dialog */
   if (eplanet_conf->cfd) e_object_del(E_OBJECT(eplanet_conf->cfd));
   eplanet_conf->cfd = NULL;

   /* Tell E the module is now unloaded. Gets removed from shelves, etc. */
   eplanet_conf->module = NULL;
   e_gadcon_provider_unregister(&_gc_class);

   /* Release the exe_del event handlers */
   if (eplanet_conf->xplanet_exe_del) ecore_event_handler_del(eplanet_conf->xplanet_exe_del);
   if (eplanet_conf->edjecc_exe_del) ecore_event_handler_del(eplanet_conf->edjecc_exe_del);
   if (eplanet_conf->preview_exe_del) ecore_event_handler_del(eplanet_conf->preview_exe_del);
   /* Cleanup our item list */
   while (eplanet_conf->xplanet_configs)
   {
      Xplanet_Config *xi = NULL;

      /* Grab an item from the list */
      xi = eplanet_conf->xplanet_configs->data;

      /* remove it */
      eplanet_conf->xplanet_configs = eina_list_remove_list(
            eplanet_conf->xplanet_configs, eplanet_conf->xplanet_configs);

      /* cleanup stringshares */
      if (xi->id) eina_stringshare_del(xi->id);

      /* keep the planet green */
      E_FREE(xi);
   }

   /* Remove timer */
   if (eplanet_conf->bg_set_timer)
   {
      ecore_timer_del(eplanet_conf->bg_set_timer);
      eplanet_conf->bg_set_timer = NULL;
   }

   if (eplanet_conf->xplanet_filename)
      eina_stringshare_del(eplanet_conf->xplanet_filename);

   if (eplanet_conf->edj_filename)
      eina_stringshare_del(eplanet_conf->edj_filename);

   if (eplanet_conf->edc_filename)
      eina_stringshare_del(eplanet_conf->edc_filename);

   /* Cleanup the main config structure */
   E_FREE(eplanet_conf);

   /* Clean EET */
   E_CONFIG_DD_FREE(_xplanet_edd);
   E_CONFIG_DD_FREE(_conf_edd);

   return 1;
}

/*
 * Function to Save the module's config
 */
EAPI int e_modapi_save(E_Module *m)
{
   e_config_domain_save("module.eplanet", _conf_edd, eplanet_conf);
   return 1;
}

/* Local Functions */

/* Called when Gadget Controller (gadcon) says to appear in scene */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Instance *inst = NULL;
   char buf[4096];
   char *lastChar;

   /* theme file */
   snprintf(buf, sizeof(buf), "%s/e-module-eplanet.edj",
         eplanet_conf->module->dir);

   /* New visual instance, any config ? */
   inst = E_NEW(Instance, 1);

   /* create on-screen object */
   inst->o_eplanet = edje_object_add(gc->evas);
   /* we have a theme ? */
   if (!e_theme_edje_object_set(inst->o_eplanet, "base/theme/modules/eplanet",
         "modules/eplanet/main")) edje_object_file_set(inst->o_eplanet, buf,
         "modules/eplanet/main");

   /* Start loading our module on screen via container */
   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->o_eplanet);
   inst->gcc->data = inst;

   /* hook a mouse down. we want/have a popup menu, right ? */
   evas_object_event_callback_add(inst->o_eplanet, EVAS_CALLBACK_MOUSE_DOWN,
         _eplanet_cb_mouse_down, inst);

   /* add to list of running instances so we can cleanup later */
   instances = eina_list_append(instances, inst);

   /* Retrieve the temporary directory location */
   if (!tmpdir) {
	   // I suspect this method of getting the dir name will get me spanked.
	   tmpdir = strdup(getenv("E_IPC_SOCKET"));

	   // Find the last / character
	   lastChar = strrchr(tmpdir, '/');

	   // Replace it with null to terminate the string
	   *lastChar = '\0';
	}


   /* return the Gadget_Container Client */
   return inst->gcc;
}

/* Called when Gadget_Container says stop */
static void _gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = NULL;

   if (!(inst = gcc->data)) return;
   instances = eina_list_remove(instances, inst);

   /* kill popup menu */
   if (inst->menu)
   {
      e_menu_post_deactivate_callback_set(inst->menu, NULL, NULL);
      e_object_del(E_OBJECT(inst->menu));
      inst->menu = NULL;
   }
   /* delete the visual */
   if (inst->o_eplanet)
   {
      /* remove mouse down callback hook */
      evas_object_event_callback_del(inst->o_eplanet, EVAS_CALLBACK_MOUSE_DOWN,
            _eplanet_cb_mouse_down);
      evas_object_del(inst->o_eplanet);
   }
   E_FREE(inst);

   free(tmpdir);
}

/* For when container says we are changing position */
static void _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

/* Gadget/Module label, name for our module */
static char *
_gc_label(E_Gadcon_Client_Class *client_class)
{
   return D_("EPlanet");
}

static Evas_Object *
_gc_icon(E_Gadcon_Client_Class *client_class, Evas *evas)
{
   Evas_Object *o = NULL;
   char buf[4096];

   /* theme */
   snprintf(buf, sizeof(buf), "%s/e-module-eplanet.edj",
         eplanet_conf->module->dir);

   /* create icon object */
   o = edje_object_add(evas);

   /* load icon from theme */
   edje_object_file_set(o, buf, "icon");

   return o;
}

static const char *
_gc_id_new(E_Gadcon_Client_Class *client_class)
{
   Xplanet_Config *xi;

   xi = _xplanet_config_get(NULL);
   return xi->id;
}

/*
 * Retrieve a specific XPlanet Config object.
 * Currently there's only ever one xplanet config, but
 * more will be supported in future.
 */
Xplanet_Config *_xplanet_config_get(const char *id)
{
   Eina_List *l;
   Xplanet_Config *xi;
   char buf[PATH_MAX];

   if (!id)
   {
      int num = 0;

      /* Create id */
      if (eplanet_conf->xplanet_configs)
      {
         const char *p;

         xi = eina_list_last(eplanet_conf->xplanet_configs)->data;
         p = strrchr(xi->id, '.');
         if (p) num = atoi(p + 1) + 1;
      }
      snprintf(buf, sizeof(buf), "%s.%d", _gc_class.name, num);
      id = buf;

      xi = E_NEW(Xplanet_Config, 1);
      xi->id = eina_stringshare_add(id);
      xi->body = eina_stringshare_add("earth");
      xi->origin = eina_stringshare_add("sun");
      xi->projection = eina_stringshare_add("rectangular");
      xi->source_type = SOURCE_ORIGIN;
      xi->viewpos_type = VIEWPOS_LATLON;
      xi->viewpos_lat = 0.0;
      xi->viewpos_lon = 0.0;
      xi->use_localtime = 0;
      xi->viewpos_file = "";
      xi->localtime = 12;
      xi->show_label = 1;
      xi->label_text = "";
      xi->label_time = LABEL_TIME_LOCAL;
      xi->label_pos = LABEL_POS_TR;
      xi->label_pos_other = "";
      xi->use_config = 0;
      xi->config_name = "config";
      xi->extra_options = "";
      xi->zone_ids = NULL;
   }
   else
   {
EINA_LIST_FOREACH   (eplanet_conf->xplanet_configs, l, xi)
   {
      if (!xi->id) continue;
      if (!strcmp(xi->id, id)) return xi;
   }
}

return xi;
}

/* new module needs a new config :), or config too old and we need one anyway */
static void _eplanet_conf_new(void)
{
   Xplanet_Config *xi;

   eplanet_conf = E_NEW(Config, 1);
   eplanet_conf->version = (MOD_CONFIG_FILE_EPOCH << 16);

#define IFMODCFG(v) if ((eplanet_conf->version & 0xffff) < v) {
#define IFMODCFGEND }

   /* setup defaults */
   IFMODCFG(0x008d);
      eplanet_conf->delay = 5;
      eplanet_conf->startDelay = 5;
      eplanet_conf->taskPriority = 0;
      eplanet_conf->loadLimit = 3.0;
      eplanet_conf->minRamLimit = 64;
      eplanet_conf->disableOnBattery = 1;
      eplanet_conf->xplanet_configs = NULL;
      eplanet_conf->force_update = 0;

      xi = _xplanet_config_get(NULL);

      eplanet_conf->xplanet_configs = eina_list_append(
            eplanet_conf->xplanet_configs, xi);

   IFMODCFGEND;

   /* update the version */
   eplanet_conf->version = MOD_CONFIG_FILE_VERSION;

   /* setup limits on the config properties here (if needed) */

   /* save the config to disk */
   e_config_save_queue();
}

/* This is called when we need to cleanup the actual configuration,
 * for example when our configuration is too old */
static void _eplanet_conf_free(void)
{
   Xplanet_Config *xi;
   EINA_LIST_FREE(eplanet_conf->xplanet_configs, xi);

   E_FREE(eplanet_conf);
}

/* timer for the config oops dialog (old configuration needs update) */
static Eina_Bool _eplanet_conf_timer(void *data)
{
   e_util_dialog_show(D_("EPlanet Configuration Updated"), "%s", (char *) data);
   return ECORE_CALLBACK_DONE;
}

/* Pants On */
static void _eplanet_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj,
      void *event)
{
   Instance *inst = NULL;
   Evas_Event_Mouse_Down *ev;
   E_Zone *zone = NULL;
   E_Menu *gadget_menu;
   E_Menu_Item *mi = NULL;
   int x, y;

   if (!(inst = data)) return;
   ev = event;
   if ((ev->button == 3) && (!inst->menu))
   {
      /* grab current zone */
      zone = e_util_zone_current_get(e_manager_current_get());

      /* create popup menu */
      gadget_menu = e_menu_new();
      e_menu_post_deactivate_callback_set(gadget_menu, _eplanet_cb_menu_post,
            inst);

      mi = e_menu_item_new(gadget_menu);
      e_menu_item_label_set(mi, D_("Update background now"));
      e_menu_item_callback_set(mi, _force_start_bg_update, NULL);

      mi = e_menu_item_new(gadget_menu);
      e_menu_item_label_set(mi, D_("Configuration"));
      e_util_menu_item_theme_icon_set(mi, "configure");
      e_menu_item_callback_set(mi, _eplanet_cb_menu_configure, NULL);

      /* Each Gadget Client has a utility menu from the Container */
      gadget_menu = e_gadcon_client_util_menu_items_append(inst->gcc, gadget_menu, 0);
      inst->menu = gadget_menu;

      e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);

      /* show the menu relative to gadgets position */
      e_menu_activate_mouse(inst->menu, zone, (x + ev->output.x), (y
            + ev->output.y), 1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
      evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
            EVAS_BUTTON_NONE, ev->timestamp, NULL);
   }
}

/* popup menu closing, cleanup */
static void _eplanet_cb_menu_post(void *data, E_Menu *menu)
{
   Instance *inst = NULL;

   if (!(inst = data)) return;
   if (!inst->menu) return;
   e_object_del(E_OBJECT(inst->menu));
   inst->menu = NULL;
}

/* call configure from popup */
static void _eplanet_cb_menu_configure(void *data, E_Menu *mn, E_Menu_Item *mi)
{
   if (!eplanet_conf) return;
   if (eplanet_conf->cfd) return;
   e_int_config_eplanet_module(mn->zone->container, NULL);
}

/* Run XPlanet, and set up a handler for when it finishes */
EAPI void _run_xplanet(char *exec, char *filename, int mode)
{
   Ecore_Exe *processHandle;

   processHandle = ecore_exe_run(exec, NULL);

   // Remove any existing registered pids - the new xplanet run will
   // override them.
   eplanet_conf->edjecc_pid = -1;
   if (mode == PREVIEW_MODE)
   {
      eplanet_conf->xplanet_pid = -1;
      eplanet_conf->preview_pid = ecore_exe_pid_get(processHandle);
   } else {
      eplanet_conf->preview_pid = -1;
      eplanet_conf->xplanet_pid = ecore_exe_pid_get(processHandle);
   }
   eplanet_conf->xplanet_filename = eina_stringshare_add(filename);
}

/*
 * Handler for when XPlanet quits
 */
EAPI Eina_Bool _handle_xplanet_exit(void *data, int type, void *event)
{
   Ecore_Exe_Event_Del *ev;
   pid_t end_pid;
   const char *filename;
   char edcFile[PATH_MAX], enc[128], cmd[PATH_MAX], ipart[PATH_MAX],
         edjFile[PATH_MAX];
   int fd;
   Evas *evas;
   E_Win *e_win;
   Evas_Object *img;
   int w = 0, h = 0;
   FILE *f;
   Ecore_Exe *processHandle;

   ev = event;
   end_pid = ecore_exe_pid_get(ev->exe);

   // Make sure the ending process is the instance XPlanet is the one we were looking for
   if (eplanet_conf->xplanet_pid > 0 && end_pid == eplanet_conf->xplanet_pid)
   {
      // Reset process pid
      eplanet_conf->xplanet_pid = -1;

      // If the config dialog is open, do nothing
      if (!e_config_dialog_find("EPlanet", CONFIG_WIN_CLASS))
      {
         // Set up filename details
         filename = ecore_file_file_get(eplanet_conf->xplanet_filename);
         snprintf(ipart, sizeof(ipart), "-id %s", ecore_file_dir_get(
               eplanet_conf->xplanet_filename));

         // We have to use a new filename every time because e is too clever:
         // give it the same filename and it does nothing.
         e_user_dir_snprintf(edjFile, sizeof(edjFile),
               "backgrounds/eplanet-%d_%d.edj", eplanet_conf->current_zone,
               (int) time(NULL));

         // Create the edc file from which the background edj will be built
         eplanet_conf->edj_filename = (char *)eina_stringshare_add(edjFile);

         sprintf(edcFile, "%s/eplanet-edc-tmp-XXXXXX", tmpdir);

         fd = mkstemp(edcFile);
         eplanet_conf->edc_filename = (char *)eina_stringshare_add(strdup(edcFile));
         if (fd < 0)
         {
            printf("EPLANET: Error Creating edc file: %s\n", strerror(errno));
            return 1;
         }
         close(fd);

         f = fopen(edcFile, "w");
         if (!f)
         {
            printf("EPLANET: Cannot open %s for writing\n", edcFile);
            return 1;
         }

         // Work out image size
         e_win = e_win_new(e_container_current_get(e_manager_current_get()));
         evas = e_win_evas_get(e_win);
         img = evas_object_image_add(evas);
         evas_object_image_file_set(img, filename, NULL);
         evas_object_image_size_get(img, &w, &h);
         evas_object_del(img);

         // Write the edc data
         snprintf(enc, sizeof(enc), "COMP");

         fprintf(f, "images { image: \"%s\" %s; }\n"
            "collections {\n"
            "group { name: \"e/desktop/background\";\n"
            "data { item: \"style\" \"0\"; }\n"
            "max: %i %i;\n"
            "parts {\n"
            "part { name: \"bg\"; mouse_events: 0;\n"
            "description { state: \"default\" 0.0;\n"
            "image { normal: \"%s\"; scale_hint: STATIC; }\n"
            "} } } } }\n", eplanet_conf->xplanet_filename, enc, w, h,
               eplanet_conf->xplanet_filename);

         fclose(f);

         // Prepare the edje compiler command
         snprintf(cmd, sizeof(cmd), "nice -%d edje_cc -v %s %s %s", eplanet_conf->taskPriority, ipart, edcFile,
               e_util_filename_escape(edjFile));

         E_FREE(e_win);

         // Now run edje_cc and set up a handler for when it exits
         processHandle = ecore_exe_run(cmd, NULL);
         eplanet_conf->edjecc_pid = ecore_exe_pid_get(processHandle);
      }

      return ECORE_CALLBACK_DONE;
   }

   return ECORE_CALLBACK_PASS_ON;
}

/*
 * Handle edje_cc exit - set the background to the created edj file.
 */
EAPI Eina_Bool _handle_edjecc_exit(void *data, int type, void *event)
{
   Ecore_Exe_Event_Del *ev;
   pid_t end_pid;
   const char *filename;
   E_Container *container;
   E_Zone *zone;
   int i;
   char backgrounds_dir[PATH_MAX], bg_file_root[PATH_MAX], del_file[PATH_MAX];
   Eina_List *bg_file_list, *list_entry;
   char *bg_file;

   ev = event;
   end_pid = ecore_exe_pid_get(ev->exe);

   // Make sure we've captured the right exiting process
   if (eplanet_conf->edjecc_pid > 0 && end_pid == eplanet_conf->edjecc_pid)
   {
      // Reset process pid
      eplanet_conf->edjecc_pid = -1;

      // If the config dialog is open, do nothing.
      if (!e_config_dialog_find("EPlanet", CONFIG_WIN_CLASS))
      {
         filename = ecore_file_file_get(eplanet_conf->xplanet_filename);

         // Delete all desktop backgrounds from the target zone
         container = e_container_current_get(e_manager_current_get());
         zone = eina_list_nth(container->zones, eplanet_conf->current_zone);

         for (i = 0; i < zone->desk_x_count * zone->desk_y_count; i++)
         {
            e_bg_del(container->num, zone->num, zone->desks[i]->x,
                  zone->desks[i]->y);
            e_bg_add(container->num, zone->num, zone->desks[i]->x,
                  zone->desks[i]->y, eplanet_conf->edj_filename);
         }

         // Clean up files
         if (eplanet_conf->xplanet_filename)
         {
            ecore_file_unlink(eplanet_conf->xplanet_filename);
            eina_stringshare_del(eplanet_conf->xplanet_filename);
            eplanet_conf->xplanet_filename = NULL;
         }

         if (eplanet_conf->edc_filename)
         {
            ecore_file_unlink(eplanet_conf->edc_filename);
            eina_stringshare_del(eplanet_conf->edc_filename);
            eplanet_conf->edc_filename = NULL;
         }

         // Delete any old eplanet backgrounds
         e_user_dir_snprintf(backgrounds_dir, sizeof(backgrounds_dir),
               "backgrounds");
         sprintf(bg_file_root, "eplanet-%d", eplanet_conf->current_zone);
         bg_file_list = ecore_file_ls(backgrounds_dir);
         EINA_LIST_FOREACH(bg_file_list, list_entry, bg_file)
         {
            // The bg_file_root allows us to identify background files that
            // are (a) from eplanet and (b) from the zone being updated. All other
            // backgrounds are left alone - we don't want to clear out the user's
            // other background files.
            if (strcmp(bg_file, ecore_file_file_get(eplanet_conf->edj_filename)) &&
                  !strncmp(bg_file, bg_file_root, strlen(bg_file_root)))
            {
               sprintf(del_file, "%s/%s", backgrounds_dir, bg_file);
               ecore_file_unlink(del_file);
            }
         }

         // Clear the edj_filename
         eina_stringshare_del(eplanet_conf->edj_filename);
         eplanet_conf->edj_filename = NULL;

         // Signal the removal of old backgrounds to e
         e_bg_update();
         e_config_save_queue();

         // Set up the new background for the correct zone
         eplanet_conf->current_zone++;
         if (eplanet_conf->current_zone < eina_list_count(
               eplanet_conf->bg_zone_ids)) _initialise_bg_update();
         else
         {
            eplanet_conf->current_zone = 0;
            eplanet_conf->current_xplanet_config++;
            if (eplanet_conf->current_xplanet_config < eina_list_count(
                  eplanet_conf->xplanet_configs)) _initialise_bg_update();
            else
            {
               // Start the timer for the next update
               eplanet_conf->current_xplanet_config = 0;
               eplanet_conf->bg_set_timer = ecore_timer_add(eplanet_conf->delay
                     * 60, _start_background_update, NULL);
            }
         }
      }

      return ECORE_CALLBACK_DONE;
   }

   return ECORE_CALLBACK_PASS_ON;
}

/*
 * Construct an XPlanet command line from config
 */
EAPI void _build_xplanet_command(Xplanet_Config *xplanet_config, int width,
      int height, char *cmd, char *filename, Eina_List *ignored_options)
{
   float viewpos_lat, viewpos_lon;
   const char *label_pos;
   int file_read_result;

   sprintf(cmd, "nice -%d xplanet -body %s", eplanet_conf->taskPriority, xplanet_config->body);

   if (xplanet_config->source_type == SOURCE_ORIGIN) sprintf(cmd + strlen(cmd),
         " -origin %s", xplanet_config->origin);
   else
   {
      sprintf(cmd + strlen(cmd), " -projection %s", xplanet_config->projection);

      switch (xplanet_config->viewpos_type)
      {
      case VIEWPOS_DEFAULT:
         viewpos_lat = 0.0;
         viewpos_lon = 0.0;
         break;
      case VIEWPOS_LATLON:
         viewpos_lat = xplanet_config->viewpos_lat;
         viewpos_lon = xplanet_config->viewpos_lon;
         break;
      case VIEWPOS_RANDOM:
         viewpos_lat = (180 * ((float) rand() / RAND_MAX)) - 90;
         viewpos_lon = (360 * ((float) rand() / RAND_MAX));
         break;
      case VIEWPOS_FILE:
         file_read_result = read_viewpos_file(xplanet_config->viewpos_file, &viewpos_lat, &viewpos_lon);
         if (file_read_result != VIEWPOS_FILE_OK)
         {
            printf("EPLANET ERROR: Read of viewpos file failed (%d)\n", file_read_result);
            viewpos_lat = 0.0;
            viewpos_lon = 0.0;
         }
         break;
      }

      sprintf(cmd + strlen(cmd), " -latitude %1g -longitude %1g",
            viewpos_lat, viewpos_lon);

      if (xplanet_config->use_localtime) {
         sprintf(cmd + strlen(cmd), " -localtime %d", xplanet_config->localtime);
      }
   }

   if (xplanet_config->show_label)
   {
      if (xplanet_config->label_time == LABEL_TIME_LOCAL) sprintf(cmd + strlen(
            cmd), " -label");
      else sprintf(cmd + strlen(cmd), " -gmtlabel");

      if (xplanet_config->label_text && strlen(xplanet_config->label_text) > 0) sprintf(
            cmd + strlen(cmd), " -label_string \"%s\"",
            xplanet_config->label_text);

      switch (xplanet_config->label_pos)
      {
      case LABEL_POS_TL:
         label_pos = "+15+15";
         break;
      case LABEL_POS_TR:
         label_pos = "-15+15";
         break;
      case LABEL_POS_BL:
         label_pos = "+15-15";
         break;
      case LABEL_POS_BR:
         label_pos = "-15-15";
         break;
      case LABEL_POS_OTHER:
         label_pos = xplanet_config->label_pos_other;
         break;
      }

      sprintf(cmd + strlen(cmd), " -labelpos %s", label_pos);
   }

   if (xplanet_config->use_config)
   {
      sprintf(cmd + strlen(cmd), " -config %s", xplanet_config->config_name);
   }

   if (xplanet_config->extra_options && strlen(xplanet_config->extra_options)
         > 0) sprintf(cmd + strlen(cmd), " %s", xplanet_config->extra_options);

   sprintf(cmd + strlen(cmd), " -num_times 1 -geometry %dx%d -output \"%s\"",
         width, height, filename);

   printf("EPLANET: Xplanet command:\n  %s\n", cmd);
}

/*
 * Force the background to be updated regardless of the current
 * timer state.
 */
EAPI void _force_start_bg_update(void *data, E_Menu *menu, E_Menu_Item *menu_item)
{
   eplanet_conf->force_update = 1;
   _start_background_update();
}

/*
 * Start a background update.
 */
EAPI Eina_Bool _start_background_update()
{
   Xplanet_Config *xplanet_config;
   E_Container *container;
   int count;

   // Check the system to see if we can run
   if (eplanet_conf->force_update || _run_allowed())
   {
      // Reset the force update flag
      eplanet_conf->force_update = 0;

      container = e_container_current_get(e_manager_current_get());

      // First up, delete the timer so we don't get multiple runs
      if (eplanet_conf->bg_set_timer)
      {
         ecore_timer_del(eplanet_conf->bg_set_timer);
         eplanet_conf->bg_set_timer = NULL;
      }

      // Select the first xplanet config, and use it to set the bg
      eplanet_conf->current_xplanet_config = 0;
      if (eplanet_conf->xplanet_configs)
      {
         xplanet_config = eina_list_nth(eplanet_conf->xplanet_configs,
               eplanet_conf->current_xplanet_config);
         if (!xplanet_config->zone_ids)
         {
            eplanet_conf->bg_zone_ids = NULL;

            for (count = 0; count < eina_list_count(container->zones); count++)
               eplanet_conf->bg_zone_ids = eina_list_append(
                     eplanet_conf->bg_zone_ids, &count);
         }
         else
         {
            eplanet_conf->bg_zone_ids = xplanet_config->zone_ids;
         }

         eplanet_conf->current_zone = 0;
         _initialise_bg_update();
      }

   }

   eplanet_conf->bg_set_timer = ecore_timer_add(eplanet_conf->delay * 60, _start_background_update, NULL);

   return ECORE_CALLBACK_DONE;
}

/*
 * Start the background update
 */
static void _initialise_bg_update()
{
   Xplanet_Config *xplanet_config;
   E_Container *container;
   E_Zone *zone;
   char cmd[PATH_MAX], filename[PATH_MAX];

   // Retrieve the XPlanet configuration
   xplanet_config = eina_list_nth(eplanet_conf->xplanet_configs,
         eplanet_conf->current_xplanet_config);
   container = e_container_current_get(e_manager_current_get());
   zone = eina_list_nth(container->zones, eplanet_conf->current_zone);

   // Build the XPlanet command and run it
   sprintf(filename, "%s/eplanet.png", tmpdir);
   _build_xplanet_command(xplanet_config, zone->w, zone->h, cmd, filename, NULL);
   _run_xplanet(cmd, filename, BG_MODE);
}

/*
 * See if the background can be updated, based on current system state
 * and configuration.
 */
int _run_allowed()
{
   int result = 1;
   double load_average;
   int freeMem;

   // See if we're on battery
//   if (eplanet_conf->disableOnBattery) {

//   }

   // Test load average
   getloadavg(&load_average, 1);
   if (load_average >= eplanet_conf->loadLimit)
   {
      printf("Not running - load average is above threshold\n");
      result = 0;
   }

   // Test free memory
   _get_free_mem(&freeMem);
   if (freeMem < eplanet_conf->minRamLimit)
   {
      printf("Not running - not enough free RAM\n");
      result = 0;
   }

   if (e_config_dialog_find("EPlanet", CONFIG_WIN_CLASS))
      result = 0;


   return result;
}

/*
 * Retrieve the available free RAM
 */
void _get_free_mem(int *freeMem)
{
   FILE *pmeminfo = NULL;
   int cursor = 0;
   char *line, *field;
   unsigned char c;
   long int value = 0, mfree = 0;

   if (!(pmeminfo = fopen("/proc/meminfo", "r")))
   {
      fprintf(stderr, "can't open /proc/meminfo");
      return;
   }

   line = (char *) calloc(64, sizeof(char));
   while (fscanf(pmeminfo, "%c", &c) != EOF)
   {
      if (c != '\n') line[cursor++] = c;
      else
      {
         field = (char *) malloc(strlen(line) * sizeof(char));
         sscanf(line, "%s %ld kB", field, &value);
         if (!strcmp(field, "MemFree:"))
            mfree = value;
         else if (!strcmp(field, "Cached:"))
            mfree += value;

         free(line);
         free(field);
         cursor = 0;
         line = (char *) calloc(64, sizeof(char));
      }
   }
   fclose(pmeminfo);

   *freeMem = mfree / 1024;
}

/*
 * Read the contents of the Viewing Position from a file.
 * Simply reads two float values from the start of the file -
 * any other data is ignored.
 *
 * If the file doesn't start with 2 floats, an error code is
 * returned.
 */
int read_viewpos_file(char *path, float *lat, float *lon)
{
   int result = VIEWPOS_FILE_OK;
   FILE *viewpos_file;
   char buffer[128];
   char *start_pointer, *end_pointer;
   float temp_lat, temp_lon;

   if (!ecore_file_can_read(path))
      result = VIEWPOS_FILE_NO_PERM;
   if (!ecore_file_exists(path))
      result = VIEWPOS_FILE_NOT_FOUND;
   else if (ecore_file_is_dir(path))
      result = VIEWPOS_FILE_IS_DIR;
   else if (ecore_file_size(path) == 0)
      result = VIEWPOS_FILE_EMPTY;
   else
   {
      temp_lat = INVALID_COORD;
      temp_lon = INVALID_COORD;
      viewpos_file = fopen(path, "r");
      start_pointer = fgets(buffer, 128, viewpos_file);

      if (strlen(start_pointer) == 0)
         result = VIEWPOS_FILE_FORMAT_ERROR;
      else
      {
         fclose(viewpos_file);
         temp_lat = strtof(buffer, &end_pointer);
         if (end_pointer == start_pointer || temp_lat < -90.0 || temp_lat > 90.0)
            result = VIEWPOS_FILE_FORMAT_ERROR;
         else
         {
            start_pointer = end_pointer + 1;
            temp_lon = strtof(start_pointer, &end_pointer);
            if (temp_lon < -180.0 || temp_lon > 360.0)
               result = VIEWPOS_FILE_FORMAT_ERROR;
         }
      }
   }

   if (result == VIEWPOS_FILE_OK)
   {
      *lat = temp_lat;
      *lon = temp_lon;
   }
   return result;
}
