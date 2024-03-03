/*
*  Copyright (C) 2018 Huawei Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/
#if defined(HI846_BYD)
{
    .sensorID             = HI846_BYD_SENSOR_ID,
    .e2prom_block_info[0] =
    {
        .AddrOffset   = 0x00,
        .datalen      = 39,
    },
    .e2prom_block_info[1] =
    {
        .AddrOffset   = 0x15ea,
        .datalen      = 2400,
    },
    .e2prom_block_num     = 2,
    .e2prom_total_size    = 2439,
},
#endif
