#!/usr/bin/python
import sklearn.svm
import sys
import numpy as np
from numpy import *
from numpy.linalg import * 
from sklearn.metrics import confusion_matrix
from sklearn import cross_validation

g_files = ['snoring.dat', 'talking.dat','null.dat']


def GetFeats(filename):
    fid = open(filename)
    feats = np.load(fid)
    fid.close()
    return feats

def GetSvmFromData(files):

    np.set_printoptions(precision=3, suppress=True, threshold=np.nan, linewidth=10000)
    #get labeled data
    labeled_data = []
    labels = []
    idx = 0
    for file in files:
        temp = GetFeats(file).astype(float)
        for i in range(temp.shape[0]):
            temp[i, :] = temp[i, :] / norm(temp[i, :])

        labels.append(np.array([idx for i in range(temp.shape[0])]))
        idx = idx + 1


        labeled_data.append(temp)

    labeled_data = np.concatenate(tuple(labeled_data))
    labels = np.concatenate(tuple(labels))

    #classify

    X_train, X_test, y_train, y_test = cross_validation.train_test_split(labeled_data, labels, test_size=0.2)

    svm = sklearn.svm.LinearSVC()
    svm.fit(X_train, y_train)

    ypred = svm.predict(X_test)

    cm = confusion_matrix(y_test, ypred).astype(float)
    rowcounts = np.sum(cm, axis=1)
    for i in range(cm.shape[1]):
        cm[i, :] = cm[i, :] / rowcounts[i]

    print cm

    coeffs = np.hstack((svm.coef_,svm.intercept_.reshape((svm.coef_.shape[0],1))))
    coeffs = coeffs * (1<<12)
    coeffs = np.round(coeffs).astype('int')

    print coeffs
    np.savetxt('coeffs.dat',coeffs,fmt='%d',delimiter=',');

    return svm

if __name__ == '__main__':
    svm = GetSvmFromData(g_files)

