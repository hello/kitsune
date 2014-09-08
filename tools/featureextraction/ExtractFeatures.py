#!/usr/bin/python
import base64
import sys
import numpy as np
import os
import json

sys.path.append('.')
import matrix_pb2

file_ending_of_feature_files = '.mel'

sample_size = 20

def DeserializeMatrixData(data):
    binarydata = base64.b64decode(data)
    mat = matrix_pb2.Matrix()
    mat.ParseFromString(binarydata) 
    array =  np.reshape(mat.idata,(mat.rows,mat.cols))
    tags = mat.tags.split(',')
    
    item = {}
    item['id'] = mat.id
    item['tags'] = tags
    item['source'] = mat.source
    item['data'] = array
    
    return item
    
def GetArrayFromListOfItems(items):
    count = 0
    isfirst = True
    for item in items:
        data = item['data']
        if isfirst:
            a = data
            isfirst = False
        else:
            a = np.concatenate([a, data])
    return a
    
    
if __name__ == '__main__':
    argc = len(sys.argv)
    itemlist = []
    itemdict = {}
    filtered_tags = []

    if argc < 2:
        print 'need to at least specify the feature you are looking for'
        sys.exit(0)
        
    
    feature_string = sys.argv[1]
    
    for i in range(2, argc):
        filtered_tags.append(sys.argv[i])
        
    for root, dir, files in os.walk(".", topdown=False):
        #do not do subdirectories yet
        if root is not '.':
            continue
            
        #filter by files that only have the desired file ending
        melfiles = [fi for fi in files if fi.endswith(file_ending_of_feature_files)]
        
        for mel in melfiles:
            lines = tuple(open(mel, 'r'))
            print 'Ingesting ' + mel
            for line in lines:
                #deserialize data
                item = DeserializeMatrixData(line)
                
                #filter by items that have the correct feature name                
                if  feature_string == item['id']:
                    
                        #filter by items that have the correct tags
                        tags = item['tags']
                        isgood = True
                        
                        for tag in filtered_tags:
                            if tag not in tags:
                                isgood = False
                                break;
 
                        if isgood:
                            print item
                            itemlist.append(item) 
                            for tag in tags:
                                if not itemdict.has_key(tag):
                                    itemdict[tag] = []
                                    
                                itemdict[tag].append(item)
                    
     

     
    print 'Saving everything to feats.dat'
    fid = open('feats.dat', 'wb')
    np.save(fid, GetArrayFromListOfItems(itemlist))
    fid.close()
    
    for key in itemdict.keys():
        filename = key + '.dat'
        items = itemdict[key]

        print 'Saving %d items to %s...' %  (len(items), filename)
        fid = open(filename, 'wb')
        myarray = GetArrayFromListOfItems(items)
        print myarray.shape
        np.save(fid, myarray)
        fid.close()
