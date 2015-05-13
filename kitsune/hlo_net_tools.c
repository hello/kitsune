#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"
typedef struct{
	Sl_WlanNetworkEntry_t * entries;
	size_t max_entries;
	int antenna;
	int duration_ms;
}scan_desc_t;
////---------------------------------
//implementations
static void resolve(hlo_future_t * result, void * ctx){
	unsigned long ip = 0;
	int ret = (int)sl_gethostbynameNoneThreadSafe((_i8*)ctx, strlen((char*)ctx), &ip, SL_AF_INET);
	hlo_future_write(result, &ip,sizeof(ip),ret);
}
void antsel(unsigned char a);
static void scan(hlo_future_t * result, void * ctx){
	scan_desc_t * desc = (scan_desc_t*)ctx;
	unsigned char policyOpt = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
	unsigned long IntervalVal = 20;
	if( desc->antenna ) {
		antsel(desc->antenna);
	}
	int r = sl_WlanPolicySet(SL_POLICY_CONNECTION , policyOpt, NULL, 0);

    // Make sure scan is enabled
    policyOpt = SL_SCAN_POLICY(1);

    // set scan policy - this starts the scan
    r = sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt, (unsigned char *)(&IntervalVal), sizeof(IntervalVal));

    // delay specific milli seconds to verify scan is started
    vTaskDelay(desc->duration_ms);

	// r indicates the valid number of entries
	// The scan results are occupied in netEntries[]
	r = sl_WlanGetNetworkList(0, desc->max_entries, desc->entries);

	// Restore connection policy to Auto
	sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 0), NULL, 0);

	hlo_future_write(result,NULL,0,r);
}
////---------------------------------
//Public
unsigned long resolve_ip_by_host_name(const char * host_name){
	unsigned long ip = 0;
	hlo_future_t * fut = hlo_future_create_task(sizeof(unsigned long), resolve, (void*)host_name);
	int rv = hlo_future_read(fut,&ip,sizeof(ip));
	if(rv >= 0){
		return ip;
	}else{
		return 0;
	}

}
int scan_for_wifi(Sl_WlanNetworkEntry_t * result, size_t max_entries, int ant_select){
	scan_desc_t desc = (scan_desc_t){
		.entries = result,
		.max_entries = max_entries,
		.antenna = ant_select,
		.duration_ms = 6000,//fixed scanning duration for now
	};
	hlo_future_t * fut = hlo_future_create_task(0, scan, &desc);
	int rv = hlo_future_read(fut, NULL, 0);
	if(rv >= 0){
		return rv;
	}else{
		return -1;
	}
}
////---------------------------------
//Commands
int Cmd_dig(int argc, char *argv[]){
	if(argc > 1){
		unsigned long ip = resolve_ip_by_host_name(argv[1]);
		if(ip){
			LOGI("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
					argv[1], SL_IPV4_BYTE(ip, 3), SL_IPV4_BYTE(ip, 2),
								   SL_IPV4_BYTE(ip, 1), SL_IPV4_BYTE(ip, 0));
		}else{
			LOGI("failed to resolve ntp addr\r\n");
		}
	}

	return 0;
}
int Cmd_scan_wifi(int argc, char *argv[]){
	size_t size = 10 * sizeof(Sl_WlanNetworkEntry_t);
	Sl_WlanNetworkEntry_t * entries = pvPortMalloc(size);
	memset(entries,0, size);
	int ret = scan_for_wifi(entries, 10, 1);
	if(ret > 0){
		DISP("Found endpoints\r\n===========\r\n");
		int i;
		for(i = 0; i < ret; i++){
			DISP("%d)%s\r\n",i, entries->ssid);
		}
	}else{
		DISP("No endpoints scanned\r\n");
	}
	vPortFree(entries);
	return 0;
}
