#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"
typedef enum{
	RESOLVE_HOST
}net_tools_task_type;

typedef struct{
	char host[64];
	hlo_future_t * notify;
}resolve_context_t;

typedef struct{
	net_tools_task_type type;
	union{
		resolve_context_t resolve;
	};
}net_tools_task_t;

static struct{
	xQueueHandle _queue;
}self;

static long try_resolve_host(char * host_name, unsigned long * ipaddr){
	return sl_gethostbynameNoneThreadSafe((_i8*)host_name, strlen(host_name), ipaddr, SL_AF_INET);

}
void hlo_net_tools_task(void * ctx){
	self._queue = xQueueCreate(8,sizeof( net_tools_task_t ) );
	assert(self._queue);
	net_tools_task_t task;
	while(1){
		   xQueueReceive( self._queue,(void *) &task, portMAX_DELAY );
		   switch(task.type){
		   case RESOLVE_HOST:
		   {
			   unsigned long ip = 0;
			   int ret = (int)try_resolve_host(task.resolve.host,&ip);
			   hlo_future_write(task.resolve.notify, &ip,sizeof(ip),ret);
		   }
		   break;
		   default:
			   break;
		   }
	}
}
hlo_future_t * resolve_host_by_name(const char * host_name){
	hlo_future_t * ret = NULL;
	net_tools_task_t t = (net_tools_task_t){
		.type = RESOLVE_HOST,
	};
	if(self._queue){
		ret = hlo_future_create(sizeof(unsigned long));
		if(ret){
			t.resolve.notify = ret;
			strncpy(t.resolve.host,host_name,sizeof(t.resolve.host)-1);
			xQueueSend(self._queue,&t,0);
		}
	}
	return ret;
}

int Cmd_dig(int argc, char *argv[]){
	hlo_future_t * fut = resolve_host_by_name(argv[1]);
	unsigned long ip;
	int rv = hlo_future_read(fut,&ip,sizeof(ip));
	if(rv >= 0){
		LOGI("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
				argv[1], SL_IPV4_BYTE(ip, 3), SL_IPV4_BYTE(ip, 2),
							   SL_IPV4_BYTE(ip, 1), SL_IPV4_BYTE(ip, 0));
	}else{
		LOGI("failed to resolve ntp addr rv %d\n", rv);
	}
	return 0;
}
