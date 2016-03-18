#ifndef _ENDPOINTS_H_
#define _ENDPOINTS_H_

char * get_server(void);
char * get_messeji_server(void);

#define DATA_SERVER	get_server()
#define MESSEJI_SERVER	get_messeji_server()

#define PROD_MESSEJI_SERVER "messeji.hello.is"
#define DEV_MESSEJI_SERVER "messeji-dev.hello.is"
#define MESSEJI_ENDPOINT "/receive"


#define PROD_DATA_SERVER                    "sense-in.hello.is"
#define DEV_DATA_SERVER						"dev-in.hello.is"
#define DATA_RECEIVE_ENDPOINT               "/in/sense/batch"
#define MORPHEUS_REGISTER_ENDPOINT          "/register/morpheus"
#define PILL_REGISTER_ENDPOINT              "/register/pill"
#define AUDIO_FEATURES_ENDPOINT             "/audio/features"
#define RAW_AUDIO_ENDPOINT                 "/audio/raw"
#define PILL_DATA_RECEIVE_ENDPOINT          "/in/pill"
#define CHECK_KEY_ENDPOINT                  "/check"
#define PROVISION_ENDPOINT					"/provision/keys"
#define FILE_SYNC_ENDPOINT					"/in/sense/files"
#define SENSE_STATE_ENDPOINT				"/in/sense/state"

#define TEST_NETWORK_ENDPOINT               "/"
#define TEST_SERVER                         "stress-in.hello.is"
#endif //_ENDPOINTS_H_
