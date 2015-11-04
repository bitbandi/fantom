
// Copyright (c) 2014-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SANDSTORM_RELAY_H
#define SANDSTORM_RELAY_H

#include "core.h"
#include "main.h"
#include "activestormnode.h"
#include "stormnodeman.h"


class CSandStormRelay
{
public:
	CTxIn vinStormnode;
    vector<unsigned char> vchSig;
    vector<unsigned char> vchSig2;
    int nBlockHeight;
    int nRelayType;
    CTxIn in;
    CTxOut out;

    CSandStormRelay();
    CSandStormRelay(CTxIn& vinStormnodeIn, vector<unsigned char>& vchSigIn, int nBlockHeightIn, int nRelayTypeIn, CTxIn& in2, CTxOut& out2);
    
    IMPLEMENT_SERIALIZE
    (
    	READWRITE(vinStormnode);
        READWRITE(vchSig);
        READWRITE(vchSig2);
		READWRITE(nBlockHeight);
		READWRITE(nRelayType);
		READWRITE(in);
		READWRITE(out);
    )

    std::string ToString();

    bool Sign(std::string strSharedKey);
    bool VerifyMessage(std::string strSharedKey);
    void Relay();
    void RelayThroughNode(int nRank);
};



#endif