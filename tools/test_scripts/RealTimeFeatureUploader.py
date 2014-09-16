#!/usr/bin/python
import pyaudio
import threading
import signal
import struct
import base64
from Queue import Queue
from boto import kinesis
import matrix_pb2
import os
import sys
sys.path.append('.')

import helloaudio
from AudioDataClient import *


k_region = 'us-east-1'

k_access_key_id = os.getenv('AWS_ACCESS_KEY_ID')
k_secret_access_key = os.getenv('AWS_SECRET_KEY')
k_auth = {'aws_access_key_id':k_access_key_id,'aws_secret_access_key' : k_secret_access_key }
k_stream = 'audio_features'


def signal_handler(signal, frame):
    global g_kill
    print('You pressed Ctrl+C!')
    g_kill = True
    sys.exit(0)

CHUNK = 1024 
FORMAT = pyaudio.paInt16 #paInt8
CHANNELS = 1 
RATE = 44100 #sample rate

send_target = 'featAudio'
num_feats = 16

g_kill = False
g_queue = Queue()

class SendThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.conn = kinesis.connect_to_region(k_region, **k_auth)
        
    def run(self):
        global g_kill
        global g_queue
        
        while (not g_kill):
            
            bindata = g_queue.get()
            response = self.conn.put_record(k_stream,bindata, 'blahblahpartitionkey' )
            print response
            g_queue.task_done()

def updateAudio(stream):
    global g_kill
    global g_queue

    first = True    
    helloaudio.Init()

    arr = helloaudio.new_intArray(CHUNK)
    feats = helloaudio.new_intArray(num_feats)

    while(not g_kill):
        try:
            data = stream.read(CHUNK)
            idata = []
            
            #extract integers from binary audio chunk
            for i in range(len(data)/2):
                idx = 2*i
                a = struct.unpack('h', data[idx:idx+2])
                idata.append(a[0])
             
            #pass of data to audio alg via SWIG interface
            for j in range(len(idata)):
                helloaudio.intArray_setitem(arr,j, idata[j])
    
            helloaudio.SetAudioData(arr)
            
            #pull out results
            debugdata = helloaudio.DumpDebugBuffer()
            
            #deserialize results
            datadict = {}
            lines = debugdata.split('\n')
            
            for line in lines:
                bindata = base64.b64decode(line) 
                mat = matrix_pb2.Matrix()
                mat.ParseFromString(bindata)
                
                if mat.id == send_target:
                    print mat.id, line

                    g_queue.put(line)
                   
             
        except IOError:
            print "IO Error"


## Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    
    signal.signal(signal.SIGINT, signal_handler)
    
    paud = pyaudio.PyAudio()
   
    stream = paud.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)
                
    sender = SendThread()
    sender.setDaemon(True)
    sender.start()

    t = threading.Thread(target=updateAudio, args = (stream,))
    #t.setDaemon(True)
    t.start()

    while(True):
        time.sleep(60)
   
