#!/usr/bin/python

import numpy as np
import sklearn as s
import sklearn.decomposition
from pylab import *
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt

files = ['talking.dat', 'crying.dat', 'snoring.dat', 'vehicles.dat']

def GetFeats(filename):
    fid = open(filename)
    feats = np.load(fid)
    fid.close()
    return feats

#"labeled" data
ldata = []
for file in files:
    temp = GetFeats(file)
    ldata.append(temp[:, 1:]) #ignore energy
   
feats = GetFeats('feats.dat')
feats = feats[:, 1:] #ignore energy


p = s.decomposition.PCA(whiten=True)
p.fit(feats) #do PCA on all data

tldata = []
for x in ldata:
    temp  = p.transform(x)
    temp = temp[:, 0:3]
    tldata.append(temp)
    
#plot(tldata[0][:,0],tldata[0][:,1],'.', tldata[1][:,0],tldata[1][:,1],'.', tldata[2][:,0],tldata[2][:,1],'.', tldata[3][:,0],tldata[3][:,1],'.')
#show()


fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
ax.scatter(tldata[0][:,0],tldata[0][:,1],  tldata[0][:,2], c='b',marker='.')
ax.scatter(tldata[1][:,0],tldata[1][:,1],  tldata[1][:,2], c='r',marker='o')
ax.scatter(tldata[2][:,0],tldata[2][:,1],  tldata[2][:,2], c='c',marker='x')
ax.scatter(tldata[3][:,0],tldata[3][:,1],  tldata[3][:,2], c='m',marker='+')

plt.show()
