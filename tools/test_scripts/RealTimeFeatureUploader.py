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
import commands
import requests
import requests.packages.urllib3 as urllib3

sys.path.append('.')

import helloaudio
from AudioDataClient import *


k_region = 'us-east-1'
k_server_url = 'https://dev-in.hello.is/audio/features'

k_access_key_id = os.getenv('AWS_ACCESS_KEY_ID')
k_secret_access_key = os.getenv('AWS_SECRET_KEY')
k_auth = {'aws_access_key_id':k_access_key_id,'aws_secret_access_key' : k_secret_access_key }
k_stream = 'audio_features'


CHUNK = 1024 
FORMAT = pyaudio.paInt16 #paInt8
CHANNELS = 1 
RATE = 44100 #sample rate

send_target = 'featAudio'
num_feats = 16



g_kill = False
g_queue = Queue()


def signal_handler(signal, frame):
    global g_kill
    print('You pressed Ctrl+C!')
    g_kill = True
    sys.exit(0)

def getHwAddr(ifname):
    txt = commands.getoutput("ifconfig " + ifname)
    mynum = bytes()
    if 'ether' in txt:
            before, middle, after = txt.partition('ether')
            before, middle, after = after.partition('\n')
            addr = before.strip()
            myhex = addr.split(':')
            print myhex
            for item in myhex:
                myint = int(item, 16)
                mynum += chr(myint)
                

    elif 'HWaddr' in txt:
        print ('LINUX MAC ADDRESS GETTER NEEDS AN IMPLEMENTATION')
        
    return mynum

class AudioDataClient():
    def __init__(self, server_url):
        self.mac = getHwAddr('en0')
        self.server_url = server_url


    def send_matrix_message(self, matrix):
        message = matrix_pb2.MatrixClientMessage()
        message.mac = self.mac
        message.matrix_payload.CopyFrom(matrix)
        message.unix_time = int(time.time())
        payload = message.SerializeToString()
        
        try:
            r = requests.post(self.server_url , data=payload)
            worked = r.status_code == requests.codes.ok
        except urllib3.exceptions.ProtocolError:
            worked = False
            print 'Failed to connect to ', self.server_url
        
        return worked
          
class SendThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.client = AudioDataClient(k_server_url)
        
    def run(self):
        global g_kill
        global g_queue
        
        while (not g_kill):
            
            mat = g_queue.get()

            self.client.send_matrix_message(mat)
            
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
                    duration = mat.time2 - mat.time1
                    print time.strftime("%X"), mat.id, mat.tags,mat.time1,duration

                    g_queue.put(mat)
                   
             
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
   
