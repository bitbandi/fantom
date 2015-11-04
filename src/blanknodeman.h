
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef STORMNODEMAN_H
#define STORMNODEMAN_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
#include "stormnode.h"

#define STORMNODES_DUMP_SECONDS               (15*60)
#define STORMNODES_SSEG_SECONDS              (1*60*60)

using namespace std;

class CStormnodeMan;

extern CStormnodeMan snodeman;

void DumpStormnodes();

/*
 Access to the SN database (sncache.dat) 
*/
 
class CStormnodeDB
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

    CStormnodeDB();
    bool Write(const CStormnodeMan &snodemanToSave);
    ReadResult Read(CStormnodeMan& snodemanToLoad);
};

class CStormnodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // map to hold all SNs
    std::vector<CStormnode> vStormnodes;
    // who's asked for the stormnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForStormnodeList;
    // who we asked for the stormnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForStormnodeList;
    // which stormnodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForStormnodeListEntry;

public:
    // keep track of ssq count to prevent stormnodes from ganing Sandstorm queue
    int64_t nSsqCount;

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * stormnodes vector
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vStormnodes);
                READWRITE(mAskedUsForStormnodeList);
                READWRITE(mWeAskedForStormnodeList);
                READWRITE(mWeAskedForStormnodeListEntry);
                READWRITE(nSsqCount);
        }
    )

    CStormnodeMan();
    CStormnodeMan(CStormnodeMan& other);
    
    // Add an entry
    bool Add(CStormnode &sn);

    // Check all stormnodes
    void Check();

    // Check all stormnodes and remove inactive
    void CheckAndRemove();

    // Clear stormnode vector
    void Clear();
    
    int CountEnabled();
    
    int CountStormnodesAboveProtocol(int protocolVersion);

    void SsegUpdate(CNode* pnode);

    // Find an entry
    CStormnode* Find(const CTxIn& vin);
    CStormnode* Find(const CPubKey& pubKeyStormnode);

    //Find an entry thta do not match every entry provided vector
    CStormnode* FindOldestNotInVec(const std::vector<CTxIn> &vVins, int nMinimumAge, int nMinimumActiveSeconds);

    // Find a random entry
    CStormnode* FindRandom();

    // Get the current winner for this block
    CStormnode* GetCurrentStormNode(int mod=1, int64_t nBlockHeight=0, int minProtocol=0);

    std::vector<CStormnode> GetFullStormnodeVector() { Check(); return vStormnodes; }

    std::vector<pair<int, CStormnode> > GetStormnodeRanks(int64_t nBlockHeight, int minProtocol=0);
    int GetStormnodeRank(const CTxIn &vin, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);
    CStormnode* GetStormnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);

    void ProcessStormnodeConnections();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    // Return the number of (unique) stormnodes
    int size() { return vStormnodes.size(); }

    std::string ToString() const;

    //
    // Relay Stormnode Messages
    //

    void RelayStormnodeEntry(const CTxIn vin, const CService addr, const std::vector<unsigned char> vchSig, const int64_t nNow, const CPubKey pubkey, const CPubKey pubkey2, const int count, const int current, const int64_t lastUpdated, const int protocolVersion, CScript donationAddress, int donationPercentage);
    void RelayStormnodeEntryPing(const CTxIn vin, const std::vector<unsigned char> vchSig, const int64_t nNow, const bool stop);

    void Remove(CTxIn vin);
    
};

#endif
