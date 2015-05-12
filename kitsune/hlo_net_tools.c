#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"

////---------------------------------
//implementations
static void resolve(hlo_future_t * result, void * ctx){
	unsigned long ip = 0;
	int ret = (int)sl_gethostbynameNoneThreadSafe((_i8*)ctx, strlen((char*)ctx), &ip, SL_AF_INET);
	hlo_future_write(result, &ip,sizeof(ip),ret);
}

hlo_future_t * resolve_ip_by_host_name(const char * host_name){
	return hlo_future_create_task(sizeof(unsigned long), resolve, (void*)host_name);

}

int Cmd_dig(int argc, char *argv[]){
	hlo_future_t * fut = resolve_ip_by_host_name(argv[1]);
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
