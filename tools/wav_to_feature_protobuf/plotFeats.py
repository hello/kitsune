#!/usr/bin/python
import sys
import matrix_pb2
import numpy as np
from matplotlib.pyplot import *

def mat_to_array(m):
    return np.array(m.idata).reshape(m.rows,m.cols)

def main():
    f = open(sys.argv[1],'rb')
    p = matrix_pb2.MatrixClientMessage.FromString(f.read())
    f.close()
    arr = None
    arr_energy = None
    for m in p.matrix_list:
        if m.id == 'feature_chunk':
            if arr == None:
                arr = mat_to_array(m)
            else:
                arr = np.append(arr,mat_to_array(m),axis=0)

    print arr.shape

    for m in p.matrix_list:
        if m.id == 'energy_chunk':
            if arr_energy == None:
                arr_energy = mat_to_array(m)
            else:
                arr_energy = np.append(arr_energy,mat_to_array(m),axis=1)

    arr_energy = arr_energy.transpose()

    #"mfccs", plot them if you want
    arr2 = arr * np.tile(arr_energy,(1,16)) / 1024.0 / 8.0
    
    print arr_energy.shape
    
    figure(1); 
    ax1 = None
    for i in range(arr.shape[1]):
        ax = subplot(arr.shape[1],1,i + 1,sharex=ax1); plot(arr[:,i]);
        if i == 0:
            title('normalzied  MFCC coefficients')
            ax1 = ax
        ylabel('%d' % i)
    #3.0 is to go from log2 to 20*log10, the 1024 factor is because we are using fixed point Q10 internally 
    figure(2); plot(arr_energy/1024.0 * 3.0); title('energy in dB')
#    figure(2); plot(arr_energy.transpose()); title('energy in dB')
    show()
    
    

if __name__ == '__main__':
    main()
