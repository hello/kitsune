#!/usr/bin/python
import numpy as np
import numpy.linalg
import sklearn as s
from sklearn import mixture
import MyPca
import MyGmm
import json

files = ['talking.dat', 'crying.dat', 'snoring.dat', 'vehicles.dat']

###########
# Get PCA
###########
ldata = []
for file in files:
    temp = MyPca.GetFeats(file)
    ldata.append(temp[:, 1:]) #ignore energy
    
    #"labeled" data
ldata = []
for file in files:
    temp = MyPca.GetFeats(file)
    ldata.append(temp[:, 1:]) #ignore energy
   
feats = MyPca.GetFeats('feats.dat')
feats = feats[:, 1:] #ignore energy


p = MyPca.MyPca()
p.fit(feats, 3) #do PCA on all data

tldata = []
for x in ldata:
    temp  = p.transform(x)
    temp = temp[:, 0:3]
    tldata.append(temp)
    
    
##########
#Fit the GMM
###########
ens = MyGmm.MyGmmEnsemble()

for x in tldata:
    g = mixture.GMM(n_components=1, covariance_type='full')
    g.fit(x)

    g2 = MyGmm.MyGmm()
    g2.setFromSklearnGmm(g)
    ens.addGmm(g2)


myclassifer = {}
myclassifer['pca'] = json.loads(p.toJson())
myclassifer['gmmsensemble'] = json.loads(ens.toJson())

f = open('gmm_coeffs.json', 'w')
json.dump(myclassifer, f)
f.close()
    

    
    
