/* z80.c: z80 supplementary functions
   Copyright (c) 1999, 2000, 2002 Philip Kendall
   EightyOne Modifications (c) 2003-2006 Michael D Wynne

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

#include "z80.h"
#include "z80_macros.h"

#include "z80cfg.h"

/* Whether a half carry occured or not can be determined by looking at
   the 3rd bit of the two arguments and the result; these are hashed
   into this table in the form r12, where r is the 3rd bit of the
   result, 1 is the 3rd bit of the 1st argument and 2 is the
   third bit of the 2nd argument; the tables differ for add and subtract
   operations */
uint8_t halfcarry_add_table[] = { 0, FLAG_H, FLAG_H, FLAG_H, 0, 0, 0, FLAG_H };
uint8_t halfcarry_sub_table[] = { 0, 0, FLAG_H, 0, FLAG_H, 0, FLAG_H, FLAG_H };

/* Similarly, overflow can be determined by looking at the 7th bits; again
   the hash into this table is r12 */
uint8_t overflow_add_table[] = { 0, 0, 0, FLAG_V, FLAG_V, 0, 0, 0 };
uint8_t overflow_sub_table[] = { 0, FLAG_V, 0, 0, 0, 0, FLAG_V, 0 };

/* Some more tables; initialised in z80_init_tables() */
uint8_t sz53_table[0x100]; /* The S, Z, 5 and 3 bits of the lookup value */
uint8_t parity_table[0x100]; /* The parity of the lookup value */
uint8_t sz53p_table[0x100]; /* OR the above two tables together */

/* Initalise the tables used to set flags */
static void z80_init_tables(z80_t* z80)
{
  int i, j, k;
  uint8_t parity;

  (void)z80;
  
  for (i = 0; i < 0x100; i++)
  {
    sz53_table[i] = i & (FLAG_3 | FLAG_5 | FLAG_S);
    j = i;
    parity = 0;
    
    for (k = 0; k < 8; k++)
    {
      parity ^= j & 1;
      j >>= 1;
    }

    parity_table[i] = (parity ? 0 : FLAG_P);
    sz53p_table[i] = sz53_table[i] | parity_table[i];
  }

  sz53_table[0]  |= FLAG_Z;
  sz53p_table[0] |= FLAG_Z;
}

/* Set up the z80 emulation */
void z80_init(z80_t* z80)
{
  z80_init_tables(z80);
}

/* Reset the z80 */
void z80_reset(z80_t* z80)
{
  AF = BC = DE = HL =0;
  AF_ = BC_ = DE_ = HL_=0;
  IX = IY = 0;
  I = R = R7 = 0;
  SP = PC = 0;
  IFF1 = IFF2 = IM = 0;
  z80->halted=0;
}

/* Process a z80 maskable interrupt */
int z80_interrupt(z80_t* z80, int ts)
{
  /* Process if IFF1 set */
  if (IFF1)
  {
    if (z80->halted)
    {
      PC++;
      z80->halted = 0;
    }

    writebyte(--SP, PCH);
    writebyte(--SP, PCL);
    IFF1 = IFF2 = 0;
    R++;

    switch (IM)
    {
    case 0: PC = 0x0038; return 13;
    case 1: PC = 0x0038; return 13;
    case 2:
    {
      uint16_t inttemp = (0x100 * I) + 0xff;
      PCL = readbyte(inttemp++);
      PCH = readbyte(inttemp);
      return 19;
    }
    
    default: return 12;
    }
  }
  
  return 0;
}

/* Process a z80 non-maskable interrupt */
int z80_nmi(z80_t* z80, int ts)
{
  int waitstates = 0;

  IFF1 = 0;

  if (z80->halted)
  {
    z80->halted = 0;
    PC++;

//    waitstates = (ts / 2) - machine.tperscanline;
//    waitstates = 4 - waitstates;
    
//    if (waitstates < 0)
//    {
      waitstates = 0;
//    }
  }

  writebyte(--SP, PCH);
  writebyte(--SP, PCL);
  R++;
  PC = 0x0066;

  return 4 + waitstates;
}
