
// Copyright (c) 2009-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BLANKNODE_H
#define BLANKNODE_H

#include "uint256.h"
#include "uint256.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "main.h"
#include "blanknode-pos.h"
#include "timedata.h"
#include "script.h"

class uint256;

#define BLANKNODE_NOT_PROCESSED               0 // initial state
#define BLANKNODE_IS_CAPABLE                  1
#define BLANKNODE_NOT_CAPABLE                 2
#define BLANKNODE_STOPPED                     3
#define BLANKNODE_INPUT_TOO_NEW               4
#define BLANKNODE_PORT_NOT_OPEN               6
#define BLANKNODE_PORT_OPEN                   7
#define BLANKNODE_SYNC_IN_PROCESS             8
#define BLANKNODE_REMOTELY_ENABLED            9

#define BLANKNODE_MIN_CONFIRMATIONS           15
#define BLANKNODE_MIN_SSEEP_SECONDS           (30*60)
#define BLANKNODE_MIN_SSEE_SECONDS            (5*60)
#define BLANKNODE_PING_SECONDS                (1*60)
#define BLANKNODE_EXPIRATION_SECONDS          (65*60)
#define BLANKNODE_REMOVAL_SECONDS             (70*60)

using namespace std;

class CBlankNode;
class CBlanknodePayments;
class CBlanknodePaymentWinner;

extern CCriticalSection cs_blanknodes;
extern CBlanknodePayments blanknodePayments;
extern map<uint256, CBlanknodePaymentWinner> mapSeenBlanknodeVotes;
extern map<int64_t, uint256> mapCacheBlockHashes;


void ProcessMessageBlanknodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool GetBlockHash(uint256& hash, int nBlockHeight);

//
// The Blanknode Class. For managing the zerosend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CBlanknode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
public:
    enum state {
    BLANKNODE_ENABLED = 1,
    BLANKNODE_EXPIRED = 2,
    BLANKNODE_VIN_SPENT = 3,
    BLANKNODE_REMOVE = 4,
    BLANKNODE_POS_ERROR = 5
    };

	static int minProtoVersion;
    CTxIn vin; 
    CService addr;
    CPubKey pubkey;
    CPubKey pubkey2;
    std::vector<unsigned char> sig;
    int activeState;
    int64_t sigTime; //ssee message times
    int64_t lastSseep;
    int64_t lastTimeSeen;
    int cacheInputAge;
    int cacheInputAgeBlock;
    bool unitTest;
    bool allowFreeTx;
    int protocolVersion;
    int64_t nLastSsq; //the ssq count from the last ssq broadcast of this node
    CScript donationAddress;
    int donationPercentage;    
    int nVote;
    int64_t lastVote;
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;

    CBlanknode();
    CBlanknode(const CBlanknode& other);
    CBlanknode(CService newAddr, CTxIn newVin, CPubKey newPubkey, std::vector<unsigned char> newSig, int64_t newSigTime, CPubKey newPubkey2, int protocolVersionIn, CScript donationAddress, int donationPercentage);
 
    void swap(CBlanknode& first, CBlanknode& second) // nothrow    
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubkey, second.pubkey);
        swap(first.pubkey2, second.pubkey2);
        swap(first.sig, second.sig);
        swap(first.activeState, second.activeState);
        swap(first.sigTime, second.sigTime);
        swap(first.lastSseep, second.lastSseep);
        swap(first.lastTimeSeen, second.lastTimeSeen);
        swap(first.cacheInputAge, second.cacheInputAge);
        swap(first.unitTest, second.unitTest);
        swap(first.cacheInputAgeBlock, second.cacheInputAgeBlock);
        swap(first.allowFreeTx, second.allowFreeTx);
        swap(first.protocolVersion, second.protocolVersion);        
        swap(first.nLastSsq, second.nLastSsq);
        swap(first.donationAddress, second.donationAddress);
        swap(first.donationPercentage, second.donationPercentage);
        swap(first.nVote, second.nVote);
        swap(first.lastVote, second.lastVote);
        swap(first.nScanningErrorCount, second.nScanningErrorCount);
        swap(first.nLastScanningErrorBlockHeight, second.nLastScanningErrorBlockHeight);
    }

    CBlanknode& operator=(CBlanknode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CBlanknode& a, const CBlanknode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CBlanknode& a, const CBlanknode& b)
    {
        return !(a.vin == b.vin);
    }

    uint256 CalculateScore(int mod=1, int64_t nBlockHeight=0);

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * all fields (?)
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vin);
                READWRITE(addr);
                READWRITE(pubkey);
                READWRITE(pubkey2);
                READWRITE(sig);
                READWRITE(activeState);
                READWRITE(sigTime);
                READWRITE(lastSseep);
                READWRITE(lastTimeSeen);
                READWRITE(cacheInputAge);
                READWRITE(cacheInputAgeBlock);
                READWRITE(unitTest);
                READWRITE(allowFreeTx);
                READWRITE(protocolVersion);
                READWRITE(nLastSsq);
                READWRITE(donationAddress);
                READWRITE(donationPercentage);
                READWRITE(nVote);
                READWRITE(lastVote);
                READWRITE(nScanningErrorCount);
                READWRITE(nLastScanningErrorBlockHeight);
        }
    )

    void UpdateLastSeen(int64_t override=0)
    {
        if(override == 0){
            lastTimeSeen = GetAdjustedTime();
        } else {
            lastTimeSeen = override;
        }
    }

    inline uint64_t SliceHash(uint256& hash, int slice)
    {
        uint64_t n = 0;
        memcpy(&n, &hash+slice*64, 64);
        return n;
    }

    void Check();

    bool UpdatedWithin(int seconds)
    {
        // LogPrintf("UpdatedWithin %d, %d --  %d \n", GetAdjustedTime() , lastTimeSeen, (GetAdjustedTime() - lastTimeSeen) < seconds);

        return (GetAdjustedTime() - lastTimeSeen) < seconds;
    }

    void Disable()
    {
        lastTimeSeen = 0;
    }

    bool IsEnabled()
    {
        return activeState == BLANKNODE_ENABLED;
    }

    int GetBlanknodeInputAge()
    {
        if(pindexBest == NULL) return 0;

        if(cacheInputAge == 0){
            cacheInputAge = GetInputAge(vin);
            cacheInputAgeBlock = pindexBest->nHeight;
        }

        return cacheInputAge+(pindexBest->nHeight-cacheInputAgeBlock);
    }

    void ApplyScanningError(CBlanknodeScanningError& snse)
    {
        if(!snse.IsValid()) return;

        if(snse.nBlockHeight == nLastScanningErrorBlockHeight) return;
        nLastScanningErrorBlockHeight = snse.nBlockHeight;

        if(snse.nErrorType == SCANNING_SUCCESS){
            nScanningErrorCount--;
            if(nScanningErrorCount < 0) nScanningErrorCount = 0;
        } else { //all other codes are equally as bad
            nScanningErrorCount++;
            if(nScanningErrorCount > BLANKNODE_SCANNING_ERROR_THESHOLD*2) nScanningErrorCount = BLANKNODE_SCANNING_ERROR_THESHOLD*2;
        }
    }

    std::string Status() {
    std::string strStatus = "ACTIVE";

    if(activeState == CBlanknode::BLANKNODE_ENABLED) strStatus   = "ENABLED";
    if(activeState == CBlanknode::BLANKNODE_EXPIRED) strStatus   = "EXPIRED";
    if(activeState == CBlanknode::BLANKNODE_VIN_SPENT) strStatus = "VIN_SPENT";
    if(activeState == CBlanknode::BLANKNODE_REMOVE) strStatus    = "REMOVE";
    if(activeState == CBlanknode::BLANKNODE_POS_ERROR) strStatus = "POS_ERROR";

    return strStatus;
    }
};

// for storing the winning payments
class CBlanknodePaymentWinner
{
public:
    int nBlockHeight;
    CTxIn vin;
    CScript payee;
    std::vector<unsigned char> vchSig;
    uint64_t score;

    CBlanknodePaymentWinner() {
        nBlockHeight = 0;
        score = 0;
        vin = CTxIn();
        payee = CScript();
    }

    uint256 GetHash(){
        uint256 n2 = Hash(BEGIN(nBlockHeight), END(nBlockHeight));
        uint256 n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);

        return n3;
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vin);
        READWRITE(score);
        READWRITE(vchSig);
     )
};

//
// Blanknode Payments Class
// Keeps track of who should get paid for which blocks
//

class CBlanknodePayments
{
private:
    std::vector<CBlanknodePaymentWinner> vWinning;
    int nSyncedFromPeer;
    std::string strMasterPrivKey;
    std::string strMainPubKey;
    bool enabled;
    int nLastBlockHeight;

public:

    CBlanknodePayments() {
        strMainPubKey = "";
        enabled = false;
    }

    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CBlanknodePaymentWinner& winner);
    bool Sign(CBlanknodePaymentWinner& winner);

    // Deterministically calculate a given "score" for a blanknode depending on how close it's hash is
    // to the blockHeight. The further away they are the better, the furthest will win the election
    // and get paid this block
    //

    uint64_t CalculateScore(uint256 blockHash, CTxIn& vin);
    bool GetWinningBlanknode(int nBlockHeight, CTxIn& vinOut);
    bool AddWinningBlanknode(CBlanknodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);
    void Relay(CBlanknodePaymentWinner& winner);
    void Sync(CNode* node);
    void CleanPaymentList();
    int LastPayment(CBlanknode& sn);

    //slow
    bool GetBlockPayee(int nBlockHeight, CScript& payee);
};



#endif
