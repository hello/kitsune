#!/usr/bin/python

from numpy import *
from numpy.linalg import *
from sklearn.cluster import KMeans


def get_laplacian_matrix(filename, threshold):

    x = load(filename).astype('float')


    for j in range(x.shape[0]):
        x[j,:] = x[j,:] / linalg.norm(x[j,:])


    A = zeros((x.shape[0], x.shape[0]))
    
    for j in range(x.shape[0]):
        for i in range(j):
            temp = dot(x[j,:],x[i,:])
            if (temp > threshold):
                A[j, i] = 1;
                A[i, j] = 1;
            
      
    d = zeros(A.shape)
    d[where(A > 0)] = 1
    degree = sum(d, axis=0)

    #discard unconnected vertices
    idx = where(degree == 0)
    A = delete(A, idx, 0)
    A = A.transpose()
    A = delete(A, idx, 0)
    degree = delete(degree, idx)
    Dinv = sqrt(diag(1.0 / degree))
    D = diag(degree)
    L = D - A;
    Lnormalized = matrix(Dinv)*matrix(L)*matrix(Dinv)
    return Lnormalized, idx

def get_clusters_from_laplacian(L, x, idx):
    val,vec = eigh(L)
    zeroeigs_idx = where(val < 1e-6)
    vec = vec[:, zeroeigs_idx[0]]
    km = KMeans(n_clusters = len(idx[0]))
    km.fit[vec]
    print km.labels_
def get_correlation_matrix(filename):

    x = load(filename).astype('float')


    for j in range(x.shape[0]):
        x[j,:] = x[j,:] / linalg.norm(x[j,:])


    a = identity(x.shape[0])
    
    for j in range(x.shape[0]):
        for i in range(j):
            temp = dot(x[j,:],x[i,:])
            if (temp > 0):
                a[j, i] = temp;
                a[i, j] = temp;
            
      
    
    return a
    
    

