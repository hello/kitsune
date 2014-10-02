#!/usr/bin/python
import numpy as np
import numpy.linalg
import sklearn as s
from sklearn import mixture
from sklearn.metrics import confusion_matrix
import MyPca
import MyGmm
import json
from pylab import *
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt

np.set_printoptions(precision=3, suppress=True, threshold=numpy.nan, linewidth=10000)

#files = ['talking.dat','crying.dat','snoring.dat', 'vehicles.dat']
#files = ['snoring.dat','talking.dat','steady.dat']
files = ['snoring.dat','talking.dat']

ndims = 3
ngaussians = 1
#cov_type = 'full'
cov_type = 'diag'
def GetConfusionMatrixFromGmmEnsemble(tldata, ens):
    threshold = 0.6
    min_maximumloglik = -50
    idx = 0;
    labels = []
    predictions  = []
    nullclass = len(tldata)
    for temp in tldata:
        probs = ens.evaluate(temp, min_maximumloglik)
   
        for i in range(probs.shape[0]):
            p = probs[i, :]
            myclass = np.where(p > threshold)[0]
            if myclass.size > 0:
                myclass = myclass[0]
            else:
                myclass = nullclass
            
            predictions.append(myclass)
            labels.append(idx)
    
        idx = idx + 1
    
   
    cm = confusion_matrix(labels, predictions).astype(float)
    rowcounts = np.sum(cm, axis=1) 
    for i in range(cm.shape[1]):
        cm[i, :] = cm[i, :] / rowcounts[i]

    print files
    print cm

###########
# Get PCA
###########
#get labeled data
ldata = []
first = True
for file in files:
    temp2 = MyPca.GetFeats(file).astype(float)
    
    for i in range(temp2.shape[0]):
        temp2[i,:] = temp2[i,:] / np.linalg.norm(temp2[i,:])

    if first:
        first = False        
        udata = temp2

    else:
        udata = np.concatenate((udata, temp2), axis=0)
    
    ldata.append(temp2) #ignore energy
    

pca = MyPca.MyPca()
pca.fit(udata, ndims) #do PCA on all data
print "energy fraction in each dimension:", pca.explainedvariance_
tldata = []
for x in ldata:
    temp  = pca.transform(x)
    tldata.append(temp)
    

if ndims == 2:
    #plot(tldata[0][:,0],tldata[0][:,1],'.', tldata[1][:,0],tldata[1][:,1],'.', tldata[2][:,0],tldata[2][:,1],'.', tldata[3][:,0],tldata[3][:,1],'.')
    plot(tldata[0][:,0],tldata[0][:,1],'.', tldata[1][:,0],tldata[1][:,1],'.')
    show()    

if ndims == 3:
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    ax.scatter(tldata[0][:,0],tldata[0][:,1],  tldata[0][:,2], c='b',marker='.')
    ax.scatter(tldata[1][:,0],tldata[1][:,1],  tldata[1][:,2], c='r',marker='o')
#    ax.scatter(tldata[2][:,0],tldata[2][:,1],  tldata[2][:,2], c='c',marker='x')
#    ax.scatter(tldata[3][:,0],tldata[3][:,1],  tldata[3][:,2], c='m',marker='+')
    plt.show()
    
##########
#Fit the GMM
###########
ens = MyGmm.MyGmmEnsemble()
for x in tldata:
    g = mixture.GMM(n_components=ngaussians, n_iter=1000, covariance_type=cov_type)
    g.fit(x)

    g2 = MyGmm.MyGmm()
    g2.setFromSklearnGmm(g)
    ens.addGmm(g2)



GetConfusionMatrixFromGmmEnsemble(tldata, ens)


myclassifer = {}
myclassifer['pca'] = pca.toDict()
myclassifer['gmmsensemble'] = ens.toDict()

f = open('gmm_coeffs.json', 'w')
json.dump(myclassifer, f)
f.close()

    

    
    
