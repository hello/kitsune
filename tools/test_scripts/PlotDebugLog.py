#!/usr/bin/python
import matrix_pb2
import base64
import sys
import numpy as np
import matplotlib.pyplot as plt

max_num_plot_lines = 50

def GetMatrixFromData(data):
    binarydata = base64.b64decode(data)
    mat = matrix_pb2.Matrix()
    mat.ParseFromString(binarydata) 
    array =  np.reshape(mat.idata,(mat.rows,mat.cols))
    return array



def plotData(key,data,fignum):
    if len(data) == 0:
        return

    arr = np.array(data[0])
    
    #aggregate
    if arr.shape[1] <= max_num_plot_lines:
        for j in range(1,len(data)):
            arr = np.concatenate((arr,data[j]),axis=0)
	x = np.array(range(len(data)))
    else:
        for j in range(1,len(data)):
            arr = np.concatenate((arr,data[j]),axis=1)
        x = np.array(range(arr.shape[1]))
        arr = arr[0,:]

    print 'fig %03d,%s, has %d entries' % (fignum,key,len(x))

    plt.figure(fignum)
    plt.plot(x,arr)
    plt.title(key)
    

###################
mydict = {}
for line in sys.stdin:
    key,data = line.split('\t',1)
    data = data.rstrip()

    if not mydict.has_key(key):
        mydict[key] = []
    try:
        mydict[key].append(GetMatrixFromData(data))
    except Exception:
        print 'exception for key %s' % key
        GetMatrixFromData(data)

fignum = 1
for key in mydict.keys():
    plotData(key,mydict[key],fignum)
    fignum = fignum + 1

plt.show()
