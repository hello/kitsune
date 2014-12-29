
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

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
#include "ustdlib.h"
#include <stdint.h>
#include "kit_assert.h"

#if COMPILE_TESTS

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

#define RUN_TEST(test,code,host)\
		RunTest(test,STRINGIFY(test),code,host)




#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  LOGI("%s :: ",__func__); LOGI(__VA_ARGS__); LOGI("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

#define TIMEOUT "timeout"
#define RETURNCODE "returncode"
#define MALFORMED "malformed"

static 	periodic_data _data;
static xSemaphoreHandle _waiter;

typedef int32_t (*TestFunc_t)(char *);
static void RunTest(TestFunc_t test,const char * name,int32_t expected_code, char * _host);


static void SetParams(char * buf, uint32_t bufsize,const char * param, const char * value,uint8_t is_last) {
	char parambuf[256];

	if (is_last) {
		usnprintf(parambuf,sizeof(parambuf),"%s=%s",param,value);
	}
	else {
		usnprintf(parambuf,sizeof(parambuf),"%s=%s&",param,value);
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

static int32_t DoSyncSendAndResponse(char * _host, const char * params[],const char * values[],const int32_t len ) {
	char *returnbytes = pvPortMalloc(512);
	char *_recv_buf = pvPortMalloc(256);
	char *_urlbuf = pvPortMalloc(256);

	int  r=0;

	assert(returnbytes&&_recv_buf&&_urlbuf);

	memset(&_data,0,sizeof(_data));
	memset(_urlbuf,0,256);

	_data.has_dust = 1;
	_data.dust = 1234;

	CreateUrl(_urlbuf,sizeof(_urlbuf),params,values,len);

	if (NetworkTask_SynchronousSendProtobuf(_host,_urlbuf,_recv_buf,sizeof(_recv_buf),periodic_data_fields,&_data,10) != 0) {
		r = ERROR_CODE_FAILED_SEND;
	}

	if (DecodeTestResponse((uint8_t *)returnbytes,sizeof(returnbytes),_recv_buf) != 0) {
		r = ERROR_CODE_FAILED_DECODE;
	}

	vPortFree(returnbytes);
	vPortFree(_recv_buf);
	vPortFree(_urlbuf);
	return r;
}

static uint32_t EncodePb(pb_ostream_t * stream, void * data) {
	network_encode_data_t * encodedata = (network_encode_data_t *)data;
	uint32_t ret = false;

	if (encodedata && encodedata->encodedata) {
	ret = pb_encode(stream,encodedata->fields,encodedata->encodedata);
	}



	return ret;
}

static void NetworkResponseCallback(const NetworkResponse_t * response,void * context) {
	int32_t * pret = context;
//	DEBUG_PRINTF("NetTask::SynchronousSendNetworkResponseCallback -- got callback");

	*pret = !response->success;

	xSemaphoreGive(_waiter);


}

static void MessWithWifiAfterDelay(void * data) {
	uint16_t i;

	for (i = 0; i < 200; i++) {
		vTaskDelay(20);
		sl_WlanDisconnect();
	}


}

bool write_a_big_goddamned_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	char buf[256];
	uint32_t i;
	const int numbufs = 1024;

	memset(buf,66,sizeof(buf));


    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }


    if (!pb_encode_varint(stream, (uint64_t)numbufs*sizeof(buf) + 1)) {
          return false;
    }

    for (i = 0; i < numbufs; i++) {
    	if (!pb_write(stream, (const uint8_t *)buf, sizeof(buf))) {
    		return false;
    	}
    }

    buf[0] = '\0';
    if (!pb_write(stream, (const uint8_t *)buf, 1)) {
    	return false;
    }


    return true;

}

static int32_t DoAsyncSendAndResponse(char * _host, const char * params[],const char * values[],const int32_t len, network_prep_callback_t prepfunc) {
	int32_t ret = -999;
	char *returnbytes = pvPortMalloc(512);
	char *_urlbuf = pvPortMalloc(256);

	assert(returnbytes&&_urlbuf);

	NetworkTaskServerSendMessage_t m;
	memset(&_data,0,sizeof(_data));
	memset(_urlbuf,0,256);
	network_encode_data_t encodedata;

	_data.has_dust = 1;
	_data.dust = 1234;
	_data.name.funcs.encode = write_a_big_goddamned_string;

	CreateUrl(_urlbuf,sizeof(_urlbuf),params,values,len);

	encodedata.fields = periodic_data_fields;
	encodedata.encodedata = &_data;

	memset(&m,0,sizeof(m));

	m.encode = EncodePb;
	m.encodedata = &encodedata;
	m.decode_buf = (uint8_t *)returnbytes;
	m.decode_buf_size = sizeof(returnbytes);

	m.endpoint = _urlbuf;
	m.host = _host;
	m.response_callback = NetworkResponseCallback;
	m.prepare = prepfunc;
	m.context = &ret;


	NetworkTask_AddMessageToQueue(&m);

	xSemaphoreTake(_waiter,portMAX_DELAY);

	vPortFree(returnbytes);
	vPortFree(_urlbuf);
	return ret;

}

static int32_t TestNominal(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(_host, params,values,len);

}

static int32_t TestBadReturnCode(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","400","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(_host, params,values,len);

}

static int32_t TestFiveSecondDelay(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"5000","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(_host, params,values,len);

}

static int32_t TestMinuteDelay(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"60000","200","0"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(_host, params,values,len);

}

static int32_t TestMalformed(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","200","1"};
	static const int32_t len = 3;

	return DoSyncSendAndResponse(_host, params,values,len);

}

static int32_t TestNoWifi(char * _host) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"0","200","0"};
	static const int32_t len = 3;
	int32_t val;
	signed char name[64];
	short namelen =64;
	SlSecParams_t secparams;
	unsigned char mac[6];
	SlGetSecParamsExt_t getextsecparams;
	SlSecParamsExt_t extparams = {0};
	unsigned long int priority = 0;

	//assumed there's only one profile -- not necessarily a great assumption here
	sl_WlanProfileGet(0,name,&namelen,mac,&secparams,&getextsecparams,&priority);

	val = DoAsyncSendAndResponse(_host, params,values,len,MessWithWifiAfterDelay);

	sl_WlanConnect(name,namelen,mac,&secparams,&extparams);

	return val;

}


static void RunTest(TestFunc_t test,const char * name,int32_t expected_code, char * _host) {
	TickType_t t1,dt;
	int32_t ret;
	uint8_t i;
	DEBUG_PRINTF("Running %s... ");

	t1 = xTaskGetTickCount();

	ret = test(_host);

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
	char * _host = pvPortMalloc(128);

	assert(_host);
	if (!_waiter) {
		_waiter = xSemaphoreCreateBinary();
	}

	strcpy(_host,host);

	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestNominal,ERROR_CODE_SUCCESS,_host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestBadReturnCode,ERROR_CODE_FAILED_DECODE,_host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestFiveSecondDelay,ERROR_CODE_SUCCESS,_host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestMalformed,ERROR_CODE_FAILED_DECODE,_host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestMinuteDelay,ERROR_CODE_FAILED_SEND,_host);
	DEBUG_PRINTF("----------------------------------------------------------");
	RUN_TEST(TestNoWifi,ERROR_CODE_FAILED_SEND,_host);

	vPortFree(_host);
}
#endif
