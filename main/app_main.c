#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_log.h"

#include "8bkc-hal.h"
#include "appfs.h"

#include "smsplus-main.h"


void app_main()
{
	kchal_init();
//	nvs_flash_init();
	
	smsemuStart();
}


