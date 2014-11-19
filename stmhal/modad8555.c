// --------------------------------------------------------------------------------------
// Module     : MODAD8555
// Author     : N. El-Fata
//
// Description: This module manipulated the AD8555 digitally controlable opamp .
//
// --------------------------------------------------------------------------------------
// Pulse Innovation, Inc. www.pulseinnovation.com
// Copyright (c) 2014, Pulse Innovation, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "mpconfig.h"
#include "nlr.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "runtime.h"

#include "pin.h"
#include "genhdr/pins.h"

#include "log.h"
#include "utils.h"

//#include "modad8555.h"
#define PIN_DIG_IN  pin_C1 // PC1/X20

typedef enum
{
    MODE_SIM = 0,   // simulation mode
    MODE_PROG,      // programming mode
} admode_t;

typedef enum
{
    FUNCTION_CHANGE_SENSE_CURRENT = 0x00,
    FUNCTION_SIMULATE_PARAM_VAL   = 0x01,
    FUNCTION_PROGRAM_PARAM_VAL    = 0x02,
    FUNCTION_READ_PARAM_VAL       = 0x03,
} func_t;

typedef enum
{
    PARAM_SECOND_STAGE_GAIN = 0x00,
    PARAM_FIRST_STAGE_GAIN  = 0x01,
    PARAM_OUTPUT_OFFSET     = 0x02,
    PARAM_OTHER             = 0x03,
} param_t;

typedef struct _mp_obj_mod_t
{
    mp_obj_base_t base;
    admode_t      mode;
    uint8_t       gain1;
    uint8_t       gain2;
    uint8_t       offset;
} mp_obj_mod_t;

static mp_obj_mod_t *mp_obj_ad8555 = 0;

static void bit_set(void) // 1 = long pulse
{
    // set high
    PIN_DIG_IN.gpio->BSRRL = PIN_DIG_IN.pin_mask;
    // wait pulse duration (at least 50usec)
    delay_usec(2*51);
    // set low
    PIN_DIG_IN.gpio->BSRRH = PIN_DIG_IN.pin_mask;
}

static void bit_clr(void) // 0 = short pulse
{
    // to guarantee the duration we need to disable the interrupts
    // only when setting low, as there is an upper limit for the pulse duration
    __disable_irq();

    // set high
    PIN_DIG_IN.gpio->BSRRL = PIN_DIG_IN.pin_mask;
    // wait pulse duration (50ns to 10usec)
    delay_usec(2*1);
    // set low
    PIN_DIG_IN.gpio->BSRRH = PIN_DIG_IN.pin_mask;

    __enable_irq();
}

static void bits_send(uint32_t bits, uint8_t len)
{
    uint8_t i, bit;

    for(i=0; i<len; i++)
    {   // write MSB first
        bit = (bits >> (len - i - 1)) & 1;
        if(bit)
        {
            bit_set();
        }
        else
        {
            bit_clr();
        }
        // wait at least 10usec in between pulses
        delay_usec(2*10+1); // use 11 to be cautious
    }
}

static void word_send(func_t function, param_t param, uint8_t val)
{
    // field 0
    bits_send(0x801, 12);   // start of packet
    // field 1
    bits_send(function, 2);
    // field 2
    bits_send(param, 2);
    // field 3
    bits_send(0x2, 2);      // dummy
    // field 4
    bits_send(val, 8);
    // field 5
    bits_send(0x7FE, 12);   // end of packet
}

// Enable/Disable Simulation mode
STATIC mp_obj_t sim(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self;

    self = args[0];

    if(n_args < 2)
    {
        return(mp_obj_new_int(self->mode));
    }
    RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");

    switch(mp_obj_get_int(args[1]))
    {
        case 0:
            LOG_INFO("Programming mode disabled\n");
            //self->mode = MODE_PROG; // enable programming mode
            break;
        case 1:
            self->mode = MODE_SIM; // enable simulation mode
            break;
        default: // not allowed
            break;
    }

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sim_obj, 1, 2, sim);

STATIC mp_obj_t gain1(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self;
    func_t func = FUNCTION_SIMULATE_PARAM_VAL;

    self = args[0];

    if(n_args < 2)
    {
        word_send(FUNCTION_READ_PARAM_VAL, PARAM_FIRST_STAGE_GAIN, 0);
        return(mp_obj_new_int(self->gain1));
    }
    RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");

    if(self->mode == MODE_PROG)
    {
        func = FUNCTION_PROGRAM_PARAM_VAL;
    }

    uint8_t gain = mp_obj_get_int(args[1]) & 0x3F;

    // only the 7 LSB bits used
    word_send(func, PARAM_FIRST_STAGE_GAIN, gain);

    self->gain1 = gain;

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gain1_obj, 1, 2, gain1);

STATIC mp_obj_t gain2(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self;
    func_t func = FUNCTION_SIMULATE_PARAM_VAL;

    self = args[0];

    if(n_args < 2)
    {
        word_send(FUNCTION_READ_PARAM_VAL, PARAM_SECOND_STAGE_GAIN, 0);
        return(mp_obj_new_int(self->gain2));
    }
    RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");

    if(self->mode == MODE_PROG)
    {
        func = FUNCTION_PROGRAM_PARAM_VAL;
    }

    uint8_t gain = mp_obj_get_int(args[1]) & 0x07;

    // only the 3 LSB bits used
    word_send(func, PARAM_SECOND_STAGE_GAIN, gain);

    self->gain2 = gain;

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gain2_obj, 1, 2, gain2);

STATIC mp_obj_t offs(uint n_args, const mp_obj_t *args)
{
    mp_obj_mod_t *self;
    func_t func = FUNCTION_SIMULATE_PARAM_VAL;

    self = args[0];

    if(n_args < 2)
    {
        word_send(FUNCTION_READ_PARAM_VAL, PARAM_OUTPUT_OFFSET, 0);
        return(mp_obj_new_int(self->offset));
    }
    RAISE_EXCEP(MP_OBJ_IS_INT(args[1]), mp_type_TypeError, "invalid type");

    if(self->mode == MODE_PROG)
    {
        func = FUNCTION_PROGRAM_PARAM_VAL;
    }

    uint8_t offset;

    offset =  mp_obj_get_int(args[1]) & 0xFF;

    word_send(func, PARAM_OUTPUT_OFFSET, offset);

    self->offset = offset;

    return(mp_const_none);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(offset_obj, 1, 2, offs);


// ---------------------------------------------------------
// MP Bindings
// ---------------------------------------------------------
STATIC const mp_obj_type_t mod_type;

STATIC mp_obj_mod_t *_new(void)
{
    mp_obj_mod_t *o = 0;

    if(mp_obj_ad8555 == 0)
    {   // first time
        o = m_new_obj(mp_obj_mod_t);
        RAISE_EXCEP(o, mp_type_BaseException, "null");

        o->base.type = &mod_type;
        o->mode   = MODE_SIM; // simulation mode by default
        o->gain1  = 0;
        o->gain2  = 0;
        o->offset = 0;

        mp_obj_ad8555 = o;

        // configure the digital output pin
        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.Speed        = GPIO_SPEED_FAST;
        GPIO_InitStructure.Mode         = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStructure.Pull         = GPIO_PULLDOWN;
        GPIO_InitStructure.Alternate    = 0;
        GPIO_InitStructure.Pin          = PIN_DIG_IN.pin_mask;
        HAL_GPIO_Init(PIN_DIG_IN.gpio, &GPIO_InitStructure);
        PIN_DIG_IN.gpio->BSRRH = PIN_DIG_IN.pin_mask; // set it low
    }
    else
    {   // return previously created instance, cannot have more than one anyway
        o = mp_obj_ad8555;
        //RAISE_EXCEP(0, mp_type_BaseException, "only one instance allowed");
    }

    return(o);
}

STATIC mp_obj_t make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args)
{
    return(_new());
}

STATIC const mp_map_elem_t locals_dict_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR_sim),         (mp_obj_t)&sim_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_gain1),       (mp_obj_t)&gain1_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_gain2),       (mp_obj_t)&gain2_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_offset),      (mp_obj_t)&offset_obj},
};
STATIC MP_DEFINE_CONST_DICT(locals_dict, locals_dict_table);

STATIC const mp_obj_type_t mod_type =
{
    { &mp_type_type },
    .name           = MP_QSTR_ad8555,
    .make_new       = make_new,
    .locals_dict    = (mp_obj_t)&locals_dict,
};

STATIC const mp_map_elem_t module_globals_table[] =
{
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),    MP_OBJ_NEW_QSTR(MP_QSTR_ad8555) },
    // Class object
    { MP_OBJ_NEW_QSTR(MP_QSTR_ad8555),      (mp_obj_t)&mod_type},
};

STATIC const mp_obj_dict_t module_globals =
{
    .base = {&mp_type_dict},
    .map =
    {
        .all_keys_are_qstrs     = 1,
        .table_is_fixed_array   = 1,
        .used                   = MP_ARRAY_SIZE(module_globals_table),
                                  .alloc                  = MP_ARRAY_SIZE(module_globals_table),
                                                            .table                  = (mp_map_elem_t*)module_globals_table,
    },
};

const mp_obj_module_t ad8555_module =
{
    .base = { &mp_type_module },
    .name = MP_QSTR_ad8555,
    .globals = (mp_obj_dict_t*)&module_globals,
};


