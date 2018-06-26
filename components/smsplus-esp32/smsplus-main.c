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
#include "8bkc-ugui.h"
#include "8bkcgui-widgets.h"
#include "appfs.h"
#include "shared.h"
#include "menu.h"
#include "smsplus-main.h"
#include "ugui.h"
#include "powerbtn_menu.h"

#define SMS_FPS 60
#define SNDRATE 22050


static SemaphoreHandle_t renderSem;

static int readJs() {
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
	return (b&KC_BTN_POWER);
}



uint16_t oledBuf[80*64];
uint32_t overlay[80*64];
int showOverlay=false;

int addOverlayPixel(uint16_t p, uint32_t ov) {
	int or, og, ob, a;
	int br, bg, bb;
	int r,g,b;
	br=((p>>11)&0x1f)<<3;
	bg=((p>>5)&0x3f)<<2;
	bb=((p>>0)&0x1f)<<3;

	a=(ov>>24)&0xff;
	//hack: Always show background darker
	a=(a/2)+128;

	ob=(ov>>16)&0xff;
	og=(ov>>8)&0xff;
	or=(ov>>0)&0xff;

	r=(br*(256-a))+(or*a);
	g=(bg*(256-a))+(og*a);
	b=(bb*(256-a))+(ob*a);

	return ((r>>(3+8))<<11)+((g>>(2+8))<<5)+((b>>(3+8))<<0);
}


int dframe;


//SMS screen is the full 256x192
static void lcdWriteSMSFrame() {
	int rgb[3];
	uint8_t *data=bitmap.data;
	uint16_t *p=oledBuf;
	uint32_t *ov=overlay;
	for (int y=0; y<192; y+=3) {
		for (int x=0; x<240; x+=3) {
			for (int sp=0; sp<3; sp++) {
				rgb[sp]=bitmap.pal.color[data[(x+sp)+((y+0)*256)]&PIXEL_MASK][sp];
				rgb[sp]+=bitmap.pal.color[data[(x+sp)+((y+1)*256)]&PIXEL_MASK][sp];
				rgb[sp]+=bitmap.pal.color[data[(x+sp)+((y+2)*256)]&PIXEL_MASK][sp];
				rgb[sp]/=3;
			}
			uint16_t col=((rgb[0]>>3)<<11)+((rgb[1]>>2)<<5)+((rgb[2]>>3)<<0);
			if (showOverlay) col=addOverlayPixel(col, *ov++);
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
	uint32_t *ov=overlay;
	uint32_t pal[2][32];
	for (int x=0; x<32; x++) {
		pal[0][x]=((bitmap.pal.color[x][0]>>3)<<11)+((bitmap.pal.color[x][1]>>3)<<5);
		pal[1][x]=((bitmap.pal.color[x][1]>>3)<<5)+((bitmap.pal.color[x][2]>>3)<<0);
	}

	int t=0;
	for (int y=24; y<144+24; y+=2) {
		for (int x=48; x<160+48; x+=2) {
			uint32_t c=0;
			c+=pal[0][data[(x)+((y)*256)]&PIXEL_MASK];
			c+=pal[1][data[(x+1)+((y)*256)]&PIXEL_MASK];
			c+=pal[0][data[(x)+((y+1)*256)]&PIXEL_MASK];
			c+=pal[1][data[(x+1)+((y+1)*256)]&PIXEL_MASK];
			c/=4;
			if (showOverlay) c=addOverlayPixel(c, *ov++);
			*p++=(c>>8)|((c&0xff)<<8);
		}
		t++;
		if (t==4) {
			y++;
			t=0;
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


uint32_t *vidGetOverlayBuf() {
	return overlay;
}

void vidRenderOverlay() {
	showOverlay=true;
	xSemaphoreGive(renderSem);
}

#define STATE_TMP_FILE "__smsplus_state.tmp"

//Runs the emu until user quits it in some way
int smsemuRun(char *rom, char *statefile, int loadState) {
	spi_flash_mmap_handle_t hrom=NULL;
	int frameno;
	int tickCnt, lastTickCnt;
	int x;
	int ret=EMU_RUN_CONT;
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
	//We don't do paging here: that means we do not support GG/SMS cartridges >2MiB
	if (appfsExists(rom)) {
		appfs_handle_t fd=appfsOpen(rom);
		appfsEntryInfo(fd, NULL, &sz);
		esp_err_t err=appfsMmap(fd, 0, sz, (const void**)&cart.rom, SPI_FLASH_MMAP_DATA, &hrom);
		if (err!=ESP_OK) {
			printf("Error: mmap for rom failed\n");
			goto exitemu;
		}
	} else {
		printf("Error: %s does not exist.\n", rom);
		goto exitemu;
	}
	
	cart.pages=(sz/0x4000);
	cart.type=TYPE_SMS;
	if (strcasecmp(rom+strlen(rom)-3, ".gg")==0) {
		cart.type=TYPE_GG;
	}

	printf("Mmap: Cart is for %s and has %d pages.\n", (cart.type==TYPE_GG)?"GameGear":"SMS", cart.pages);

	kchal_sound_start(SNDRATE, 2048);
	sms_system_init(SNDRATE);
	if (loadState) {
		appfs_handle_t fd=appfsOpen(statefile);
		if (fd<0) {
			printf("Couldn't load state %s\n", statefile);
		} else {
			sms_system_load_state(fd);
			appfsClose(fd);
		}
	}
	printf("Sound buffer: %d samples, enabled=%d.\n", snd.bufsize, snd.enabled);
	lastTickCnt=0;
	while(ret==EMU_RUN_CONT) {
		for (frameno=0; frameno<SMS_FPS; frameno++) {
			int showMenu=readJs();
			if (showMenu) {
				ret=menuShow();
				//wait till power button is released
				while(readJs()) vTaskDelay(10);
				if (ret!=EMU_RUN_CONT) break;
			}
			sms_frame(0);
			showOverlay=false;
			xSemaphoreGive(renderSem);
			for (x=0; x<(SNDRATE/SMS_FPS); x++) {
				sbuf[x]=((snd.buffer[0][x]+snd.buffer[1][x])/512)+128;
			}
			kchal_sound_push(sbuf, (SNDRATE/SMS_FPS));
		}
		tickCnt=xTaskGetTickCount();
		if (tickCnt==lastTickCnt) tickCnt++;
		printf("fps=%d disp=%d\n", (SMS_FPS*100)/(tickCnt-lastTickCnt), dframe);
		printf("Free mem: %d\n", xPortGetFreeHeapSize());
		dframe=0;
		lastTickCnt=tickCnt;
	}

	if (ret==EMU_RUN_NEWROM || ret==EMU_RUN_POWERDOWN || ret==EMU_RUN_EXIT) {
		//Save state
		appfs_handle_t fd;
		esp_err_t r;
		r=appfsCreateFile(STATE_TMP_FILE, 1<<16, &fd);
		if (r!=ESP_OK) {
			//Not enough room... delete old state file and retry
			appfsDeleteFile(statefile);
			r=appfsCreateFile(STATE_TMP_FILE, 1<<16, &fd);
		}
		if (r!=ESP_OK) {
			printf("Couldn't create save state %s: %d\n", statefile, r);
		} else {
			sms_system_save_state(fd);
			appfsClose(fd);
			appfsRename(STATE_TMP_FILE, statefile);
		}
	}

	sms_system_shutdown();
	kchal_sound_stop();
exitemu:

	vTaskDelay(40/portTICK_RATE_MS); //make sure render thread is done
	spi_flash_munmap(hrom);
	free(bitmap.data);
	free(sms.sram);
	return ret;
}

static void debug_screen() {
	kcugui_cls();
	UG_FontSelect(&FONT_6X8);
	UG_SetForecolor(C_WHITE);
	UG_PutString(0, 0, "INFO");
	UG_SetForecolor(C_YELLOW);
	UG_PutString(0, 16, "SMSPlus");
	UG_PutString(0, 24, "Gitrev");
	UG_SetForecolor(C_WHITE);
	UG_PutString(0, 32, GITREV);
	UG_SetForecolor(C_YELLOW);
	UG_PutString(0, 40, "Compiled");
	UG_SetForecolor(C_WHITE);
	UG_PutString(0, 48, COMPILEDATE);
	kcugui_flush();

	while (kchal_get_keys()&KC_BTN_SELECT) vTaskDelay(100/portTICK_RATE_MS);
	while (!(kchal_get_keys()&KC_BTN_SELECT)) vTaskDelay(100/portTICK_RATE_MS);
}

static int fccallback(int button, char **glob, char **desc, void *usrptr) {
	if (button & KC_BTN_POWER) {
		int r=powerbtn_menu_show(kcugui_get_fb());
		//No need to save state or whatever; we're not in a game.
		if (r==POWERBTN_MENU_EXIT) kchal_exit_to_chooser();
		if (r==POWERBTN_MENU_POWERDOWN) kchal_power_down();
	}
	if (button & KC_BTN_SELECT) debug_screen();
	return 0;
}


void emuThread(void *arg) {
	char rom[128]="";
	char statefile[130];
	nvs_handle nvsh;
	//Let other threads start
	vTaskDelay(200/portTICK_PERIOD_MS);
	nvsh=kchal_get_app_nvsh();

	int ret;
	int loadState=1;

	unsigned int size=sizeof(rom);
	nvs_get_str(nvsh, "rom", rom, &size);

	while(1) {
		int emuRan=0;
		if (strlen(rom)>0 && appfsExists(rom)) {
			//Figure out name for statefile
			strcpy(statefile, rom);
			char *dot=strrchr(statefile, '.');
			if (dot==NULL) dot=statefile+strlen(statefile);
			strcpy(dot, ".state");
			printf("State file: %s\n", statefile);
			//Kill rom str so when emu crashes, we don't try to load again
			nvs_set_str(nvsh, "rom", "");
			//Run emu
			kchal_sound_mute(0);
			ret=smsemuRun(rom, statefile, loadState);
			emuRan=1;
		} else {
			ret=EMU_RUN_NEWROM;
		}

		if (ret==EMU_RUN_NEWROM) {
			kcugui_init();
			appfs_handle_t f=kcugui_filechooser("*.gg,*.sms", "SELECT ROM", fccallback, NULL, 0);
			const char *rrom;
			appfsEntryInfo(f, &rrom, NULL);
			strncpy(rom, rrom, sizeof(rom));
			printf("Selected ROM %s\n", rom);
			kcugui_deinit();
			loadState=1;
		} else if (ret==EMU_RUN_RESET) {
			loadState=0;
		} else if (ret==EMU_RUN_POWERDOWN) {
			nvs_set_str(nvsh, "rom", rom);
			break;
		} else if (ret==EMU_RUN_EXIT) {
			printf("Exiting to chooser...\n");
			kchal_exit_to_chooser();
		}
	}
	kchal_power_down();
}



void smsemuStart() {
	renderSem=xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(&emuThread, "emuThread", 1024*8, NULL, 5, NULL, 0);
	xTaskCreatePinnedToCore(&lcdThread, "lcdThread", 1024*4, NULL, 5, NULL, 1);
}
