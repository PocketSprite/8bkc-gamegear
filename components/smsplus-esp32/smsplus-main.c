#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "8bkc-hal.h"
#include "appfs.h"
#include "shared.h"

#define SMS_FPS 60
#define SNDRATE 22050


static SemaphoreHandle_t renderSem;

static void readJs() {
	//Convert buttons to SMS buttons
	int b=kchal_get_keys();
	int smsButtons=0, smsSystem=0;
	if (b&KC_BTN_UP) smsButtons|=INPUT_UP;
	if (b&KC_BTN_DOWN) smsButtons|=INPUT_DOWN;
	if (b&KC_BTN_LEFT) smsButtons|=INPUT_LEFT;
	if (b&KC_BTN_RIGHT) smsButtons|=INPUT_RIGHT;
	if (b&KC_BTN_A) smsButtons|=INPUT_BUTTON1;
	if (b&KC_BTN_B) smsButtons|=INPUT_BUTTON2;
	if (b&KC_BTN_START) smsSystem|=INPUT_START;
	if (b&KC_BTN_SELECT) smsSystem|=INPUT_PAUSE;
//	if (b&KC_BTN_UP) smsSystem|=INPUT_SOFT_RESET;
//	if (b&KC_BTN_UP) smsSystem|=INPUT_HARD_RESET;
	input.pad[0]=smsButtons;
	input.system=smsSystem;
}



uint16_t oledBuf[80*64];

int dframe;


//SMS screen is the full 256x192
static void lcdWriteSMSFrame() {
	int rgb[3];
	uint8_t *data=bitmap.data;
	uint16_t *p=oledBuf;
	for (int y=0; y<192; y+=3) {
		for (int x=0; x<240; x+=3) {
			for (int sp=0; sp<3; sp++) {
				rgb[sp]=bitmap.pal.color[data[(x+sp)+((y+0)*256)]&PIXEL_MASK][sp];
				rgb[sp]+=bitmap.pal.color[data[(x+sp)+((y+1)*256)]&PIXEL_MASK][sp];
				rgb[sp]+=bitmap.pal.color[data[(x+sp)+((y+2)*256)]&PIXEL_MASK][sp];
				rgb[sp]/=3;
			}
			uint16_t col=((rgb[0]>>3)<<11)+((rgb[1]>>2)<<5)+((rgb[2]>>3)<<0);
			*p++=(col>>8)|((col&0xff)<<8);
		}
	}
	kchal_send_fb(oledBuf);
	dframe++;
}

//GG screen is 160*144 with an offset of +48+24
static void lcdWriteGGFrame() {
	uint8_t *data=bitmap.data;
	uint16_t *p=oledBuf;
	uint32_t pal[2][32];
	for (int x=0; x<32; x++) {
		pal[0][x]=((bitmap.pal.color[x][0]>>3)<<11)+((bitmap.pal.color[x][1]>>3)<<5);
		pal[1][x]=((bitmap.pal.color[x][1]>>3)<<5)+((bitmap.pal.color[x][2]>>3)<<0);
	}

	for (int y=24; y<128+24; y+=2) {
		for (int x=48; x<160+48; x+=2) {
			uint32_t c=0;
			c+=pal[0][data[(x)+((y)*256)]&PIXEL_MASK];
			c+=pal[1][data[(x+1)+((y)*256)]&PIXEL_MASK];
			c+=pal[0][data[(x)+((y+1)*256)]&PIXEL_MASK];
			c+=pal[1][data[(x+1)+((y+1)*256)]&PIXEL_MASK];
			c/=4;
//			uint16_t col=((rgb[0]>>3)<<11)+((rgb[1]>>2)<<5)+((rgb[2]>>3)<<0);
			*p++=(c>>8)|((c&0xff)<<8);
		}
	}
	kchal_send_fb(oledBuf);
	dframe++;
}



static void lcdThread(void *arg) {
	while(1) {
		xSemaphoreTake(renderSem, portMAX_DELAY);
		if (cart.type==TYPE_SMS) {
			lcdWriteSMSFrame();
		} else {
			lcdWriteGGFrame();
		}
	}
}

void sms_system_load_sram(void) {
}


spi_flash_mmap_handle_t hrom;

void smsemuRun(char *rom) {
	int frameno;
	int x;
	uint8_t sbuf[(SNDRATE/SMS_FPS)];
	sms.use_fm=0;
	sms.country=TYPE_OVERSEAS;
	bitmap.data=malloc(256*192);
	bitmap.width=256;
	bitmap.height=192;
	bitmap.pitch=256;
	bitmap.depth=8;
	sms.dummy=bitmap.data; //A normal cart shouldn't access this memory ever. Point it to vram just in case.
	sms.sram=malloc(0x8000);
	
	int sz;
	if (appfsExists(rom)) {
		appfs_handle_t fd=appfsOpen(rom);
		appfsEntryInfo(fd, NULL, &sz);
		esp_err_t err=appfsMmap(fd, 0, sz, (const void**)&cart.rom, SPI_FLASH_MMAP_DATA, &hrom);
		if (err!=ESP_OK) {
			printf("Error: mmap for rom failed\n");
		}
	} else {
		printf("Error: %s does not exist.\n", rom);
		return;
	}
	
	cart.pages=(sz/0x4000);
	cart.type=TYPE_GG;
	printf("Mmap: Cart has %d pages.\n", cart.pages);

	kchal_sound_start(SNDRATE, 2048);
	sms_system_init(SNDRATE);
	printf("Sound buffer: %d samples, enabled=%d.\n", snd.bufsize, snd.enabled);
	lastTickCnt=0;
	while(1) {
		for (frameno=0; frameno<SMS_FPS; frameno++) {
			readJs();
			sms_frame(0);
			xSemaphoreGive(renderSem);
			for (x=0; x<(SNDRATE/SMS_FPS); x++) {
				sbuf[x]=((snd.buffer[0][x]+snd.buffer[1][x])/512)+128;
			}
			kchal_sound_push(sbuf, (SNDRATE/SMS_FPS));
		}
		tickCnt=xTaskGetTickCount();
		if (tickCnt==lastTickCnt) tickCnt++;
		printf("fps=%d disp=%d\n", (SMS_FPS*100)/(tickCnt-lastTickCnt), dframe);
		dframe=0;
		lastTickCnt=tickCnt;
	}
}

void emuThread(void *arg) {
	smsemuRun("Sonic2.gg");
}

void smsemuStart() {
	renderSem=xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(&emuThread, "emuThread", 1024*8, NULL, 5, NULL, 0);
	xTaskCreatePinnedToCore(&lcdThread, "lcdThread", 1024*4, NULL, 5, NULL, 1);
}
