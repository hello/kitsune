#ifndef _ENDPOINTS_H_
#define _ENDPOINTS_H_

char * get_server(void);
char * get_messeji_server(void);

char * get_ws_server(void);
char * get_speech_server(void);

#define DEV_WS_SERVER "dev-ws.hello.is"
#define PROD_WS_SERVER "ws.hello.is"

#define DATA_SERVER	get_server()
#define MESSEJI_SERVER	get_messeji_server()

#ifdef USE_SHA2
#define PROD_MESSEJI_SERVER "messeji-sha2.hello.is"
#define DEV_MESSEJI_SERVER "messeji-dev-sha2.hello.is"
#else
#define PROD_MESSEJI_SERVER "messeji.hello.is"
#define DEV_MESSEJI_SERVER "messeji-dev.hello.is"
#endif

#define PROD_SPEECH_SERVER "speech.hello.is"
#define DEV_SPEECH_SERVER "dev-speech.hello.is"

#define SPEECH_ENDPOINT "/v2/upload/audio"
#define SPEECH_KEEPALIVE_ENDPOINT "/v2/ping"

#define MESSEJI_ENDPOINT "/receive"


#ifdef USE_SHA2
#define PROD_DATA_SERVER                    "sense-in-sha2.hello.is"
#define DEV_DATA_SERVER						"dev-in-sha2.hello.is"
#else
#define PROD_DATA_SERVER                    "sense-in.hello.is"
#define DEV_DATA_SERVER						"dev-in.hello.is"
#endif

#define DATA_RECEIVE_ENDPOINT               "/in/sense/batch"
#define MORPHEUS_REGISTER_ENDPOINT          "/register/morpheus"
#define PILL_REGISTER_ENDPOINT              "/register/pill"
#define AUDIO_KEYWORD_FEATURES_ENDPOINT     "/audio/keyword_features"
#define RAW_AUDIO_ENDPOINT                 "/audio/raw"
#define PILL_DATA_RECEIVE_ENDPOINT          "/in/pill"
#define CHECK_KEY_ENDPOINT                  "/check"
#define PROVISION_ENDPOINT					"/provision/keys"
#define FILE_SYNC_ENDPOINT					"/in/sense/files"
#define SENSE_STATE_ENDPOINT				"/in/sense/state"

#define TEST_NETWORK_ENDPOINT               "/"
#define TEST_SERVER                         "stress-in.hello.is"
#endif //_ENDPOINTS_H_
