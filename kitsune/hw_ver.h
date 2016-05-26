#ifndef HW_VER
#define HW_VER

enum {
	DVT,
	PVT,
	EVT1_1p5,
};

extern int hw_ver;

//WARNING only use these in init, before threads start spawning!
extern void check_hw_version();
extern int get_hw_ver();
extern int Cmd_hwver(int argc, char *argv[]);

#endif
