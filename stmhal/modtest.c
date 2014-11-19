
#include <stdio.h>

//#include "stm32f4xx_hal.h"

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
MP_DEFINE_CONST_FUN_OBJ_1(glb_xprint_obj, glb_print);

STATIC mp_obj_t glb_nop(void)
{
    return(mp_const_none);
}
MP_DEFINE_CONST_FUN_OBJ_0(glb_xnop_obj, glb_nop);

//
// Local
//

typedef struct _mp_obj_test_t
{   // linked list
    mp_obj_base_t base;
    int num;
} mp_obj_test_t;

STATIC const mp_obj_type_t test_type;

STATIC void test_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind)
{
    mp_obj_test_t *self = self_in;
    print(env, "<test %d>", self->num);
}

STATIC mp_obj_test_t *test_new(int num)
{
    mp_obj_test_t *o = m_new_obj(mp_obj_test_t);
    o->base.type = &test_type;
    o->num = num;
    printf("new test num %d\n", num);
    return o;
}

//
// Constructor
//
STATIC mp_obj_t test_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args)
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

    return(test_new(num));
}

//
// Class members
//

STATIC mp_obj_t test_num(mp_obj_t self_in)
{
    mp_obj_test_t *self = self_in;
    printf("test num %d\n", self->num);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(test_num_obj, test_num);

STATIC mp_obj_t test_str(mp_obj_t self_in, const mp_obj_t str)
{
    mp_obj_test_t *self = self_in;
    const char *_str;

    RAISE_EXCEP(MP_OBJ_IS_STR(str), mp_type_TypeError, "invalid type");

    _str = mp_obj_str_get_str(str);

    printf("test%d: %s\n", self->num, _str);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(test_str_obj, test_str);

STATIC mp_obj_t test_args(uint n_args, const mp_obj_t *args)
{
    mp_obj_test_t *self;
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
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(test_args_obj, 1, 3, test_args);

// Table of class members
STATIC const mp_map_elem_t test_locals_dict_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR_num),  (mp_obj_t)&test_num_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_str),  (mp_obj_t)&test_str_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_args), (mp_obj_t)&test_args_obj },
};
STATIC MP_DEFINE_CONST_DICT(test_locals_dict, test_locals_dict_table);

STATIC const mp_obj_type_t test_type =
{
    { &mp_type_type },
    .name           = MP_QSTR_test,
    .print          = test_print,
    .make_new       = test_make_new,
    .locals_dict    = (mp_obj_t)&test_locals_dict,
};

// Table of global class members
STATIC const mp_map_elem_t test_module_globals_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_test) },
    // Class object
    { MP_OBJ_NEW_QSTR(MP_QSTR_test),    (mp_obj_t)&test_type },
    // Global functions
    { MP_OBJ_NEW_QSTR(MP_QSTR_print),   (mp_obj_t)&glb_xprint_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_nop),     (mp_obj_t)&glb_xnop_obj},
};

STATIC const mp_obj_dict_t test_module_globals =
{
    .base = {&mp_type_dict},
    .map =
    {
        .all_keys_are_qstrs     = 1,
        .table_is_fixed_array   = 1,
        .used                   = MP_ARRAY_SIZE(test_module_globals_table),
        .alloc                  = MP_ARRAY_SIZE(test_module_globals_table),
        .table                  = (mp_map_elem_t *)test_module_globals_table,
    },
};

const mp_obj_module_t test_module =
{
    .base       = { &mp_type_module },
    .name       = MP_QSTR_test,
    .globals    = (mp_obj_dict_t*)&test_module_globals,
};


