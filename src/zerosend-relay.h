
// Copyright (c) 2014-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZEROSEND_RELAY_H
#define ZEROSEND_RELAY_H

#include "core.h"
#include "main.h"
#include "activeblanknode.h"
#include "blanknodeman.h"


class CZeroSendRelay
{
public:
	CTxIn vinBlanknode;
    vector<unsigned char> vchSig;
    vector<unsigned char> vchSig2;
    int nBlockHeight;
    int nRelayType;
    CTxIn in;
    CTxOut out;

    CZeroSendRelay();
    CZeroSendRelay(CTxIn& vinBlanknodeIn, vector<unsigned char>& vchSigIn, int nBlockHeightIn, int nRelayTypeIn, CTxIn& in2, CTxOut& out2);
    
    IMPLEMENT_SERIALIZE
    (
    	READWRITE(vinBlanknode);
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