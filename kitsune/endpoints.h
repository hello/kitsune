#ifndef _ENDPOINTS_H_
#define _ENDPOINTS_H_

char * get_server(void);

#define DATA_SERVER	get_server()

#define PROD_DATA_SERVER                    "sense.target.dev"
#define DEV_DATA_SERVER						"dev-in.hello.is"
#define DATA_RECEIVE_ENDPOINT               "/in/sense/batch"
#define MORPHEUS_REGISTER_ENDPOINT          "/register/morpheus"
#define PILL_REGISTER_ENDPOINT              "/register/pill"
#define AUDIO_FEATURES_ENDPOINT             "/audio/features"
#define RAW_AUDIO_ENDPOINT                 "/audio/raw"
#define PILL_DATA_RECEIVE_ENDPOINT          "/in/pill"
#define CHECK_KEY_ENDPOINT                  "/check"
#define PROVISION_ENDPOINT					"/provision/keys"

#define TEST_NETWORK_ENDPOINT               "/"
#define TEST_SERVER                         "stress-in.hello.is"

#define STORE_IP() SL_IPV4_VAL(192,168,82,2)
#endif //_ENDPOINTS_H_
