#!/usr/bin/python
import numpy as np
import numpy.linalg
import json
import scipy.linalg

class MyGmm():
    def init(self):
        self.L_ = []
        self.LTInv_ = []
        self.denom_ = []
        
        logk2pi_ = -0.5 * np.log(np.pi * 2)*self.dim_

        for i in range(self.nmodels_):
            mat = np.matrix(self.covars_[i])
            mychol = scipy.linalg.cholesky(mat)
            mycholInverseTranspose = np.linalg.inv(mychol).transpose()

            #compute sqrt(det(covariance))
            k = 1;
            for j in range(self.dim_):
                k = k * mychol[j, j]   
          
            self.denom_.append(-np.log(k) + logk2pi_)
            
            self.L_.append(mychol)
            self.LTInv_.append(mycholInverseTranspose)
        
        

        
    def setFromSklearnGmm(self, g):
        self.covars_ = g.covars_
        self.means_ = g.means_
        self.weights_ = g.weights_
        self.dim_ = self.covars_.shape[1]
        self.nmodels_ = self.covars_.shape[0]
        self.init()
        
    def toJson(self):
        me = {}
        me['covars'] = self.covars_.tolist()
        me['means'] = self.means_.tolist()
        me['weights'] = self.weights_.tolist()
        me['dim'] = self.dim_
        me['nmodels'] = self.nmodels_
        return json.dumps(me)
        
    def setFromJson(self, str):
        me = json.loads(str)
        self.covars_ = np.array(me['covars'])
        self.means_ = np.array(me['means'])
        self.weights_ = np.array(me['weights'])
        self.dim_ = me['dim']
        self.nmodels_ = me['nmodels']
        self.init()


    #excepts x as a bunch of rows
    def evaluate(self, x):
        logliksum = np.array([])
        first = True
        for imodel in range(self.nmodels_):
            loglik = self.evalgaussian(self.L_[imodel], self.LTInv_[imodel], self.weights_[imodel], self.means_[imodel], self.denom_[imodel], x)

            if first:
                logliksum = loglik
            else:
                logliksum = logliksum + loglik
            
        return logliksum
        
    def evalgaussian(self, L, LTInv, w, mean, loglikdenom, x):
        # lik  == 1/sqrt( (2*pi)^k * det(P))  * exp[ -0.5 (x-mu)^T  * P^-1 * (x-mu) ]
        # L = cholesky decomposition of P, P^-1 = (L'L)^-1 = L^-1 * L'^-1
        # y = (L^T)^-1 * (x - mu)  
        # log(lik) = -1/2 [k*log(2*pi)] - 0.5*log(L(1,1)) - 0.5*log(L(2,2)) ... -0.5 *log(L(N,N))  - 0.5 y^T * y
        #print np.tile(mean, (x.shape[0], 1))
        xnomean = x + np.tile(mean, (x.shape[0], 1))

        xt = xnomean.transpose()
        y = np.matrix(LTInv) * xt
                
        y = np.array(y)
        
        y = y * y;
        
        y = np.sum(y, axis=0)
        y = y * -0.5
        loglik = y + loglikdenom
                
        return loglik
            
if __name__ == '__main__':
    #test
    data = {"dim": 3, "covars": 
        [[[1, 0.1, 0.2], 
            [0.1, 1, 0.3], 
             [0.2, 0.3, 1]]], 
    "nmodels": 1, "weights": [1.0000000000000002], 
    "means": [[-0.1, -0.2, -0.3]]}

    g = MyGmm()
    g.setFromJson(json.dumps(data))
    
    x = np.array([0.1, 0.2, 0.3])
    x = x.reshape((1, 3))
    print g.evaluate(x) + 2.6883 
    
    x = np.array([0.0, 0.15, 0.5])
    x = x.reshape((1, 3))

    print g.evaluate(x) + 2.7245

    
