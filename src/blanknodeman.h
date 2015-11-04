
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BLANKNODEMAN_H
#define BLANKNODEMAN_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
#include "blanknode.h"

#define BLANKNODES_DUMP_SECONDS               (15*60)
#define BLANKNODES_SSEG_SECONDS              (1*60*60)

using namespace std;

class CBlanknodeMan;

extern CBlanknodeMan snodeman;

void DumpBlanknodes();

/*
 Access to the SN database (sncache.dat) 
*/
 
class CBlanknodeDB
{
private:
    boost::filesystem::path pathSN;
    std::string strMagicMessage;
public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CBlanknodeDB();
    bool Write(const CBlanknodeMan &snodemanToSave);
    ReadResult Read(CBlanknodeMan& snodemanToLoad);
};

class CBlanknodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // map to hold all SNs
    std::vector<CBlanknode> vBlanknodes;
    // who's asked for the blanknode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForBlanknodeList;
    // who we asked for the blanknode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForBlanknodeList;
    // which blanknodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForBlanknodeListEntry;

public:
    // keep track of ssq count to prevent blanknodes from ganing Zerosend queue
    int64_t nSsqCount;

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * blanknodes vector
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vBlanknodes);
                READWRITE(mAskedUsForBlanknodeList);
                READWRITE(mWeAskedForBlanknodeList);
                READWRITE(mWeAskedForBlanknodeListEntry);
                READWRITE(nSsqCount);
        }
    )

    CBlanknodeMan();
    CBlanknodeMan(CBlanknodeMan& other);
    
    // Add an entry
    bool Add(CBlanknode &sn);

    // Check all blanknodes
    void Check();

    // Check all blanknodes and remove inactive
    void CheckAndRemove();

    // Clear blanknode vector
    void Clear();
    
    int CountEnabled();
    
    int CountBlanknodesAboveProtocol(int protocolVersion);

    void SsegUpdate(CNode* pnode);

    // Find an entry
    CBlanknode* Find(const CTxIn& vin);
    CBlanknode* Find(const CPubKey& pubKeyBlanknode);

    //Find an entry thta do not match every entry provided vector
    CBlanknode* FindOldestNotInVec(const std::vector<CTxIn> &vVins, int nMinimumAge);

    // Find a random entry
    CBlanknode* FindRandom();

    // Get the current winner for this block
    CBlanknode* GetCurrentBlankNode(int mod=1, int64_t nBlockHeight=0, int minProtocol=0);

    std::vector<CBlanknode> GetFullBlanknodeVector() { Check(); return vBlanknodes; }

    std::vector<pair<int, CBlanknode> > GetBlanknodeRanks(int64_t nBlockHeight, int minProtocol=0);
    int GetBlanknodeRank(const CTxIn &vin, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);
    CBlanknode* GetBlanknodeByRank(int nRank, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);

    void ProcessBlanknodeConnections();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    // Return the number of (unique) blanknodes
    int size() { return vBlanknodes.size(); }

    std::string ToString() const;

    //
    // Relay Blanknode Messages
    //

    void RelayBlanknodeEntry(const CTxIn vin, const CService addr, const std::vector<unsigned char> vchSig, const int64_t nNow, const CPubKey pubkey, const CPubKey pubkey2, const int count, const int current, const int64_t lastUpdated, const int protocolVersion, CScript donationAddress, int donationPercentage);
    void RelayBlanknodeEntryPing(const CTxIn vin, const std::vector<unsigned char> vchSig, const int64_t nNow, const bool stop);

    void Remove(CTxIn vin);
    
};

#endif
