#include "audioclassifier.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>     /* abs */
#include <stdio.h>
#include "fft.h"
#include "debugutils/debuglog.h"
#include "debugutils/matmessageutils.h"



#define ITEMS_SIZE_2N (8)
#define ITEMS_SIZE (1 << ITEMS_SIZE_2N)
#define ITEMS_SIZE_MASK (ITEMS_SIZE - 1)

#define  NUM_LIST_ITEMS (32)

#define SIMILARITY_THRESHOLD (0.707f)

/******************
 typedefs
 *****************/


typedef struct ListItem {
    uint8_t listidx;
    uint8_t updatecount;
    struct ListItem * next;
    struct ListItem * prev;
} ListItem_t; //~12 bytes


typedef struct {
    int8_t feats[NUM_LIST_ITEMS][NUM_AUDIO_FEATURES]; //32 * 16 = 512 bytes
    uint8_t featsidx[NUM_LIST_ITEMS]; //32 bytes
    ListItem_t listdata[NUM_LIST_ITEMS];//32 * 12 = 384 bytes
    uint16_t featModeIndex;

    uint8_t occurencesindices[ITEMS_SIZE]; //256 bytes
    uint8_t occurenceDurations[ITEMS_SIZE]; //256 bytes
    uint16_t occurenceDeltaTimeSinceLastSegment[ITEMS_SIZE]; //512 bytes

    uint16_t occurenceidx;
    
    uint16_t numItemsInOccurenceBuffer;
    NotificationCallback_t fpNovelDataCallback;
    int64_t lastUpdateTime;
    int64_t firstUpdateTime;
    uint8_t updateCountNoveltyThreshold;
    
    MutexCallback_t fpLock;
    MutexCallback_t fpUnlock;
    
} AudioClassifier_t;



/******************
  static memory
 *****************/
static const int16_t k_similarity_threshold = TOFIX(SIMILARITY_THRESHOLD,10);
static const char * k_occurence_indices_buf_id = "occurenceIndices";
static const char * k_occurence_durations_buf_id = "occurenceDurations";
static const char * k_occurence_deltatimes_buf_id = "occurenceDeltaTimes";
static const char * k_feat_vec_buf_id = "featVecs";
static const char * k_feat_index_buf_id = "featIndices";
static const char * k_occurence_info_buf_id = "occurenceInfo";

static ListItem_t * _pHead;
static ListItem_t * _pFree;
static ListItem_t * _pTail;

static AudioClassifier_t _data;


/******************
 static functions
 *****************/
static ListItem_t * FindSimilarItem(const int8_t * featvec8) {
    
    int16_t cosvec;
    ListItem_t * p;
    int8_t * feat;
    
    //go through list to all stored feature vectors
    //take dot product, normalized, of featvec8 with stored feat vecs
    //if cosine of angle is greater than some threshold, we consider
    //these vectors similar
    p = _pHead;
    while(p) {
        feat = _data.feats[p->listidx];
        
        //take dot product of each feature vector
        cosvec = cosvec8(feat, featvec8, NUM_AUDIO_FEATURES);
        
        //if similar enough
        if (cosvec > k_similarity_threshold) {
            break;
        }
        
        p = p->next;
    }
    
    return p;
}


/*
 List is arranged in descending order in time
 
 If the list fills up (i.e. no more free elements) we pop off the last
 element in the list.
 
 Otherwise, we insert an element in the list
 */
static ListItem_t * AddItemToHeadOfList(const int8_t * featvec8,const Segment_t * pSeg) {
    ListItem_t * p;
    int8_t * featdata;
    
    //kill oldest item if we have no free space
    if (_pFree == NULL) {
        
        if (!_pTail) {
            //! \todo LOG ERROR!
            return NULL;
        }
        
        //pop
        _pFree = _pTail;
        _pTail = _pTail->prev;
        
        //terminate end
        _pTail->next = NULL;
        
        _pFree->prev = NULL;
    }
    
    //we will always have a free item here
    if (!_pHead) {
        //first add
        _pHead = _pFree;
        _pFree = _pFree->next;
        _pHead->next = NULL;
        _pFree->prev = NULL;
        _pTail = _pHead;
    }
    else {
        //insert new item at top
        p = _pHead; //old head is p
        
        //new head is free
        _pHead = _pFree;
        
        //pop head of free list
        _pFree = _pFree->next;
        if (_pFree) {
            _pFree->prev = NULL;
        }
        
        //new head next is old head
        _pHead->next = p;
        
        //old head prev is new head
        p->prev = _pHead;
    }
    
    featdata = _data.feats[_pHead->listidx];
    
    memcpy(featdata,featvec8,sizeof(int8_t)*NUM_AUDIO_FEATURES);
    _data.featsidx[_pHead->listidx] = _data.featModeIndex;
    _pHead->updatecount = 0;
    return  _pHead;
}

static void MoveItemToHeadOfList(ListItem_t * p) {
    
    //I'm the head?
    if (!p->prev) {
        //yes so do nothing
        return;
    }
    
    //am I the tail?
    if (!p->next) {
        //yes, so pop from tail
        _pTail = p->prev;
        _pTail->next = NULL;
    }
    else {
        //pop from the middle of the list
        p->prev->next = p->next;
        p->next->prev = p->prev;
    }
    
    p->next = _pHead;
    
    _pHead = p;
    _pHead->next->prev = _pHead;
    _pHead->prev = NULL;
    
    
}


void Scale16VecTo8(int8_t * pscaled, const int16_t * vec,uint16_t n) {
    int16_t extrema = 0;
    int16_t i;
    int16_t absval;
    int16_t shift;
    
    for (i = 0; i < n; i++) {
        absval = abs(vec[i]);
        if (absval > extrema) {
            extrema = absval;
        }
    }
    
    shift = CountHighestMsb(extrema);
    shift -= 7;
    
    if (shift > 0 ) {
        for (i = 0; i < n; i++) {
            pscaled[i] = vec[i] >> shift;
        }
    }
    else {
        for (i = 0; i < n; i++) {
            pscaled[i] = (int8_t)vec[i];
        }
    }
    
}



/******************
 exported functions
 *****************/

void AudioClassifier_Init(uint8_t updateCountNoveltyThreshold,NotificationCallback_t novelDataCallback,MutexCallback_t fpLock, MutexCallback_t fpUnlock) {
    uint16_t i;
    
    _data.fpNovelDataCallback = novelDataCallback;
    
    //set up the link list
    memset(&_data,0,sizeof(AudioClassifier_t));
    
    _pFree = &_data.listdata[0];
    _pHead = NULL;
    _pTail = NULL;
    for (i = 0; i < NUM_LIST_ITEMS - 1; i++) {
        _data.listdata[i].next = &_data.listdata[i+1];
        _data.listdata[i+1].prev = &_data.listdata[i];
    }
    
    for (i = 0; i < NUM_LIST_ITEMS; i++) {
        _data.listdata[i].listidx = i;
    }
    
    _data.updateCountNoveltyThreshold = updateCountNoveltyThreshold;
    _data.fpUnlock = fpUnlock;
    _data.fpLock = fpLock;
}

/* Call this after you pull the buffer */
void AudioClassifier_ResetUpdateTime(void) {
    
    if (_data.fpLock) {
        _data.fpLock();
    }

    _data.firstUpdateTime = 0;
    _data.numItemsInOccurenceBuffer = 0;
    
    if (_data.fpUnlock) {
        _data.fpUnlock();
    }
}

/* 
 
   - Set stream to NULL to get size of written buffer
   - Set source and tags to NULL if you want.
 
 */
uint32_t AudioClassifier_GetSerializedBuffer(pb_ostream_t * stream,const char * macbytes, uint32_t unix_time,const char * tags, const char * source) {
    
    uint32_t size = 0;
    
    
    if (_data.fpLock) {
        _data.fpLock();
    }
    
    {
        const uint16_t info[2] = {_data.occurenceidx,_data.numItemsInOccurenceBuffer};
        
        MatDesc_t descs[6] = {
            {k_occurence_info_buf_id,tags,source,{},1,2,_data.firstUpdateTime,_data.lastUpdateTime},
            {k_occurence_indices_buf_id,tags,source,{},1,_data.numItemsInOccurenceBuffer,_data.firstUpdateTime,_data.lastUpdateTime},
            {k_occurence_durations_buf_id,tags,source,{},1,_data.numItemsInOccurenceBuffer,_data.firstUpdateTime,_data.lastUpdateTime},
            {k_occurence_deltatimes_buf_id,tags,source,{},1,_data.numItemsInOccurenceBuffer,_data.firstUpdateTime,_data.lastUpdateTime},
            {k_feat_index_buf_id,tags,source,{},1,NUM_LIST_ITEMS,_data.firstUpdateTime,_data.lastUpdateTime},
            {k_feat_vec_buf_id,tags,source,{},NUM_LIST_ITEMS,NUM_AUDIO_FEATURES,_data.firstUpdateTime,_data.lastUpdateTime},
        };
        
        /*****************/
        descs[0].data.len = 2;
        descs[0].data.type = euint16;
        descs[0].data.data.uint16 = info;
        
        /*****************/
        descs[1].data.len = _data.numItemsInOccurenceBuffer;
        descs[1].data.type = euint8;
        descs[1].data.data.uint8 = _data.occurencesindices;
        
        /*****************/
        descs[2].data.len = _data.numItemsInOccurenceBuffer;
        descs[2].data.type = euint8;
        descs[2].data.data.uint8 = _data.occurenceDurations;
        
        /*****************/
        descs[3].data.len = _data.numItemsInOccurenceBuffer;
        descs[3].data.type = euint16;
        descs[3].data.data.uint16 = _data.occurenceDeltaTimeSinceLastSegment;
        
        /*****************/
        descs[4].data.len = NUM_LIST_ITEMS;
        descs[4].data.type = euint8;
        descs[4].data.data.uint8 = _data.featsidx;
        
        /*****************/
        descs[5].data.len = NUM_LIST_ITEMS*NUM_AUDIO_FEATURES;
        descs[5].data.type = esint8;
        descs[5].data.data.sint8 = &_data.feats[0][0];
        
        
        size = SetMatrixMessage(stream, macbytes, unix_time, descs, sizeof(descs) / sizeof(MatDesc_t));
    }
    
    
    if (_data.fpUnlock) {
        _data.fpUnlock();
    }

    return size;
}



void AudioClassifier_SegmentCallback(const int16_t * feats, const Segment_t * pSegment) {
    
    uint8_t isNovel = FALSE;
    
    if (_data.fpLock) {
        _data.fpLock();
    }
    
    {
        int8_t featvec8[NUM_AUDIO_FEATURES];
        ListItem_t * pitem;
        uint8_t duration;
        int64_t deltaTimeSinceLastUpdate;
        
        //scale to int8
        Scale16VecTo8(featvec8,feats,NUM_AUDIO_FEATURES);
        
        //go through list and find a similar item (potentially NUM_LIST_ITEMS dot products)
        pitem = FindSimilarItem(featvec8);
        
        if (pitem) {
            //we found a similar item!
            MoveItemToHeadOfList(pitem);
        }
        else {
            //add similarity vector, because this is phresh
            pitem = AddItemToHeadOfList(featvec8,pSegment);
            
            //increment (and possibly even rollover) our index number for the features
            _data.featModeIndex++;
        }
        
        //safety first
        if (pitem) {
            
            //for the first N new feature vectors, let someone know that this is a new sound
            if (pitem->updatecount++ < _data.updateCountNoveltyThreshold) {
                isNovel = TRUE;
            }
            
            
            //compute how long it's been since the last segment came in
            deltaTimeSinceLastUpdate = pSegment->t1 - _data.lastUpdateTime;
            _data.lastUpdateTime = pSegment->t1;
            
            if (deltaTimeSinceLastUpdate > UINT16_MAX) {
                deltaTimeSinceLastUpdate = UINT16_MAX;
            }
            
            _data.occurenceDeltaTimeSinceLastSegment[_data.occurenceidx] = (uint16_t)deltaTimeSinceLastUpdate;
            
            if (!_data.firstUpdateTime) {
                _data.firstUpdateTime = pSegment->t1;
            }
            
            //add occurence to circular buffer, give it index of the feature vector to which is associated
            _data.occurencesindices[_data.occurenceidx] = _data.featsidx[pitem->listidx];
            
            //compute duration
            if (pSegment->duration > UINT8_MAX) {
                duration = UINT8_MAX;
            }
            else {
                duration = pSegment->duration;
            }
            
            _data.occurenceDurations[_data.occurenceidx] = duration;
            
            
            //update occurence index, wrapping as necessary
            _data.occurenceidx++;
            _data.occurenceidx &= ITEMS_SIZE_MASK;
            
            //update number of items in occurence buffer
            if (++_data.numItemsInOccurenceBuffer > ITEMS_SIZE ) {
                _data.numItemsInOccurenceBuffer = ITEMS_SIZE;
            }
            
            DEBUG_LOG_S16("featAudio",NULL,feats,NUM_AUDIO_FEATURES,pSegment->t1,pSegment->t2);
        }
    }
    
    if (_data.fpUnlock) {
        _data.fpUnlock();
    }
    
    if (isNovel && _data.fpNovelDataCallback) {
        _data.fpNovelDataCallback();
    }
    
}
















