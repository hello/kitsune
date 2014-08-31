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

#def signal_handler(signal, frame):
 #       print('You pressed Ctrl+C!')
 #       g_kill = True
 #       sys.exit(0)

CHUNK = 1024 
FORMAT = pyaudio.paInt16 #paInt8
CHANNELS = 1 
RATE = 44100 #sample rate

g_kill = False

def update(curve, p6, stream):

    p6.setRange(xRange=(0, 1023), yRange=(-32768, 32767))
    #p6.enableAutoRange('xy', False)  ## stop auto-scaling after the first data set is plotted
    while(not g_kill):
        data = stream.read(CHUNK)
        idata = []
        for i in range(len(data)/2):
            idx = 2*i
            a = struct.unpack('h', data[idx:idx+2])
            idata.append(a[0])
        curve.setData(idata)


## Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    
    #signal.signal(signal.SIGINT, signal_handler)

    paud = pyaudio.PyAudio()
    app = QtGui.QApplication([])

    win = pg.GraphicsWindow(title="Basic plotting examples")
    win.resize(640,480)
    win.setWindowTitle('sound!')
    pg.setConfigOptions(antialias=True)

    p6 = win.addPlot(title="another title")
    curve = p6.plot(pen='y')
    

    stream = paud.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)
                

    t = threading.Thread(target=update, args = (curve,p6, stream))
    t.start()


    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
