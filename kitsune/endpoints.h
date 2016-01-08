#ifndef _ENDPOINTS_H_
#define _ENDPOINTS_H_

char * get_server(void);

#define DATA_SERVER	get_server()

#define PROD_DATA_SERVER                    "sense-in.hello.is"
#define DEV_DATA_SERVER						"dev-in.hello.is"
#define DATA_RECEIVE_ENDPOINT               "/in/sense/batch"
#define MORPHEUS_REGISTER_ENDPOINT          "/register/morpheus"
#define PILL_REGISTER_ENDPOINT              "/register/pill"
#define AUDIO_FEATURES_ENDPOINT             "/audio/features"
#define RAW_AUDIO_ENDPOINT                 "/audio/raw"
#define PILL_DATA_RECEIVE_ENDPOINT          "/in/pill"
#define PILL_PROX_DATA_RECEIVE_ENDPOINT		"/in/pill/prox"
#define CHECK_KEY_ENDPOINT                  "/check"
#define PROVISION_ENDPOINT					"/provision/keys"

#define TEST_NETWORK_ENDPOINT               "/"
#define TEST_SERVER                         "stress-in.hello.is"
#endif //_ENDPOINTS_H_
