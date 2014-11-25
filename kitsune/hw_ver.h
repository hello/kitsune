#ifndef HW_VER
#define HW_VER

enum {
	EVT1,
	EVT2,
	DVT,
	PVT,
};

extern int hw_ver;

//WARNING only use these in init, before threads start spawning!
extern void check_hw_version();
extern int get_hw_ver();

#endif
