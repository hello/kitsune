#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "TestNetwork.h"
#include "../networktask.h"
#include <string.h>
#include <stdio.h>

#include "../protobuf/periodic.pb.h"
#include "../debugutils/matmessageutils.h"

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

static void DoTest1(void) {
	static const char * params[] = {TIMEOUT,RETURNCODE,MALFORMED};
	static const char * values[] = {"5000","200","true"};
	static const int32_t len = 3;

	memset(&_data,0,sizeof(_data));
	memset(_urlbuf,0,sizeof(_urlbuf));


	CreateUrl(_urlbuf,sizeof(_urlbuf),params,values,len);

	UARTprintf("%s\r\n",_urlbuf);

	NetworkTask_SynchronousSendProtobuf(DATA_SERVER,_urlbuf,_recv_buf,sizeof(_recv_buf),periodic_data_fields,&_data,10);


}


void TestNetwork_RunTests(void) {

	DoTest1();


}
