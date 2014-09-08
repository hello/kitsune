#!/usr/bin/python

import numpy as np
import numpy.linalg

files = ['talking.dat', 'crying.dat', 'snoring.dat', 'vehicles.dat']

def GetFeats(filename):
    fid = open(filename)
    feats = np.load(fid)
    fid.close()
    return feats


class MyPca():
    def fit(self, x, ndimsout):
        #print np.cov(feats.transpose, axis).shape
        mean =  np.mean(x, axis=0).reshape(1, x.shape[1])
        meanmat = np.tile(mean, (x.shape[0], 1))
        print meanmat.shape
        nomeanfeats = x - meanmat
        P = np.cov(nomeanfeats.transpose())
        d = np.sqrt(np.diagonal(P))

        for j in range(nomeanfeats.shape[1]):
            nomeanfeats[:, j] = nomeanfeats[:, j] / d[j]
    
        P2 = np.cov(nomeanfeats.transpose())

        w, v = np.linalg.eig(P2)

        transform = v[:, 0:ndimsout]
        
        self.covdiags_ = d
        self.explainedvariance_ = d / np.sum(d)
        self.mean_ = mean
        self.transform_ = transform
        #self.reducedfeats_ = f2
        
     #takes a bunch of row vectors   
    def transform(self, x):
        #subtrac tmean
        meanmat = np.tile(self.mean_, (x.shape[0], 1))
        nomeanfeats = x - meanmat
        
        #normalize by variance
        for j in range(nomeanfeats.shape[1]):
            nomeanfeats[:, j] = nomeanfeats[:, j] / self.covdiags_[j]
        
        #"rotate" and reduce dimensions
        retmat =  np.matrix(nomeanfeats) * np.matrix(self.transform_)
        return np.array(retmat)
        
    def toDict(self):
        me = {}
        me['covdiags'] = self.covdiags_.tolist()
        me['transform'] = self.transform_.tolist()
        me['mean'] = self.mean_.tolist()
        return me
        
    def setFromDict(self, me):
        self.covdiags_ = np.array(me['covdiags'])
        self.transform_ = np.array(me['transform'])
        self.mean_ = np.array(me['mean'])

#"labeled" data
if __name__ == '__main__':
    ldata = []
    for file in files:
        temp = GetFeats(file)
        ldata.append(temp[:, 1:]) #ignore energy
   
    feats = GetFeats('feats.dat')
    feats = feats[:, 1:] #ignore energy

    p = MyPca()
    p.fit(feats, 3)
    a =  p.transform(feats)
    s = p.toDict()
    print s
    p = MyPca()
    p.setFromDict(s)
    b=  p.transform(feats)

    print np.sum(a - b)


