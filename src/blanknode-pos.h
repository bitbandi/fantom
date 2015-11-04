

// Copyright (c) 2014-2015 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef STORMNODE_POS_H
#define STORMNODE_POS_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"

using namespace std;
using namespace boost;

class CStormnodeScanning;
class CStormnodeScanningError;

extern map<uint256, CStormnodeScanningError> mapStormnodeScanningErrors;
extern CStormnodeScanning snscan;

static const int MIN_STORMNODE_POS_PROTO_VERSION = 60020;

/*
	1% of the network is scanned every 2.5 minutes, making a full
	round of scanning take about 4.16 hours. We're targeting about 
	a day of proof-of-service errors for complete removal from the 
	stormnode system.
*/
static const int STORMNODE_SCANNING_ERROR_THESHOLD = 6;

#define SCANNING_SUCCESS                       1
#define SCANNING_ERROR_NO_RESPONSE             2
#define SCANNING_ERROR_IX_NO_RESPONSE          3
#define SCANNING_ERROR_MAX                     3

void ProcessMessageStormnodePOS(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

class CStormnodeScanning
{
public:
    void DoStormnodePOSChecks();
    void CleanStormnodeScanningErrors();
};

// Returns how many stormnodes are allowed to scan each block
int GetCountScanningPerBlock();

class CStormnodeScanningError
{
public:
    CTxIn vinStormnodeA;
    CTxIn vinStormnodeB;
    int nErrorType;
    int nExpiration;
    int nBlockHeight;
    std::vector<unsigned char> vchStormNodeSignature;

    CStormnodeScanningError ()
    {
        vinStormnodeA = CTxIn();
        vinStormnodeB = CTxIn();
        nErrorType = 0;
        nExpiration = GetTime()+(60*60);
        nBlockHeight = 0;
    }

    CStormnodeScanningError (CTxIn& vinStormnodeAIn, CTxIn& vinStormnodeBIn, int nErrorTypeIn, int nBlockHeightIn)
    {
    	vinStormnodeA = vinStormnodeAIn;
    	vinStormnodeB = vinStormnodeBIn;
    	nErrorType = nErrorTypeIn;
    	nExpiration = GetTime()+(60*60);
    	nBlockHeight = nBlockHeightIn;
    }

    CStormnodeScanningError (CTxIn& vinStormnodeBIn, int nErrorTypeIn, int nBlockHeightIn)
    {
        //just used for IX, StormnodeA is any client
        vinStormnodeA = CTxIn();
        vinStormnodeB = vinStormnodeBIn;
        nErrorType = nErrorTypeIn;
        nExpiration = GetTime()+(60*60);
        nBlockHeight = nBlockHeightIn;
    }

    uint256 GetHash() const {return SerializeHash(*this);}

    bool SignatureValid();
    bool Sign();
    bool IsExpired() {return GetTime() > nExpiration;}
    void Relay();
    bool IsValid() {
    	return (nErrorType > 0 && nErrorType <= SCANNING_ERROR_MAX);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(vinStormnodeA);
        READWRITE(vinStormnodeB);
        READWRITE(nErrorType);
        READWRITE(nExpiration);
        READWRITE(nBlockHeight);
        READWRITE(vchStormNodeSignature);
    )
};


#endif