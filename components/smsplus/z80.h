/* z80.h: z80 emulation core
   Copyright (c) 1999-2002 Philip Kendall

   $Id: z80.h,v 1.5 2003/01/14 15:02:58 pak21 Exp $

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   Author contact information:

   E-mail: pak21-fuse@srcf.ucam.org
   Postal address: 15 Crescent Road, Wokingham, Berks, RG40 2DB, England

*/

#ifndef FUSE_Z80_H
#define FUSE_Z80_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Union allowing a register pair to be accessed as bytes or as a word */
typedef union
{
#ifdef WORDS_BIGENDIAN
  struct { uint8_t h, l; } b;
#else
  struct { uint8_t l, h; } b;
#endif
  uint16_t w;
}
z80_regpair_t;

/* What's stored in the main processor */
typedef struct
{
  z80_regpair_t af, bc, de, hl;
  z80_regpair_t af_, bc_, de_, hl_;
  z80_regpair_t ix, iy;
  uint8_t i;
  uint16_t r; /* The low seven bits of the R register. 16 bits long so it can
                 also act as an RZX instruction counter */
  uint8_t r7; /* The high bit of the R register */
  z80_regpair_t sp, pc;
  uint8_t iff1, iff2, im;
  int halted;
}
z80_t;

void z80_init(z80_t* z80);
void z80_reset(z80_t* z80);

int z80_interrupt(z80_t* z80, int ts);
int z80_nmi(z80_t* z80, int ts);

int z80_do_opcode(z80_t* z80);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef FUSE_Z80_H */
