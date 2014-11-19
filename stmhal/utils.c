// --------------------------------------------------------------------------------------
// Module     : UTILS
// Author     : N. El-Fata
// --------------------------------------------------------------------------------------
// Pulse Innovation, Inc. www.pulseinnovation.com
// Copyright (c) 2014, Pulse Innovation, Inc. All rights reserved.
// --------------------------------------------------------------------------------------

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "mpconfig.h"
#include "misc.h"
#include "systick.h"
#include "utils.h"

void delay_usec(int32_t usec)
{
    int32_t tstart;

    tstart = (int32_t)sys_tick_get_microseconds();
    while((int32_t)sys_tick_get_microseconds() - tstart < usec);
}
