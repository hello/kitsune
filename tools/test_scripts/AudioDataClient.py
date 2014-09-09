#!/usr/bin/python
import requests
import commands
import time
import copy
import requests.packages.urllib3 as urllib3
import sys
sys.path.append('.')

import matrix_pb2


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


    def sendMatrixMessage(self, matrix):
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
        
if __name__ == '__main__':
    foo = 3
