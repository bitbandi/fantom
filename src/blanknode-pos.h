

// Copyright (c) 2014-2015 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BLANKNODE_POS_H
#define BLANKNODE_POS_H

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

class CBlanknodeScanning;
class CBlanknodeScanningError;

extern map<uint256, CBlanknodeScanningError> mapBlanknodeScanningErrors;
extern CBlanknodeScanning snscan;

static const int MIN_BLANKNODE_POS_PROTO_VERSION = 60020;

/*
	1% of the network is scanned every 2.5 minutes, making a full
	round of scanning take about 4.16 hours. We're targeting about 
	a day of proof-of-service errors for complete removal from the 
	blanknode system.
*/
static const int BLANKNODE_SCANNING_ERROR_THESHOLD = 6;

#define SCANNING_SUCCESS                       1
#define SCANNING_ERROR_NO_RESPONSE             2
#define SCANNING_ERROR_IX_NO_RESPONSE          3
#define SCANNING_ERROR_MAX                     3

void ProcessMessageBlanknodePOS(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

class CBlanknodeScanning
{
public:
    void DoBlanknodePOSChecks();
    void CleanBlanknodeScanningErrors();
};

// Returns how many blanknodes are allowed to scan each block
int GetCountScanningPerBlock();

class CBlanknodeScanningError
{
public:
    CTxIn vinBlanknodeA;
    CTxIn vinBlanknodeB;
    int nErrorType;
    int nExpiration;
    int nBlockHeight;
    std::vector<unsigned char> vchBlankNodeSignature;

    CBlanknodeScanningError ()
    {
        vinBlanknodeA = CTxIn();
        vinBlanknodeB = CTxIn();
        nErrorType = 0;
        nExpiration = GetTime()+(60*60);
        nBlockHeight = 0;
    }

    CBlanknodeScanningError (CTxIn& vinBlanknodeAIn, CTxIn& vinBlanknodeBIn, int nErrorTypeIn, int nBlockHeightIn)
    {
    	vinBlanknodeA = vinBlanknodeAIn;
    	vinBlanknodeB = vinBlanknodeBIn;
    	nErrorType = nErrorTypeIn;
    	nExpiration = GetTime()+(60*60);
    	nBlockHeight = nBlockHeightIn;
    }

    CBlanknodeScanningError (CTxIn& vinBlanknodeBIn, int nErrorTypeIn, int nBlockHeightIn)
    {
        //just used for IX, BlanknodeA is any client
        vinBlanknodeA = CTxIn();
        vinBlanknodeB = vinBlanknodeBIn;
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
        READWRITE(vinBlanknodeA);
        READWRITE(vinBlanknodeB);
        READWRITE(nErrorType);
        READWRITE(nExpiration);
        READWRITE(nBlockHeight);
        READWRITE(vchBlankNodeSignature);
    )
};


#endif