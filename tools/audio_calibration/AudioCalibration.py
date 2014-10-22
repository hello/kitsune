#!/usr/bin/python
import wave
from scipy.signal import *
from pylab import *
import sys
import struct
from numpy import *


def gather_energies_by_octave(fs, z):
    z = z[1:]
    N = len(z)
    fbin = fs/2/N
    print N
    k = 1
    nextk = 2
    step = 1
    energy = 0

    energies = []
    f = []
    while(k < N):
        energy = 0
        while k < nextk:
            energy = energy + z[k]
            k = k + 1
            
        energies.append(energy)
        f.append((k - step/2)*fbin)
        step = step * 2
        nextk = nextk + step
            
    plot(f, 20*log10(energies), 'o-')
    ylabel('dB (power)')
    xlabel('freq' )
    show()
        
def read_wav_file(filename):
    #assumes that wave file is of pink noise
    #equal energy for equal octaves
    print ('Opening %s' % (filename))
    f = wave.open(sys.argv[1])
    fs = f.getframerate()
    
    print ('Reading frames, fs = %d...' % fs)
    x = f.readframes(f.getnframes())
    
    f.close()
    y = zeros((len(x)/2))
        
    k = 0
    for j in range(len(x)/2):
        myints = struct.unpack('h',x[2*j:2*j+2])
        y[k] = myints[0]
        k = k + 1
                        
    
    f, z = welch(y, fs, nfft=1024 * 8)
      
    myfs = zeros(1)
    myfs[0] = fs
    return myfs, f, z

if __name__ == '__main__':
    if len(sys.argv) > 1:
        fs, f, z = read_wav_file(sys.argv[1])
        save ('fz', (fs, f, z))

    else:
        fs, f, z = load('fz.npy')
    
    gather_energies_by_octave(fs[0], z)

