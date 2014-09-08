#!/usr/bin/python

import numpy as np
from pylab import *
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
from MyPca import *
files = ['talking.dat', 'crying.dat', 'snoring.dat', 'vehicles.dat']

np.set_printoptions(precision=3, suppress=True, threshold=numpy.nan)

ndim= 2

#"labeled" data
ldata = []
idx = 0
for file in files:
    temp = GetFeats(file).astype(float)
    data = temp[:,1:]
    for i in range(temp.shape[0]):
        data[i,:] = data[i,:] / temp[i,0]

    ldata.append(data) #ignore energy
    idx = idx + 1
   
first = True
for d in ldata:
    if first:
        feats = d
        first = False;
    else:
        feats = np.concatenate((feats, d))

p = MyPca()
p.fit(feats, ndim) #do PCA on all data


tldata = []
for x in ldata:
    temp  = p.transform(x)
    tldata.append(temp)
    
if ndim == 2:
    plot(tldata[0][:,0],tldata[0][:,1],'.', tldata[1][:,0],tldata[1][:,1],'.', tldata[2][:,0],tldata[2][:,1],'.', tldata[3][:,0],tldata[3][:,1],'.')
    show()


if ndim == 3:
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    ax.scatter(tldata[0][:,0],tldata[0][:,1],  tldata[0][:,2], c='b',marker='.')
    ax.scatter(tldata[1][:,0],tldata[1][:,1],  tldata[1][:,2], c='r',marker='o')
    ax.scatter(tldata[2][:,0],tldata[2][:,1],  tldata[2][:,2], c='c',marker='x')
    ax.scatter(tldata[3][:,0],tldata[3][:,1],  tldata[3][:,2], c='m',marker='+')
    plt.show()
