#!/usr/bin/python
from pyqtgraph.Qt import QtGui, QtCore
import numpy as np
import pyqtgraph as pg
import pyaudio
import wave
import sys
import threading
import signal
import struct
import helloaudio
import matrix_pb2
import base64


#def signal_handler(signal, frame):
 #       print('You pressed Ctrl+C!')
 #       g_kill = True
 #       sys.exit(0)

CHUNK = 1024 
FORMAT = pyaudio.paInt16 #paInt8
CHANNELS = 1 
RATE = 44100 #sample rate

plot_target = 'mfcc_avg'
plot_samples = 430
num_feats = 8
plot_yrange = (-50000, 300000)
plot_num_signal = num_feats + 1

g_kill = False

def DeserializeDebugData(base64data):
    binarydata = base64.b64decode(base64data)
    mat = matrix_pb2.Matrix()
    mat.ParseFromString(binarydata) 
    
    return (mat.id,mat.idata)

def CreatePlotCurves(p6):
     p6.setRange(xRange=(0, plot_samples-1), yRange=plot_yrange)

     curves = []
     for i in range(plot_num_signal):
        curves.append(p6.plot(pen=(i, plot_num_signal)))
        
     return curves
     
def Refresh(p6, curves):
    for c in curves:
        p6.removeItem(c)
        p6.addItem(c)
        
def update(p6, stream):
    plotdata = []
    segdata = [0 for j in range(plot_samples)]
    
    graphicsitems = []

    for j in range(num_feats):
        plotdata.append([])
    plotdata.append(segdata)

    curves = CreatePlotCurves(p6)

    helloaudio.Init()

    arr = helloaudio.new_intArray(CHUNK)
    feats = helloaudio.new_intArray(num_feats)

    #p6.enableAutoRange('xy', False)  ## stop auto-scaling after the first data set is plotted
    while(not g_kill):
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

        retval = helloaudio.SetAudioData(arr)
        t1 = 0
        t2 = 0
        if retval:
            t1 = helloaudio.GetT1() 
            t2 = helloaudio.GetT2() 
            duration = t2 - t1
            
            t2 = t2 % plot_samples
            t1 = t2 - duration
            if t1 < 0:
                t1 = 0
                
            segdata[t1] = plot_yrange[1]
            segdata[t2] = plot_yrange[1]
            
            text = pg.TextItem('hello', anchor=(0, 0))
            p6.addItem(text)
            graphicsitems.append(text)
            text.setPos(t2, 0)


            helloaudio.GetAudioFeatures(feats)

        #pull out results
        debugdata = helloaudio.DumpDebugBuffer()
        
        #deserialize results
        datadict = {}
        lines = debugdata.split('\n')
        
        for line in lines:
            dd = DeserializeDebugData(line)
            datadict[dd[0]] = dd[1]
         
    
        #add data to plot vectors
        vec = datadict[plot_target]
        for j in range(len(vec)):
            plotdata[j].append(vec[j]);
         
        #reset plot signals
        if len(plotdata[0]) > plot_samples:
            plotdata = []
            segdata = [0 for j in range(plot_samples)]

            for j in range(num_feats):
                plotdata.append([])
            plotdata.append(segdata)

            #remove text
            for gitem in graphicsitems:
                p6.removeItem(gitem)
             
            graphicsitems = []
            
            Refresh(p6, curves)
            
        else:
            #update plot
            for j in range(plot_num_signal):
                curves[j].setData(plotdata[j])


## Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    
    #signal.signal(signal.SIGINT, signal_handler)
    paud = pyaudio.PyAudio()
    app = QtGui.QApplication([])

    win = pg.GraphicsWindow(title="Basic plotting examples")
    win.resize(640,480)
    win.setWindowTitle('sound!')
    pg.setConfigOptions(antialias=True)

    p6 = win.addPlot(title="mfcc features")
   
    

    stream = paud.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)
                

    t = threading.Thread(target=update, args = (p6, stream))
    t.start()


    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
