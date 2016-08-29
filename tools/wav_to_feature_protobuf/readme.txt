Feature extraction binary without equalization.  Equalization
period takes 10 seconds at 16Khz if you enable it (comment out 
NO_EQUALIZATION in the CMakeLists.txt in this directory)  

Also included is a plotting script in python, which will
also show you how to extract features from the protobuf output.

How to use
------------------------
usage: ./wavrunner My16khzMonoAudio.wav
output: My16khzMonoAudio.wav.proto

plotting? ./plotFeats.py

make sure to edit plotFeats to choose the filename.


Description of protobuf output
-------------------------
* data comes in chunks of 32 items, alternating MFCCs first, then energy
* each pair of chunks (MFCC + energy) of 32 is 1 second
* features are length 16 MFCC vectors, excluding dc
* feature are quantized to 4 bits (ie. range is -8 to 7), and normalized
* "energy" is the log2 if the total frame energy (i.e. dc) in fixed point Q10
* to recover the MFCC w/magnitude, you might do something like
mfcc[i] * (energy/1024.0) / 8.0
