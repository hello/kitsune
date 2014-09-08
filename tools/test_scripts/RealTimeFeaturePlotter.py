#!/usr/bin/python
from pyqtgraph.Qt import QtGui, QtCore
import numpy as np
import numpy.linalg
import pyqtgraph as pg
import pyaudio
import wave
import sys
import threading
import signal
import struct
import matrix_pb2
import base64
from Queue import Queue

import sys
sys.path.append('.')
import helloaudio
import GmmAndPca

np.set_printoptions(precision=3, suppress=True, threshold=numpy.nan)


#def signal_handler(signal, frame):
 #       print('You pressed Ctrl+C!')
 #       g_kill = True
 #       sys.exit(0)

CHUNK = 1024 
FORMAT = pyaudio.paInt16 #paInt8
CHANNELS = 1 
RATE = 44100 #sample rate

plot_target = 'mfcc_avg'
plot_samples = 430*1
num_feats = 16
plot_yrange = (-50000, 300000)
plot_num_signal = num_feats + 1

g_kill = False
g_PlotQueue = Queue()



global g_graphicsitems
global g_p6
global g_segdata
global g_plotdata
global g_curves

g_curves = []
g_plotdata = []
g_segdata = [0 for j in range(plot_samples)]
g_graphicsitems = []
g_p6 = None

class DataBlock():
    def __init__(self, data, mytype):
        self.data_ = data
        self.mytype_ = mytype
        
    

def DeserializeDebugData(base64data):
    binarydata = base64.b64decode(base64data)
    mat = matrix_pb2.Matrix()
    mat.ParseFromString(binarydata) 
    
    return mat
    
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
        
def updatePlot():
    global g_graphicsitems
    global g_p6
    global g_segdata
    global g_plotdata
    global g_curves
    
    try:
        while True:
            block = g_PlotQueue.get(False)
            if block.mytype_ == 'audiofeatures':
                vec = block.data_.idata
                idx = block.data_.time1 % plot_samples
                for j in range(len(vec)):
                    g_plotdata[j][idx]= (vec[j]);
                
                
                #reset plot signals
                if idx == plot_samples - 1:
                    print "HERE"
                    g_plotdata = []
                    g_segdata = [0 for j in range(plot_samples)]

                    #fill in empy buffers
                    for j in range(num_feats):
                        g_plotdata.append([0 for j in range(plot_samples)])

                    g_plotdata.append(g_segdata)

        #remove text
                    for gitem in g_graphicsitems:
                        g_p6.removeItem(gitem)
             
                    g_graphicsitems = []
            
                    Refresh(g_p6, g_curves)
            
                else:
                    #update plot
                    for j in range(plot_num_signal):
                        g_curves[j].setData(g_plotdata[j])
            
            if block.mytype_ == 'segdata':
                s1 = block.data_[0]
                s2 = block.data_[1]
                g_segdata[s1[0]] = s1[1]
                g_segdata[s2[0]] = s2[1]
                segtype = block.data_[2]
                
                text = pg.TextItem(segtype, anchor=(0, 0))
                text.setPos(s2[0], 0)
                g_graphicsitems.append(text)
                g_p6.addItem(text)


    
    except Exception:
        foo = 3

def updateAudio(stream):
    first = True
    dcfeats = None
    
    gmm = GmmAndPca.GmmPcaEvalutator()
    gmm.SetFromJsonFile('gmm_coeffs.json')

 
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
        
        if retval:
            t1 = helloaudio.GetT1() 
            t2 = helloaudio.GetT2() 
            segtype = helloaudio.GetSegmentType()
            helloaudio.GetAudioFeatures(feats)
            
            segfeats = []
            for j in range(0, num_feats):
                segfeats.append(helloaudio.intArray_getitem(feats, j))
            
            segfeats = np.array(segfeats).astype(float)
            
            if first and segtype == 1:
                first = False
                dcfeats = segfeats
                print dcfeats
        
            if dcfeats is not None:
                segfeats = segfeats - dcfeats
            
            normalizedfeats = segfeats / segfeats[0]
            normalizedfeats = normalizedfeats[1:].reshape((1, num_feats-1))
            
           
             
            probs = gmm.evaluate(normalizedfeats)
            
            
            if (segtype == 0):
                segtype = 'packet';
                print probs
                #print normalizedfeats[0].tolist()
            else:
                segtype = 'steady'
                
            
                
            duration = t2 - t1
            
            t2 = t2 % plot_samples
            t1 = t2 - duration
            if t1 < 0:
                t1 = 0
               
            segdata = [(t1,plot_yrange[1]), (t2, plot_yrange[1]), segtype]
            
            block = DataBlock(segdata, 'segdata')
            g_PlotQueue.put(block)
            

        #pull out results
        debugdata = helloaudio.DumpDebugBuffer()
        
        #deserialize results
        datadict = {}
        lines = debugdata.split('\n')
        
        for line in lines:
            dd = DeserializeDebugData(line)
            datadict[dd.id] = dd
         
    
        #add data to plot vectors
        data = datadict[plot_target]
        block = DataBlock(data, 'audiofeatures')
        g_PlotQueue.put(block)


## Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    
    #signal.signal(signal.SIGINT, signal_handler)
    paud = pyaudio.PyAudio()
    app = QtGui.QApplication([])

    plotTimer = QtCore.QTimer()
    plotTimer.timeout.connect(updatePlot)
    plotTimer.start(10)
    


    win = pg.GraphicsWindow(title="Basic plotting examples")
    win.resize(640,480)
    win.setWindowTitle('sound!')
    pg.setConfigOptions(antialias=True)

    g_p6 = win.addPlot(title="mfcc features")
   
    g_curves = CreatePlotCurves(g_p6)

    for j in range(num_feats):
        g_plotdata.append([0 for j in range(plot_samples)])
    g_plotdata.append(g_segdata)


    stream = paud.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)
                

    t = threading.Thread(target=updateAudio, args = (stream,))
    t.start()


    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        #this blocks
        QtGui.QApplication.instance().exec_()
    
    g_kill = True
