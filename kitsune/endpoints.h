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
#define PROD_MESSEJI_SERVER "sense.pims.me"
#define DEV_MESSEJI_SERVER "sense.pims.me"
#else
#define PROD_MESSEJI_SERVER "sense.pims.me"
#define DEV_MESSEJI_SERVER "sense.pims.me"
#endif

#define PROD_SPEECH_SERVER "sense.pims.me"
#define DEV_SPEECH_SERVER "sense.pims.me"

#define SPEECH_ENDPOINT "/speech/v2/upload/audio"
#define SPEECH_KEEPALIVE_ENDPOINT "/speech/v2/ping"

#define MESSEJI_ENDPOINT "/messeji/receive"


#ifdef USE_SHA2
#define PROD_DATA_SERVER                    "sense.pims.me"
#define DEV_DATA_SERVER						"sense.pims.me"
#else
#define PROD_DATA_SERVER                    "sense.pims.me"
#define DEV_DATA_SERVER						"sense.pims.me"
#endif

#define DATA_RECEIVE_ENDPOINT               "/in/sense/batch"
#define MORPHEUS_REGISTER_ENDPOINT          "/register/morpheus"
#define PILL_REGISTER_ENDPOINT              "/register/pill"
#define AUDIO_KEYWORD_FEATURES_ENDPOINT     "/in/audio/keyword_features"
#define RAW_AUDIO_ENDPOINT                 "/in/audio/raw"
#define PILL_DATA_RECEIVE_ENDPOINT          "/in/pill"
#define CHECK_KEY_ENDPOINT                  "/check"
#define PROVISION_ENDPOINT					"/provision/keys"
#define FILE_SYNC_ENDPOINT					"/in/sense/files"
#define SENSE_STATE_ENDPOINT				"/in/sense/state"

#define TEST_NETWORK_ENDPOINT               "/"
#define TEST_SERVER                         "stress-in.hello.is"
#endif //_ENDPOINTS_H_
