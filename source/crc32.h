/*
 * crc32.c -- 32-bit cyclic reduncancy check
 *
 * Copyright (C) 2000 Finn Yannick Jacobs
 * Copyright (C) 2008 Pyriel
 *
 * This file is part of Omniconvert.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * I'm not really sure how copyright works on this.  I wrote the code using a standard reference.
 * But I'm not sure who actually originated it.
 */

#ifndef _CRC32_H_
#define _CRC32_H_

#include "abbtypes.h"

u32 crc32(const u8 *buffer, u32 size, u32 crc);

#endif //_CRC32_H_
