#!/usr/bin/python
from struct import *
import sys
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
from matplotlib import cm

mel_size = 15


def FileToMatrix(filename,N):
    myints = []
    with open(filename,'rb') as f:
        exit = False
        while (not exit):
            data = f.read(1);

            try:
                num = unpack('B',data);
                myints.append(float(num[0]));
            except Exception:
                exit = True

    M = len(myints) / N
    print N,M
    return np.reshape(myints,(M,N))


if __name__ == '__main__':
    inputfile = sys.argv[1]
    mat = FileToMatrix(inputfile,mel_size)
    
    X,Y = np.meshgrid(np.arange(mat.shape[1]),np.arange(mat.shape[0]))
    
    fig = plt.figure()
    ax = fig.gca(projection='3d')
    surf = ax.plot_surface(X,Y,mat,cmap=cm.coolwarm)
    ax.view_init(90,0)
    plt.show()
