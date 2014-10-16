#!/usr/bin/python
from sklearn.cluster import spectral
import numpy
from numpy import *
from numpy.linalg import *
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import copy

colors = ['b', 'g', 'r', 'c']

set_printoptions(precision=3, suppress=True, threshold=numpy.nan, linewidth=10000)


def plot3(xx):

    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    
    k = 0
    for x in xx:
        y = x[:, 1].tolist()
        z = x[:, 2].tolist()
        x = x[:, 0].tolist()
        
        ax.scatter(x,y,z, c=colors[k],marker='o')
        k = k + 1
    
    plt.show()

class AudioFeatureClustering(object):
    def set_data(self, data):
        if isinstance(data, basestring):
            self.x = array(load(data).astype('float'))
        else:
            self.x = copy.deepcopy(data)
            
        #normalize
        for j in range(self.x.shape[0]):
            self.x[j,:] = self.x[j,:] / linalg.norm(self.x[j,:])
            
        self.compute_similarity_scores()
            
            
    def compute_similarity_scores(self):
    
        x = self.x
        a = zeros( (x.shape[0], x.shape[0]) )

    
        print x.shape
        for j in range(x.shape[0]):
            for i in range(j):
            
                temp = dot(x[j,:], x[i,:])
                a[j, i] = temp
                a[i, j] = temp;
            
    
        self.similarity = a
        
        
        
    def compute_cluster_means(self, n_components, threshold):
        
        #get connection matrix from similarity
        connection_matrix = copy.deepcopy(self.similarity)
        connection_matrix[where(self.similarity < threshold)] = 0.0
        connection_matrix[where(self.similarity >= threshold)] = 1.0

   
        labels = spectral.spectral_clustering(connection_matrix, n_components=n_components, eigen_solver='arpack')
        
        #organize data by labels
        g = []
        for ilabel in range(n_components):
            g.append(self.x[where(labels == ilabel)[0]])

        #compute mean of data in each cluster
        v = []
        for mat in g:
            v.append(mean(mat,axis=0))
        
        
        self.vectors = matrix(v)
        
    
    def get_transformed_data(self):
        return self.x * self.vectors.transpose()


    



