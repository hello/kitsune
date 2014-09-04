#!/usr/bin/python
import helloaudio
import random

helloaudio.Init()

arr = helloaudio.new_intArray(1024)
feats = helloaudio.new_intArray(8)

for i in range(430):
    if (i  < 43 or  i > 86):
        for j in range(1024):
            helloaudio.intArray_setitem(arr,j, 0)

    else:
        for j in range(1024):
            val = (random.randint(-16384, 16383))
            helloaudio.intArray_setitem(arr,j, val)

    retval = helloaudio.SetAudioData(arr)
    debugbuf = helloaudio.DumpDebugBuffer()
    print debugbuf
    if retval:
        t1 = helloaudio.GetT1()
        t2 = helloaudio.GetT2();
        pyfeats = []
        helloaudio.GetAudioFeatures(feats)
        for j in range(8):
            pyfeats.append(helloaudio.intArray_getitem(feats, j))
                    
        print t1, t2,pyfeats

helloaudio.delete_intArray(arr)
helloaudio.delete_intArray(feats)
