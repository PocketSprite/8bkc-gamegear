
#include "shared.h"
#include "system.h"
#include "appfs.h"

extern t_bitmap bitmap;
extern t_cart cart;
extern t_snd snd;
extern t_input input;

#define appfs_replace_fwrite(adr, sz, ct, fd) do { appfsWrite(fd, loc, adr, sz*ct); loc+=sz*ct; } while(0)
#define appfs_replace_fread(adr, sz, ct, fd) do { appfsRead(fd, loc, adr, sz*ct); loc+=sz*ct; } while(0)

void sms_system_save_state(void *gfd)
{
	int fd=(int)gfd;
	size_t loc=0;

	appfsErase(fd, 0, (1<<16));

    /* Save VDP context */
    appfs_replace_fwrite(&vdp, sizeof(t_vdp), 1, fd);

    /* Save SMS context */
    appfs_replace_fwrite(&sms, sizeof(t_sms), 1, fd);

    /* Save Z80 context */
    appfs_replace_fwrite(Z80_Context, sizeof(z80_t), 1, fd);
//    appfs_replace_fwrite(&after_EI, sizeof(int), 1, fd);

#if 0
    /* Save YM2413 registers */
    fwrite(&ym2413.reg[0], 0x40, 1, fd);
#endif

    /* Save SN76489 context */
    appfs_replace_fwrite(&sn[0], sizeof(t_SN76496), 1, fd);

    /* Save sram context */
    appfs_replace_fwrite(sms.sram, 0x8000, 1, fd);
}


void sms_system_load_state(void *gfd)
{
	int fd=(int)gfd;
	size_t loc=0;
    int i;
#if 0
    uint8 reg[0x40];
#endif

    /* Initialize everything */
    cpu_reset();
    sms_system_reset();

    /* Load VDP context */
    appfs_replace_fread(&vdp, sizeof(t_vdp), 1, fd);

    /* SMS context contains two pointers we need to save. The load overwrites them otherwise */
    uint8_t *old_sram=sms.sram;
    uint8_t *old_dummy=sms.dummy;
    /* Load SMS context */
    appfs_replace_fread(&sms, sizeof(t_sms), 1, fd);
    /* Restore ptrs */
    sms.sram=old_sram;
    sms.dummy=old_dummy;

    /* Load Z80 context */
    appfs_replace_fread(Z80_Context, sizeof(z80_t), 1, fd);
//    appfs_replace_fread(&after_EI, sizeof(int), 1, fd);

#if 0
    /* Load YM2413 registers */
    appfs_replace_fread(reg, 0x40, 1, fd);
#endif

    /* Load SN76489 context */
    appfs_replace_fread(&sn[0], sizeof(t_SN76496), 1, fd);

    /* Load sram */
    appfs_replace_fread(sms.sram, 0x8000, 1, fd);

    /* Restore callbacks */
    //z80_set_irq_callback(sms_irq_callback);

    cpu_readmap[0] = cart.rom + 0x0000; /* 0000-3FFF */
    cpu_readmap[1] = cart.rom + 0x2000;
    cpu_readmap[2] = cart.rom + 0x4000; /* 4000-7FFF */
    cpu_readmap[3] = cart.rom + 0x6000;
    cpu_readmap[4] = cart.rom + 0x0000; /* 0000-3FFF */
    cpu_readmap[5] = cart.rom + 0x2000;
    cpu_readmap[6] = sms.ram;
    cpu_readmap[7] = sms.ram;

    cpu_writemap[0] = sms.dummy;
    cpu_writemap[1] = sms.dummy;
    cpu_writemap[2] = sms.dummy;         
    cpu_writemap[3] = sms.dummy;
    cpu_writemap[4] = sms.dummy;         
    cpu_writemap[5] = sms.dummy;
    cpu_writemap[6] = sms.ram;           
    cpu_writemap[7] = sms.ram;

    sms_mapper_w(3, sms.fcr[3]);
    sms_mapper_w(2, sms.fcr[2]);
    sms_mapper_w(1, sms.fcr[1]);
    sms_mapper_w(0, sms.fcr[0]);

    /* Force full pattern cache update */
//  is_vram_dirty = 1;
//  memset(vram_dirty, 1, 0x200);

    /* Restore palette */
    for(i = 0; i < PALETTE_SIZE; i += 1)
        palette_sync(i);

    /* Restore sound state */
    if(snd.enabled)
    {
#if 0
        /* Clear YM2413 context */
        OPLL_reset(opll) ;
        OPLL_reset_patch(opll,0) ;            /* if use default voice data. */ 

        /* Restore rhythm enable first */
        ym2413_write(0, 0, 0x0E);
        ym2413_write(0, 1, reg[0x0E]);

        /* User instrument settings */
        for(i = 0x00; i <= 0x07; i += 1)
        {
            ym2413_write(0, 0, i);
            ym2413_write(0, 1, reg[i]);
        }

        /* Channel frequency */
        for(i = 0x10; i <= 0x18; i += 1)
        {
            ym2413_write(0, 0, i);
            ym2413_write(0, 1, reg[i]);
        }

        /* Channel frequency + ctrl. */
        for(i = 0x20; i <= 0x28; i += 1)
        {
            ym2413_write(0, 0, i);
            ym2413_write(0, 1, reg[i]);
        }

        /* Instrument and volume settings  */
        for(i = 0x30; i <= 0x38; i += 1)
        {
            ym2413_write(0, 0, i);
            ym2413_write(0, 1, reg[i]);
        }
#endif
    }
}
