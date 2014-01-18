#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

/* Macros used for config file versioning */
/* You can increment the EPOCH value if the old configuration is not
 * compatible anymore, it creates an entire new one.
 * You need to increment GENERATION when you add new values to the
 * configuration file but is not needed to delete the existing conf  */
#define MOD_CONFIG_FILE_EPOCH 0x0002
#define MOD_CONFIG_FILE_GENERATION 0x0001
#define MOD_CONFIG_FILE_VERSION \
   ((MOD_CONFIG_FILE_EPOCH << 16) | MOD_CONFIG_FILE_GENERATION)

/* Gettext: you need to use the D_ prefix for all your messages,
 * like  printf D_("Hello World\n");  so can be replaced by gettext */
#define D_(str) dgettext(PACKAGE, str)

/* Constant config values */
#define SOURCE_ORIGIN 0
#define SOURCE_PROJECTION 1

#define VIEWPOS_DEFAULT 0
#define VIEWPOS_LATLON 1
#define VIEWPOS_RANDOM 2
#define VIEWPOS_FILE 3

#define VIEWPOS_FILE_OK 0
#define VIEWPOS_FILE_NOT_FOUND 1
#define VIEWPOS_FILE_IS_DIR 2
#define VIEWPOS_FILE_FORMAT_ERROR 3
#define VIEWPOS_FILE_NO_PERM 4
#define VIEWPOS_FILE_EMPTY 5

#define INVALID_COORD -999

#define LABEL_TIME_LOCAL 0
#define LABEL_TIME_GMT 1

#define LABEL_POS_TL 0
#define LABEL_POS_TR 1
#define LABEL_POS_BL 2
#define LABEL_POS_BR 3
#define LABEL_POS_OTHER 4

#define BG_MODE 0
#define PREVIEW_MODE 1

#define STATUS_DEFAULT 0
#define STATUS_UPDATING 1
#define STATUS_SUSPENDED 2
#define STATUS_DISABLED 3
#define STATUS_ERROR 4

/* We create a structure config for our module, and also a config structure
 * for every item element (you can have multiple gadgets for the same module) */
typedef struct _Config Config;
typedef struct _Xplanet_Config Xplanet_Config;

struct _Xplanet_Config
{
   const char *id;
   const char *body;

   int source_type;
   const char *origin;
   const char *projection;

   int viewpos_type;
   float viewpos_lat, viewpos_lon;

   int use_localtime;
   int localtime;

   char *viewpos_file;

   int show_label;
   const char *label_text;
   int label_time;
   int label_pos;
   const char *label_pos_other;
   int use_config;
   const char *config_name;
   const char *extra_options;

   Eina_List *zone_ids;
};

/* Base config struct. Store Item Count, etc
 * 
 * *module (not written to disk) (E Interaction)
 * *cfd (not written to disk) (config dialog)
 * 
 * Store list of your items that you want to keep. (sorting)
 * Can define per-module config properties here.
 * 
 * Version used to know when user config too old */
struct _Config
{
   /* Store a reference to the module instance provided by enlightenment in
    * e_modapi_init, in case you need to access it. (not written to disk) */
   E_Module *module;

   /* if you open a config dialog, store a reference to it in a pointer like
    * this one, so if the user opens a second time the dialog, you know it's
    * already open. Also needed for destroy the dialog when we are exiting */
   E_Config_Dialog *cfd;

   /* config file version */
   int version;

   /* actual config properties; Define your own. (globally per-module) */
   int delay;
   int startDelay;
   int taskPriority;
   double loadLimit;
   int minRamLimit;
   int disableOnBattery;

   /* Store multiple XPlanet configs for multiple screens/desks.
    There'll only be one for now, but it's good to future-proof */
   Eina_List *xplanet_configs;

   /* Exe_Del handlers */
   Ecore_Event_Handler *xplanet_exe_del, *edjecc_exe_del, *preview_exe_del;
   pid_t xplanet_pid, edjecc_pid, preview_pid;
   const char *xplanet_filename;
   char *edc_filename;
   char *edj_filename;

   /* BG Set variables */
   Ecore_Timer *bg_set_timer;
   Eina_List *bg_zone_ids;
   int current_xplanet_config;
   int current_zone;
   int force_update;
};

/* Setup the E Module Version, Needed to check if module can run. */
/* The version is stored at compilation time in the module, and is checked
 * by E in order to know if the module is compatible with the actual version */
EAPI extern E_Module_Api e_modapi;

/* E API Module Interface Declarations
 *
 * e_modapi_init:     it is called when e17 initialize the module, note that
 *                    a module can be loaded but not initialized (running)
 *                    Note that this is not the same as _gc_init, that is called
 *                    when the module appears on his container
 * e_modapi_shutdown: it is called when e17 is closing, so calling the modules
 *                    to finish
 * e_modapi_save:     this is called when e17 or by another reason is requeested
 *                    to save the configuration file                      */
EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);

/* Function for calling the module's Configuration Dialog */
E_Config_Dialog *e_int_config_eplanet_module(E_Container *con,
      const char *params);

/* Run XPlanet and set the resulting image as the background */
EAPI Eina_Bool _start_background_update();
EAPI void _force_start_bg_update(void *data, E_Menu *menu, E_Menu_Item *menu_item);
EAPI void _run_xplanet(char *exec, char *filename, int mode);

/* Generate an XPlanet command */
EAPI void _build_xplanet_command(Xplanet_Config *xplanet_config, int width,
      int height, char *cmd, char *filename, Eina_List *ignore_options);

/* Handle exiting processes */
EAPI Eina_Bool _handle_xplanet_exit(void *data, int type, void *event);
EAPI Eina_Bool _handle_edjecc_exit(void *data, int type, void *event);

Xplanet_Config *_xplanet_config_get(const char *id);

int read_viewpos_file(char *path, float *lat, float *lon);


extern Config *eplanet_conf;

#endif

