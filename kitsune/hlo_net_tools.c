#include "hlo_net_tools.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "wifi_cmd.h"
#include "sl_sync_include_after_simplelink_header.h"
#include <strings.h>
#include "tinyhttp/http.h"
#include "socket.h"
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
	hlo_future_write(result, &ip, sizeof(ip), ret);
}
void antsel(unsigned char a);

static void reselect_antenna(Sl_WlanNetworkEntry_t * entries, int num_entries ) {
	if( num_entries == 0 ) {
		return;
	}
	int i;
	char ssid[MAX_SSID_LEN] = {0};
	wifi_get_connected_ssid( (uint8_t*)ssid, sizeof(ssid));
    for(i = 0; i < num_entries; i++) {
    	if( 0 == strcmp( (char*)entries[i].ssid, ssid ) ) {
    		antsel(entries[i].reserved[0]);
    		save_default_antenna(entries[i].reserved[0]);
    		LOGI("ssid: %s a:%d\r\n", ssid, entries[i].reserved[0]);
    		break;
    	}
    }
}
static void* response_realloc(void* opaque, void* ptr, int size)
{
	DISP("realloc %d\r\n", size);
    return pvPortRealloc(ptr, size);
}

static void response_body(void* opaque, const char* data, int size)
{
	DISP("Body %s\r\n", data);
}

static void response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue)
{ /* example doesn't care about headers */ }

static void response_code(void* opaque, int code)
{
	DISP("code: %d\r\n", code);
}
static const struct http_funcs responseFuncs = {
    response_realloc,
    response_body,
    response_header,
    response_code,
};
static int open_sock(unsigned long ip, int * out_sock){
	SlSockAddrIn_t  Addr;
	Addr.sin_family = SL_AF_INET;
	Addr.sin_port = sl_Htons(80);
	Addr.sin_addr.s_addr = sl_Htonl(ip);
	int sock = socket(SL_AF_INET,SL_SOCK_STREAM, 0); //todo secure socket
	if(sock < 0){
		return sock;
	}
	*out_sock = sock;
	return connect(sock, (const SlSockAddr_t*)&Addr,  sizeof(SlSockAddrIn_t));
}
static void get_stream(hlo_future_t * result, void * ctx){
	unsigned long ip = resolve_ip_by_host_name("google.com");
	LOGI("IP: %d.%d.%d.%d \n\r\n\r",SL_IPV4_BYTE(ip, 3), SL_IPV4_BYTE(ip, 2),
									   SL_IPV4_BYTE(ip, 1), SL_IPV4_BYTE(ip, 0));
	const char request[] = "GET / HTTP/1.1\nHost:google.com\nAccept: text/html, application/xhtml+xml, */*\n\n";
	//const char request[] = "GET /robots.txt HTTP/1.0\r\nContent-Length: 0\r\n\r\n";
	int ret, len, sock;
	bool needmore = true;
	char buffer[1024];
	struct http_roundtripper rt;

	if(open_sock(ip, &sock) < 0 ){
		DISP("Socket opened error\r\n");
		goto end;
	}

	len = send(sock, request, strlen(request), 0);
	if (len != strlen(request)) {
		LOGI("Failed to send request %d\n", len);
		ret = -1;
		goto close_sock;
	}

	http_init(&rt, responseFuncs, NULL);
    while (needmore) {
    	int ndata = 0;
    	int retry = 0;
    	const char* data = buffer;
    	do{
    		ndata = recv(sock, buffer, sizeof(buffer), 0);
			if(++retry > 4){
				break;
			}
			if(ndata < 0 ){
				vTaskDelay(200);
			}
		}while(ndata == SL_EAGAIN);

		if (ndata <= 0) {
			DISP("Error receiving data %d\n", ndata);
			ret = ndata;
			goto close_rt;
		}
		while (needmore && ndata) {
			int read;
			needmore = http_data(&rt, data, ndata, &read);
			ndata -= read;
			data += read;
		}
    }
	if (http_iserror(&rt)) {
		DISP("Error parsing data\n");
		ret = -1;
		goto close_rt;
	}
	ret = 0;
close_rt:
	http_free(&rt);
close_sock:
	close(sock);
end:
	hlo_future_write(result,NULL,0,ret);
}
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
	//do not need to capture any values since we are storing it directly to network list
	hlo_future_write(result, NULL, 0, r);
}
static int scan_for_wifi(Sl_WlanNetworkEntry_t * result, size_t max_entries, int ant_select, int duration){
	scan_desc_t desc = (scan_desc_t){
		.entries = result,
		.max_entries = max_entries,
		.antenna = ant_select,
		.duration_ms = duration,
	};
	hlo_future_t * fut = hlo_future_create_task_bg(scan, &desc, 1024);
	int rv = hlo_future_read(fut, NULL, 0, portMAX_DELAY);
	if(rv >= 0){
		hlo_future_destroy(fut);
		return rv;
	}else{
		hlo_future_destroy(fut);
		return -1;
	}
}
static void worker_scan_unique(hlo_future_t * result, void * ctx){
	Sl_WlanNetworkEntry_t entries[10] = {0};
	hlo_future_write(result, entries, sizeof(entries), get_unique_wifi_list(entries, 10));
}

static void SortByRSSI(Sl_WlanNetworkEntry_t* netEntries,
                                            unsigned char ucSSIDCount){
    Sl_WlanNetworkEntry_t tTempNetEntry;
    unsigned char ucCount, ucSwapped;
    do{
        ucSwapped = 0;
        for(ucCount =0; ucCount < ucSSIDCount - 1; ucCount++)
        {
           if(netEntries[ucCount].rssi < netEntries[ucCount + 1].rssi)
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
unsigned long resolve_ip_by_host_name(const char * host_name){
	unsigned long ip = 0;
	if(0 <= hlo_future_read_once(
				hlo_future_create_task_bg(resolve, (void*)host_name, 1024),
				&ip,
				sizeof(ip))){
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

	if(!ifa_list || !pcb_list){
		goto exit;
	}
	//do ifa
	retries = 3;
	while(--retries > 0 && (ret = scan_for_wifi(ifa_list, num_entries, IFA_ANT, 3000)) == 0){
		DISP("Retrying IFA %d\r\n", retries);
	}
	while(--ret > 0){
		ifa_list[ret].reserved[0] = IFA_ANT;
		ifa_list[ret].ssid_len = 0;
		tally += _replace_ssid_by_rssi(result, num_entries, &ifa_list[ret]);
	}
	//now do pcb
	retries = 3;
	while(--retries > 0 && (ret = scan_for_wifi(pcb_list, num_entries, PCB_ANT, 3000)) == 0){
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

	reselect_antenna( result, num_entries );
	return tally;

}
hlo_future_t * prescan_wifi(size_t num_entries){
	return hlo_future_create_task_bg(worker_scan_unique, NULL, 2048);
}

hlo_future_t * http_get_stream(const char * url){
	//parse url

	return hlo_future_create_task_bg(get_stream, NULL, 2048);


}
////---------------------------------
//Commands
int Cmd_test_get(int argc, char * argv[]){
	hlo_future_destroy(http_get_stream("google.com"));
	return 0;
}
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
	Sl_WlanNetworkEntry_t entries[10] = {0};
	int ret, i;
	if(!result){
		result = prescan_wifi( 10 );
	}
	if(!result){
		return -1;
	}
	ret = hlo_future_read(result,entries,sizeof(entries), portMAX_DELAY);
	DISP("\r\n");
	if(ret > 0){
		DISP("Found %d endpoints\r\n===========\r\n", ret);
		SortByRSSI(entries, ret);
		for(i = 0; i < ret; i++){
			DISP("%d)%s, %d, %d dB, %d\r\n",i, entries[i].ssid, entries[i].sec_type, entries[i].rssi, entries[i].reserved[0]);
		}
	}else{
		DISP("No endpoints scanned\r\n");
	}
	//queue another scan
	hlo_future_destroy(result);
	result = prescan_wifi( 10 );
	return 0;
}
