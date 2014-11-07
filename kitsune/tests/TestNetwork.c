#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "TestNetwork.h"
#include "../networktask.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../protobuf/SyncResponse.pb.h"
#include "../protobuf/periodic.pb.h"
#include "../debugutils/matmessageutils.h"
#include "../wifi_cmd.h"

#include "uartstdio.h"

/*
 *  Makes request to test server with particular test cases
 *
 *
 *
 */

#define TIMEOUT "timeout"
#define RETURNCODE "returncode"
#define MALFORMED "malformed"



static char _urlbuf[256];
static char _recv_buf[256];
static 	periodic_data _data;
static char _host[128];

typedef int32_t (*TestFunc_t)(void);
static void RunTest(TestFunc_t test,const char * name);


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
	int32_t len = 0;
	const char* header_content_len = "Content-Length: ";
	char * content = strstr(data, "\r\n\r\n") + 4;
	char * len_str = strstr(data, header_content_len) + strlen(header_content_len);


	memset(&reply,0,sizeof(reply));


	if (http_response_ok(data) != 1) {
		return -1;
	}

	if (len_str == NULL) {
		return -1;
	}

	len = atoi(len_str);


	if(decode_rx_data_pb((uint8_t *) content, len, SyncResponse_fields, &reply) != 0) {
		return -1;
	}



	return 0;

}

static int32_t DoTest1(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"5000","200","true"};
	static const int32_t len = 3;
	char returnbytes[512];

	memset(&_data,0,sizeof(_data));
	memset(_urlbuf,0,sizeof(_urlbuf));


	CreateUrl(_urlbuf,sizeof(_urlbuf),params,values,len);

	UARTprintf("%s\r\n",_urlbuf);

	if (NetworkTask_SynchronousSendProtobuf(_host,_urlbuf,_recv_buf,sizeof(_recv_buf),periodic_data_fields,&_data,10) != 0) {
		return -1;
	}

	if (!DecodeTestResponse((uint8_t *)returnbytes,sizeof(returnbytes),_recv_buf)) {
		return -2;
	}

	return 0;

}

#define RUN_TEST(test)\
		RunTest(test,#test)

static void RunTest(TestFunc_t test,const char * name) {
	TickType_t t1,dt;
	int32_t ret;

	t1 = xTaskGetTickCount();

	ret = test();

	dt = xTaskGetTickCount() - t1;

	UARTprintf("Running %s... ");

	UARTprintf("%d.%d seconds elapsed, ",dt/1000,dt % 1000);

	if (ret == 0) {
		UARTprintf("success");
	}
	else {
		UARTprintf("FAIL with code %d",ret);
	}

	UARTprintf("\r\n");

}

void TestNetwork_RunTests(const char * host) {

	strcpy(_host,host);

	RUN_TEST(DoTest1);




}
