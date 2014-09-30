#include "audioclassifier.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>     /* abs */
#include <stdio.h>
#include "fft.h"
#include "debugutils/debuglog.h"




static const int16_t k_similarity_threshold = TOFIX(0.707f,10);
static const uint32_t k_time_update_threshold = 60 * 43; //about a minute
static const uint32_t k_similarity_minimum_dt = 3; //counts, about 23ms each count





typedef struct {
    int64_t lastUpdateTime;  //8 bytes
    uint16_t updateCount;
    int8_t feat[NUM_AUDIO_FEATURES]; //16 bytes
} FeatMode_t; //~20 bytes

typedef struct ListItem {
    FeatMode_t data;
    struct ListItem * next;
    struct ListItem * prev;
} ListItem_t;

/* static memory  */

#define  NUM_LIST_ITEMS (16)
static ListItem_t _listdata[NUM_LIST_ITEMS];
static ListItem_t * _pHead;
static ListItem_t * _pFree;
static ListItem_t * _pTail;
static SegmentAndFeatureCallback_t _fpNovelDataCallback;



void AudioClassifier_Init(SegmentAndFeatureCallback_t novelDataCallback) {
    uint16_t i;

    _fpNovelDataCallback = novelDataCallback;
    
    //set up the link list
    _pFree = &_listdata[0];
    _pHead = NULL;
    _pTail = NULL;
    memset(_listdata,0,sizeof(_listdata));
    for (i = 0; i < NUM_LIST_ITEMS - 1; i++) {
        _listdata[i].next = &_listdata[i+1];
        _listdata[i+1].prev = &_listdata[i];
    }
}

ListItem_t * CheckSimilarityAndUpdateEntry(uint32_t * pDurationSinceLastUpdate,const int8_t * featvec8,const Segment_t * pSeg) {
    /*  Identify if novel  */
    int16_t cosvec;
    ListItem_t * p;
    
    //go through start of buffer to all stored feature vectors
    //take dot product, normalized, of featvec8 with stored feat vecs
    //if cosine of angle is greater than some threshold, we consider
    //these vectors similar
    p = _pHead;
    while(p) {
        FeatMode_t * pmode = &p->data;
        
        //take dot product of each feature vector
        cosvec = cosvec8(pmode->feat, featvec8, NUM_AUDIO_FEATURES);
        
        //if similar enough
        if (cosvec > k_similarity_threshold) {
            
            *pDurationSinceLastUpdate = 0;
            
            //update duration
            if (pmode->lastUpdateTime) {
                int64_t temp64 = pSeg->t1 - pmode->lastUpdateTime;
                
                if (temp64 > UINT32_MAX) {
                    temp64 = UINT32_MAX;
                }
                
                *pDurationSinceLastUpdate = (uint32_t)temp64;
            }
            
            //if a new sound, or if the time since last update is large enough, we will update the "lastUpdateTime"
            //otherwise, we consider a similar sound too close together to be the same sound
            if ( (*pDurationSinceLastUpdate) == 0 || (*pDurationSinceLastUpdate) >= k_similarity_minimum_dt ) {
                pmode->lastUpdateTime = pSeg->t1;
                pmode->updateCount++;
            }
            
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
static void AddItemToHeadOfList(const int8_t * featvec8,const Segment_t * pSeg) {
    ListItem_t * p;
    FeatMode_t * pdata;
    
    //kill oldest item if we have no free space
    if (_pFree == NULL) {

        if (!_pTail) {
            //! \todo LOG ERROR!
            return;
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
    
    pdata = &_pHead->data;
    
    memcpy(pdata->feat,featvec8,sizeof(pdata->feat));
    pdata->lastUpdateTime = pSeg->t1;
    pdata->updateCount = 0;
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




void AudioClassifier_SegmentCallback(const int16_t * feats, const Segment_t * pSegment) {
    int8_t featvec8[NUM_AUDIO_FEATURES];
    uint32_t durationSinceLastSimilarVector;
    uint32_t counts = 0;
    ListItem_t * pitem,* p2;
    Scale16VecTo8(featvec8,feats,NUM_AUDIO_FEATURES);

    pitem = CheckSimilarityAndUpdateEntry(&durationSinceLastSimilarVector,featvec8,pSegment);
    
    if (pitem) {
        MoveItemToHeadOfList(pitem);
        
        if (durationSinceLastSimilarVector > k_similarity_minimum_dt) {
            printf("SIMILAR: count=%d,%lld, %lld, time since last = %d\n",pitem->data.updateCount, pSegment->t1,pSegment->t2 - pSegment->t1,durationSinceLastSimilarVector);
        }
        
        
    }
    else {
        //add similarity vector
        AddItemToHeadOfList(featvec8,pSegment);
        
    }
    
    if (_fpNovelDataCallback) {
        _fpNovelDataCallback(feats,pSegment);
    }

    
#if 0
    p2 = _pHead;
    while (p2) {
        if (p2 != _pHead) {
            printf(",");
        }
        else {
            printf("ITEMS: ");
        }
        printf("%lld",p2->data.lastUpdateTime);
        p2 = p2->next;
    }
    
    printf("\n");
#endif
    
    DEBUG_LOG_S16("featAudio",NULL,feats,NUM_AUDIO_FEATURES,pSegment->t1,pSegment->t2);
    
    //go classify feature vector
    //! \todo CLASSIFY THIS FEATURE VECTOR
    
}
















