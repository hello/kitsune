/* File : example.i */
%module helloaudio
%include "carrays.i"
%array_functions(int,intArray)
%{
/* Put headers and other declarations here */
extern void Init(void);
extern int SetAudioData(int buf[256]);
extern void GetAudioFeatures(int feats[16]);
extern int GetT1(void);
extern int GetT2(void);
extern const char * DumpDebugBuffer();
extern int GetSegmentType(void);

%}

extern void Init(void);
extern int SetAudioData(int buf[256]);
extern void GetAudioFeatures(int feats[16]);
extern int GetT1(void);
extern int GetT2(void);
extern const char * DumpDebugBuffer();
extern int GetSegmentType(void);

