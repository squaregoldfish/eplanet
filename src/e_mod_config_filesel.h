#ifndef E_MOD_CONFIG_FILESEL_H
#define E_MOD_CONFIG_FILESEL_H

#define FILESEL_WIN_CLASS "advanced/eplanet-filesel"

/*
 * Show the file selection dialog for the viewpos file location.
 */

E_Win *e_mod_config_filesel(E_Config_Dialog *parent, const char *path);

/*
 * See if a file selection dialog has already been registered.
 */
E_Border *_find_existing_filesel();

#endif

