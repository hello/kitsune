#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "wifi_cmd.h"
#include "sl_sync_include_after_simplelink_header.h"
#include <strings.h>
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
    //Interval is intentional
    r = sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt, (unsigned char *)(IntervalVal), sizeof(IntervalVal));

    // delay specific milli seconds to verify scan is started
    vTaskDelay(desc->duration_ms);

	// r indicates the valid number of entries
	// The scan results are occupied in netEntries[]
	r = sl_WlanGetNetworkList(0, desc->max_entries, desc->entries);

	// Restore connection policy to Auto
	sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 0), NULL, 0);

	hlo_future_write(result,NULL,0,r);
}
static int scan_for_wifi(Sl_WlanNetworkEntry_t * result, size_t max_entries, int ant_select, int duration){
	scan_desc_t desc = (scan_desc_t){
		.entries = result,
		.max_entries = max_entries,
		.antenna = ant_select,
		.duration_ms = duration,
	};
	hlo_future_t * fut = hlo_future_create_task_bg(0, scan, &desc, 1024);
	int rv = hlo_future_read(fut, NULL, 0, portMAX_DELAY);
	hlo_future_destroy(fut);
	if(rv >= 0){
		return rv;
	}else{
		return -1;
	}
}
static void worker_scan_unique(hlo_future_t * result, void * ctx){
	int num_entries = result->buf_size / sizeof(Sl_WlanNetworkEntry_t);
	Sl_WlanNetworkEntry_t * entries = (Sl_WlanNetworkEntry_t*)result->buf;
	int ret = get_unique_wifi_list(entries, num_entries);
	hlo_future_write(result,NULL,0,ret);
}
////---------------------------------
//Public
unsigned long resolve_ip_by_host_name(const char * host_name){
	unsigned long ip = 0;
	hlo_future_t * fut = hlo_future_create_task(sizeof(unsigned long), resolve, (void*)host_name);
	int rv = hlo_future_read(fut,&ip,sizeof(ip), portMAX_DELAY);
	hlo_future_destroy(fut);
	if(rv >= 0){
		return ip;
	}else{
		return 0;
	}

}
int _replace_ssid_by_rssi(Sl_WlanNetworkEntry_t * main, size_t main_size, const Sl_WlanNetworkEntry_t* entry){
	int i;
	for(i = 0; i < main_size; i++){
		Sl_WlanNetworkEntry_t * row = &main[i];
		if(row->rssi == 0 && row->ssid[0] == 0){
			//fresh entry, copy over
			*row = *entry;
			return 1;
		}else if(!strncmp((const char*)row->ssid, (const char*)entry->ssid, sizeof(row->ssid))){
			if(entry->rssi > row->rssi){
				*row = *entry;
				return 0;
			}
			return 0;
		}
	}
	return 0;
}
//this implementation is O(n^2)
int get_unique_wifi_list(Sl_WlanNetworkEntry_t * result, size_t num_entries){
	size_t size = num_entries * sizeof(Sl_WlanNetworkEntry_t);
	int retries, ret, tally = 0;
	Sl_WlanNetworkEntry_t * ifa_list = pvPortMalloc(size);
	Sl_WlanNetworkEntry_t * pcb_list = pvPortMalloc(size);
	memset(result, 0, size);
	if(!ifa_list || !pcb_list){
		goto exit;
	}
	//do ifa
	retries = 3;
	while(retries > 0 && (ret = scan_for_wifi(ifa_list, num_entries, IFA_ANT, 3000)) == 0){
		DISP("Retrying IFA %d\r\n", retries);
	}
	while(--ret > 0){
		ifa_list[ret].reserved[0] = IFA_ANT;
		ifa_list[ret].ssid_len = 0;
		tally += _replace_ssid_by_rssi(result, num_entries, &ifa_list[ret]);
	}
	//now do pcb
	retries = 3;
	while(retries > 0 && (ret = scan_for_wifi(pcb_list, num_entries, PCB_ANT, 3000)) == 0){
		DISP("Retrying PCB %d\r\n", retries);
	}
	while(--ret > 0){
		pcb_list[ret].reserved[0] = PCB_ANT;
		pcb_list[ret].ssid_len = 0;
		tally += _replace_ssid_by_rssi(result, num_entries, &pcb_list[ret]);
	}
exit:
	if(ifa_list){vPortFree(ifa_list);}
	if(pcb_list){vPortFree(pcb_list);}
	return tally;

}
hlo_future_t * prescan_wifi(size_t num_entries){
	size_t size = num_entries * sizeof(Sl_WlanNetworkEntry_t);
	return hlo_future_create_task_bg(size, worker_scan_unique, NULL, 1024);
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
//despite using futures, this is not thread safe, but given the speed
//of human input, it's hard to race it.
int Cmd_scan_wifi(int argc, char *argv[]){
	static hlo_future_t * result;
	Sl_WlanNetworkEntry_t entries[10];
	int ret;
	if(!result){
		result = prescan_wifi( 10 );
	}
	if(!result){
		return -1;
	}
	ret = hlo_future_read(result,entries,sizeof(entries), portMAX_DELAY);
	DISP("\r\n");
	if(ret > 0){
		DISP("Found endpoints\r\n===========\r\n");
		int i;
		for(i = 0; i < ret; i++){
			DISP("%d)%s\r\n",i, entries[i].ssid);
		}
	}else{
		DISP("No endpoints scanned\r\n");
	}
	//queue another scan
	hlo_future_destroy(result);
	result = prescan_wifi( 10 );
	return 0;
}
