#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "TestNetwork.h"
#include "networktask.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sync_response.pb.h"
#include "periodic.pb.h"
#include "matmessageutils.h"
#include "wifi_cmd.h"

#include "led_cmd.h"

#include "uartstdio.h"

/*
 *  Makes request to test server with particular test cases
 *
 *
 *
 */

#define ERROR_CODE_SUCCESS       (0)
#define ERROR_CODE_FAILED_SEND   (-1)
#define ERROR_CODE_FAILED_DECODE (-2)

#if 1
#define USE_DEBUG_PRINTF
#endif


#define STR(x) #x
#define STRINGIFY(x) STR(x)

#define RUN_TEST(test,code)\
		RunTest(test,STRINGIFY(test),code)




#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  LOGI("%s :: ",__func__); LOGI(__VA_ARGS__); LOGI("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

#define TIMEOUT "timeout"
#define RETURNCODE "returncode"
#define MALFORMED "malformed"



static char _urlbuf[256];
static char _recv_buf[256];
static 	periodic_data _data;
static char _host[128];

typedef int32_t (*TestFunc_t)(void);
static void RunTest(TestFunc_t test,const char * name,int32_t expected_code);


static void SetParams(char * buf, uint32_t bufsize,const char * param, const char * value,uint8_t is_last) {
	char parambuf[256];

	if (is_last) {
		snprintf(parambuf,sizeof(parambuf),"%s=%s",param,value);
	}
	else {
		snprintf(parambuf,sizeof(parambuf),"%s=%s&",param,value);
	}

	strncat(buf,parambuf,bufsize);
}

static void CreateUrl(char * buf, uint32_t bufsize, const char ** params, const char ** values, uint32_t numparams) {
	uint32_t i;
	strncpy(buf,TEST_NETWORK_ENDPOINT,bufsize);
	strcat(buf,"?");

	for (i = 0; i < numparams; i++) {
		SetParams(buf,bufsize,params[i],values[i],i == numparams-1 );
	}

}

static int32_t DecodeTestResponse(uint8_t * buf,uint32_t bufsize, const char * data) {
	SyncResponse reply;
	int32_t ret;
	const char* header_content_len = "Content-Length: ";
	char * content = strstr(data, "\r\n\r\n") + 4;
	char * len_str = strstr(data, header_content_len) + strlen(header_content_len);
	int len = atoi(len_str);

	memset(&reply,0,sizeof(reply));


	if (http_response_ok(data) != 1) {
		DEBUG_PRINTF("http_response_ok != 1");
		return -1;
	}

	if (len_str == NULL) {
		DEBUG_PRINTF("len_str == NULL");
		return -2;
	}


	ret = decode_rx_data_pb((uint8_t *) content, len, SyncResponse_fields, &reply);

	if(ret != 0) {
		DEBUG_PRINTF("decode_rx_data_pb fail with return code %d",ret);
		return -3;
	}


	DEBUG_PRINTF("acc sampling interval %d",reply.acc_sampling_interval);

	return 0;

}

static int32_t DoSyncSendAndResponse(const char * params[],const char * values[],const int32_t len ) {
	char returnbytes[512];

	memset(&_data,0,sizeof(_data));
	memset(_urlbuf,0,sizeof(_urlbuf));

	_data.has_dust = 1;
	_data.dust = 1234;

	CreateUrl(_urlbuf,sizeof(_urlbuf),params,values,len);

	if (NetworkTask_SynchronousSendProtobuf(_host,_urlbuf,_recv_buf,sizeof(_recv_buf),periodic_data_fields,&_data,10) != 0) {
		return ERROR_CODE_FAILED_SEND;
	}

	if (DecodeTestResponse((uint8_t *)returnbytes,sizeof(returnbytes),_recv_buf) != 0) {
		return ERROR_CODE_FAILED_DECODE;
	}

	return 0;
}

static int32_t TestNominal(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(params,values,len);

}

static int32_t TestBadReturnCode(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","400","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(params,values,len);

}

static int32_t TestFiveSecondDelay(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"5000","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(params,values,len);

}

static int32_t TestMinuteDelay(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"60000","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(params,values,len);

}

static int32_t TestMalformed(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","200","1"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(params,values,len);

}



static void RunTest(TestFunc_t test,const char * name,int32_t expected_code) {
	TickType_t t1,dt;
	int32_t ret;
	uint8_t i;
	DEBUG_PRINTF("Running %s... ");

	t1 = xTaskGetTickCount();

	ret = test();

	dt = xTaskGetTickCount() - t1;


	DEBUG_PRINTF("%d.%d seconds elapsed, ",dt/1000,dt % 1000);

	if (ret == expected_code) {
		DEBUG_PRINTF("SUCCESS -- %s -- expecting code %d",name,expected_code);
		led_set_user_color(0,255,0); //green
	}
	else {
		for (i = 0; i < 25; i++) {
			DEBUG_PRINTF("FAIL -- %s --  code %d",name,ret);
			led_set_user_color(255,0,0); //red rum
		}
	}

	vTaskDelay(2000);

	led_set_user_color(0,0,0);


}

void TestNetwork_RunTests(const char * host) {

	strcpy(_host,host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestNominal,ERROR_CODE_SUCCESS);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestBadReturnCode,ERROR_CODE_FAILED_DECODE);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestFiveSecondDelay,ERROR_CODE_SUCCESS);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestMalformed,ERROR_CODE_FAILED_DECODE);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestMinuteDelay,ERROR_CODE_FAILED_SEND);




}
