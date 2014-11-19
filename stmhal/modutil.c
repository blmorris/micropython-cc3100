
#include <stdio.h>

#include "mpconfig.h"
#include "nlr.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "log.h"

//
// Global functions
//

STATIC mp_obj_t glb_print(mp_obj_t msg_int)
{
    if(MP_OBJ_IS_INT(msg_int))
    {
        printf("[%d]\n", mp_obj_get_int(msg_int));
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "float not supported"));
    }
    return(mp_const_none);
}
MP_DEFINE_CONST_FUN_OBJ_1(glb_print_obj, glb_print);

STATIC mp_obj_t glb_nop(void)
{
    return(mp_const_none);
}
MP_DEFINE_CONST_FUN_OBJ_0(glb_nop_obj, glb_nop);

//
// Local
//

typedef struct _mp_obj_util_t
{   // linked list
    mp_obj_base_t base;
    int num;
} mp_obj_util_t;

STATIC const mp_obj_type_t util_type;

STATIC void util_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind)
{
    mp_obj_util_t *self = self_in;
    print(env, "<util %d>", self->num);
}

STATIC mp_obj_util_t *util_new(int num)
{
    mp_obj_util_t *o = m_new_obj(mp_obj_util_t);
    o->base.type = &util_type;
    o->num = num;
    printf("new test num %d\n", num);
    return o;
}

//
// Constructor
//
STATIC mp_obj_t util_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args)
{
    static int num = 0;

    if(n_args > 0)
    {
        //assert(MP_OBJ_IS_SMALL_INT(args[0]));
        num = MP_OBJ_SMALL_INT_VALUE(args[0]);
    }
    else
    {
        num++;
    }

    return(util_new(num));
}

//
// Class members
//

STATIC mp_obj_t util_num(mp_obj_t self_in)
{
    mp_obj_util_t *self = self_in;
    printf("test num %d\n", self->num);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(util_num_obj, util_num);

STATIC mp_obj_t util_str(mp_obj_t self_in, const mp_obj_t str)
{
    mp_obj_util_t *self = self_in;
    const char *_str;

    RAISE_EXCEP(MP_OBJ_IS_STR(str), mp_type_TypeError, "invalid type");

    _str = mp_obj_str_get_str(str);

    printf("test%d: %s\n", self->num, _str);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(util_str_obj, util_str);

STATIC mp_obj_t util_args(uint n_args, const mp_obj_t *args)
{
    mp_obj_util_t *self;
    const char *_str;

    RAISE_EXCEP(n_args>1, mp_type_TypeError, "no arguments");
    RAISE_EXCEP(args, mp_type_RuntimeError, "null argument");

    RAISE_EXCEP(args[0], mp_type_RuntimeError, "null argument");
    self = args[0];

    printf("test%d\n", self->num);

    //RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");
    RAISE_EXCEP(args[1], mp_type_RuntimeError, "null argument");
    RAISE_EXCEP(MP_OBJ_IS_STR(args[1]), mp_type_TypeError, "invalid type");
    //fflush(stdout);

    _str = mp_obj_str_get_str(args[1]);

    printf("test%d: %s\n", self->num, _str);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(util_args_obj, 1, 3, util_args);

// Table of class members
STATIC const mp_map_elem_t util_locals_dict_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR_num),  (mp_obj_t)&util_num_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_str),  (mp_obj_t)&util_str_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_args), (mp_obj_t)&util_args_obj },
};
STATIC MP_DEFINE_CONST_DICT(util_locals_dict, util_locals_dict_table);

STATIC const mp_obj_type_t util_type =
{
    { &mp_type_type },
    .name           = MP_QSTR_util,
    .print          = util_print,
    .make_new       = util_make_new,
    .locals_dict    = (mp_obj_t)&util_locals_dict,
};

// Table of global class members
STATIC const mp_map_elem_t util_module_globals_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_util) },
    // Class object
    { MP_OBJ_NEW_QSTR(MP_QSTR_util),    (mp_obj_t)&util_type },
    // Global functions
    { MP_OBJ_NEW_QSTR(MP_QSTR_print),   (mp_obj_t)&glb_print_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_nop),     (mp_obj_t)&glb_nop_obj},
};

STATIC const mp_obj_dict_t util_module_globals =
{
    .base = {&mp_type_dict},
    .map =
    {
        .all_keys_are_qstrs     = 1,
        .table_is_fixed_array   = 1,
        .used                   = MP_ARRAY_SIZE(util_module_globals_table),
        .alloc                  = MP_ARRAY_SIZE(util_module_globals_table),
        .table                  = (mp_map_elem_t *)util_module_globals_table,
    },
};

const mp_obj_module_t util_module =
{
    .base       = { &mp_type_module },
    .name       = MP_QSTR_util,
    .globals    = (mp_obj_dict_t*)&util_module_globals,
};


