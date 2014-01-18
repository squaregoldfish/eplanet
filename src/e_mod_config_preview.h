#ifndef E_MOD_CONFIG_PREVIEW_H
#define E_MOD_CONFIG_PREVIEW_H

#define PREV_WIN_CLASS "advanced/eplanet-preview"

E_Win *e_mod_config_preview(E_Config_Dialog *parent);
E_Border *_find_existing_preview();

void do_generate_preview(E_Win *win, Xplanet_Config *xplanet_config);
Eina_Bool _draw_preview(void *data, int type, void *event);

#endif

