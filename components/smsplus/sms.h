
#ifndef _SMS_H_
#define _SMS_H_

#define TYPE_OVERSEAS   (0)
#define TYPE_DOMESTIC   (1)

/* SMS context */
typedef struct
{
    uint8 *dummy; //JMD: Point this into outher space plz.
    uint8 ram[0x2000];
//    uint8 sram[0x8000];
    uint8 *sram;
    uint8 fcr[4];
    uint8 paused;
    uint8 save;
    uint8 country;
    uint8 port_3F;
    uint8 port_F2;
    uint8 use_fm;
    uint8 irq;
    uint8 psg_mask;
}t_sms;

/* Global data */
extern t_sms sms;
extern z80_t *Z80_Context;
extern int z80_ICount;

extern unsigned char *cpu_readmap[8];
extern unsigned char *cpu_writemap[8];
void cpu_writemem16(int address, int data);
void cpu_writeport(int port, int data);
int cpu_readport(int port);


/* Function prototypes */
void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);
int  sms_irq_callback(int param);
void sms_mapper_w(int address, int data);
void cpu_reset(void);

#endif /* _SMS_H_ */
