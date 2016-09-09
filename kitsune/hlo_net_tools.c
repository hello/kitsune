#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "wifi_cmd.h"
#include "sl_sync_include_after_simplelink_header.h"
#include <strings.h>
#include "tinyhttp/http.h"
#include "socket.h"
#include "hw_ver.h"
typedef struct{
	SlWlanNetworkEntry_t * entries;
	size_t max_entries;
	int antenna;
	int duration_ms;
}scan_desc_t;

////---------------------------------
//implementations
int parse_url(url_desc_t * desc, const char * url){
	char * marker = NULL;
	char * term = NULL;
	memset(desc, 0, sizeof(*desc));
	desc->protocol = HTTP;
	if(strstr(url, "https") == url){
		desc->protocol = HTTPS;
	}

	marker = strstr(url, "://");
	if(!marker){
		marker = url;
	}else{
		marker += 3;
	}

	term = strstr(marker, "/");
	if(term){
		char * itr = desc->host;
		while(marker < term){
			*itr++ = *marker++;
		}
	}else{
		strcpy(desc->host, url);
		desc->path[0] = '/';
		return 0;
	}
	marker = term;
	strcpy(desc->path, marker);
	return 0;
}
void itohexstring(size_t n, char output[9]){
	static const char hex[] = "0123456789ABCDEF";
	memset(output, 0, sizeof(output));
	int i;
	for(i = 0; i < 8; i++){
		output[i] = hex[(n >> ((7-i) * 4)) & 0x0F];
	}
}
void antsel(unsigned char a);

static void reselect_antenna(SlWlanNetworkEntry_t * entries, int num_entries ) {
	if( num_entries == 0 ) {
		return;
	}
	int i;
	char ssid[MAX_SSID_LEN] = {0};
	wifi_get_connected_ssid( (uint8_t*)ssid, sizeof(ssid));
    for(i = 0; i < num_entries; i++) {
    	if( 0 == strcmp( (char*)entries[i].Ssid, ssid ) ) {
    		antsel(entries[i].Reserved[0]);
    		save_default_antenna(entries[i].Reserved[0]);
    		LOGI("ssid: %s a:%d\r\n", ssid, entries[i].Reserved[0]);
    		break;
    	}
    }
}

static int scan(scan_desc_t * desc){
	unsigned char policyOpt = SL_WLAN_CONNECTION_POLICY(0, 0, 0, 0);
	unsigned long IntervalVal = 20;
	if( desc->antenna ) {
		antsel(desc->antenna);
	}

	sl_enter_critical_region();

	int r = sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION , policyOpt, NULL, 0);

    // Make sure scan is enabled
    policyOpt = SL_WLAN_SCAN_POLICY(1,1);

    // set scan policy - this starts the scan
    //Interval is intentional
    r = sl_WlanPolicySet(SL_WLAN_POLICY_SCAN , policyOpt, (unsigned char *)(&IntervalVal), sizeof(IntervalVal));

    // delay specific milli seconds to verify scan is started
    vTaskDelay(desc->duration_ms);

	// r indicates the valid number of entries
	// The scan results are occupied in netEntries[]
	r = sl_WlanGetNetworkList(0, desc->max_entries, desc->entries);

	// Restore connection policy to Auto
	sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(1, 0, 0, 0), NULL, 0);

	sl_exit_critical_region();
	//do not need to capture any values since we are storing it directly to network list
	return r;
}
static int scan_for_wifi(SlWlanNetworkEntry_t * result, size_t max_entries, int ant_select, int duration){
	scan_desc_t desc = (scan_desc_t){
		.entries = result,
		.max_entries = max_entries,
		.antenna = ant_select,
		.duration_ms = duration,
	};
	return scan( &desc );
}
static void worker_scan_unique(hlo_future_t * result, void * ctx){
	SlWlanNetworkEntry_t entries[10] = {0};
	hlo_future_write(result, entries, sizeof(entries), get_unique_wifi_list(entries, 10));
}

static void SortByRSSI(SlWlanNetworkEntry_t* netEntries,
                                            unsigned char ucSSIDCount){
    SlWlanNetworkEntry_t tTempNetEntry;
    unsigned char ucCount, ucSwapped;
    do{
        ucSwapped = 0;
        for(ucCount =0; ucCount < ucSSIDCount - 1; ucCount++)
        {
           if(netEntries[ucCount].Rssi < netEntries[ucCount + 1].Rssi)
           {
              tTempNetEntry = netEntries[ucCount];
              netEntries[ucCount] = netEntries[ucCount + 1];
              netEntries[ucCount + 1] = tTempNetEntry;
              ucSwapped = 1;
           }
        } //end for
     }while(ucSwapped);
}
////---------------------------------
//Public
int _replace_ssid_by_rssi(SlWlanNetworkEntry_t * main, size_t main_size, const SlWlanNetworkEntry_t* entry){
	int i;
	for(i = 0; i < main_size; i++){
		SlWlanNetworkEntry_t * row = &main[i];
		if(row->Rssi == 0 && row->Ssid[0] == 0){
			//fresh entry, copy over
			*row = *entry;
			return 1;
		}else if(!strncmp((const char*)row->Ssid, (const char*)entry->Ssid, sizeof(row->Ssid))){
			if(entry->Rssi > row->Rssi){
				*row = *entry;
			}
			return 0;
		}
	}
	return 0;
}

//TODO REMOVE
//this implementation is O(n^2)
int get_unique_wifi_list(SlWlanNetworkEntry_t * result, size_t num_entries){
	size_t size = num_entries * sizeof(SlWlanNetworkEntry_t);
	int retries, ret = 0;
	DISP("Scan begin\n");

	sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(0, 0, 0, 0), NULL, 0);
	sl_WlanDisconnect();
	vTaskDelay(100);

	DISP("Scan PCB\n");
	//now do pcb
	retries = 3;
	while(--retries > 0 && (ret = scan_for_wifi(result, num_entries, PCB_ANT, 5000)) == 0){
		DISP("Retrying PCB %d\r\n", retries);
	}
exit:
	sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(1, 0, 0, 0), NULL, 0);

	DISP("Scan complete\n");
	return ret;

}
hlo_future_t * prescan_wifi(size_t num_entries){
	return hlo_future_create_task_bg(worker_scan_unique, NULL, 2560);
}
////---------------------------------
//Commands

//despite using futures, this is not thread safe, but given the speed
//of human input, it's hard to race it.
int Cmd_scan_wifi(int argc, char *argv[]){
	static hlo_future_t * result;
	SlWlanNetworkEntry_t entries[10] = {0};
	int ret, i;
	if(!result){
		result = prescan_wifi( 10 );
	}
	if(!result){
		return -1;
	}
	ret = hlo_future_read_once(result,entries,sizeof(entries));
	DISP("\r\n");
	if(ret > 0){
		DISP("Found %d endpoints\r\n===========\r\n", ret);
		SortByRSSI(entries, ret);
		for(i = 0; i < ret; i++){
			DISP("%d)%s, %d, %d dB, %d\r\n",i, entries[i].Ssid, entries[i].SecurityInfo, entries[i].Rssi, entries[i].Reserved[0]);
		}
	}else{
		DISP("No endpoints scanned\r\n");
	}
	//queue another scan
	result = prescan_wifi( 10 );
	return 0;
}
