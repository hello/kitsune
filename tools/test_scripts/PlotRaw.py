#!/usr/bin/python
from struct import *
import sys
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
from matplotlib import cm
from pylab import *

def FileToMatrix(filename):
    myints = []
    with open(filename,'rb') as f:
        exit = False
        while (not exit):
            data = f.read(2);

            try:
                #num = unpack('B',data);
                num = unpack('h',data);
                myints.append(float(num[0]));
            except Exception:
                exit = True

        return myints

if __name__ == '__main__':
    inputfile = sys.argv[1]
    mat = FileToMatrix(inputfile)
    x = range(len(mat))
    x2 = []
    for xx in x:
        x2.append(xx/1024)

    plot(x2,mat)
    show()    
