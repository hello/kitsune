#include "gesture.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uartstdio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stdbool.h"



static struct{
	int wave_count;
	int hold_count;
	int all_counts_for_diagnostics;
	xSemaphoreHandle gesture_count_semaphore;
}self;

void gesture_init(){
	memset(&self,0,sizeof(self));

	self.gesture_count_semaphore = xSemaphoreCreateMutex();

}

void gesture_increment_wave_count()
{
	analytics_event( "{gesture: wave}" );
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		self.wave_count++;
		self.all_counts_for_diagnostics++;
		xSemaphoreGive(self.gesture_count_semaphore);
	}

}

void gesture_increment_hold_count()
{
	analytics_event( "{gesture: hold}" );
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		self.hold_count++;
		self.all_counts_for_diagnostics++;
		xSemaphoreGive(self.gesture_count_semaphore);
	}

}

int gesture_get_wave_count()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		int ret = self.wave_count;
		xSemaphoreGive(self.gesture_count_semaphore);

		return ret;
	}

	return 0;
}

int gesture_get_hold_count()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		int ret = self.hold_count;
		xSemaphoreGive(self.gesture_count_semaphore);
		return ret;
	}

	return 0;
}

int gesture_get_and_reset_all_diagnostic_counts() {
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		int ret = self.all_counts_for_diagnostics;
		self.all_counts_for_diagnostics = 0;
		xSemaphoreGive(self.gesture_count_semaphore);
		return ret;
	}

	return 0;
}

void gesture_counter_reset()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		self.hold_count = 0;
		self.wave_count = 0;
		xSemaphoreGive(self.gesture_count_semaphore);
	}
}
