#include "../include/common.h"
#include "EfiMonitor.h"
#include "EfiInput.h"

STATIC mp_obj_t mp_init_efidirect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum { ARG_w, ARG_h, ARG_auto_refresh };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_w, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = LV_HOR_RES_MAX} },
        { MP_QSTR_h, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = LV_VER_RES_MAX} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    monitor_init(args[ARG_w].u_int, args[ARG_h].u_int);

    input_init();

    return mp_const_none;
}

STATIC mp_obj_t mp_deinit_efidirect(void)
{
    monitor_deinit();
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mp_init_efidirect_obj, 0, mp_init_efidirect);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_deinit_efidirect_obj, mp_deinit_efidirect);

DEFINE_PTR_OBJ(monitor_flush);
DEFINE_PTR_OBJ(mouse_read);
DEFINE_PTR_OBJ(keyboard_read);

STATIC const mp_rom_map_elem_t efidirect_globals_table[] = {
        { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_efidirect) },
        { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&mp_init_efidirect_obj) },
        { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&mp_deinit_efidirect_obj) },
        { MP_ROM_QSTR(MP_QSTR_monitor_flush), MP_ROM_PTR(&PTR_OBJ(monitor_flush))},
        { MP_ROM_QSTR(MP_QSTR_mouse_read), MP_ROM_PTR(&PTR_OBJ(mouse_read))},
        { MP_ROM_QSTR(MP_QSTR_keyboard_read), MP_ROM_PTR(&PTR_OBJ(keyboard_read))},
};
         

STATIC MP_DEFINE_CONST_DICT (
    mp_module_efidirect_globals,
    efidirect_globals_table
);

const mp_obj_module_t mp_module_efidirect = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_efidirect_globals
};

