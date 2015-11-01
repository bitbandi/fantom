// Copyright (c) 2009-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The DarkCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ACTIVEBLANKNODE_H
#define ACTIVEBLANKNODE_H

#include "uint256.h"
#include "sync.h"
#include "net.h"
#include "key.h"
//#include "primitives/transaction.h"
#include "main.h"
#include "init.h"
#include "wallet.h"
#include "zerosend.h"

// Responsible for activating the blanknode and pinging the network
class CActiveBlanknode
{
public:
	// Initialized by init.cpp
	// Keys for the main blanknode
	CPubKey pubKeyBlanknode;

	// Initialized while registering blanknode
	CTxIn vin;
    CService service;

    int status;
    std::string notCapableReason;

    CActiveBlanknode()
    {        
        status = BLANKNODE_NOT_PROCESSED;
    }

    void ManageStatus(); // manage status of main blanknode

    bool Sseep(std::string& errorMessage); // ping for main blanknode
    bool Sseep(CTxIn vin, CService service, CKey key, CPubKey pubKey, std::string &retErrorMessage, bool stop); // ping for any blanknode

    bool StopBlankNode(std::string& errorMessage); // stop main blanknode
    bool StopBlankNode(std::string strService, std::string strKeyBlanknode, std::string& errorMessage); // stop remote blanknode
    bool StopBlankNode(CTxIn vin, CService service, CKey key, CPubKey pubKey, std::string& errorMessage); // stop any blanknode

    /// Register remote Blanknode
    bool Register(std::string strService, std::string strKey, std::string txHash, std::string strOutputIndex, std::string strDonationAddress, std::string strDonationPercentage, std::string& errorMessage); 
    /// Register any Blanknode
    bool Register(CTxIn vin, CService service, CKey key, CPubKey pubKey, CKey keyBlanknode, CPubKey pubKeyBlanknode, CScript donationAddress, int donationPercentage, std::string &retErrorMessage); 
    bool RegisterByPubKey(std::string strService, std::string strKeyBlanknode, std::string collateralAddress, std::string& errorMessage); // register for a specific collateral address    
    
    // get 512FNX input that can be used for the blanknode
    bool GetBlankNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    bool GetBlankNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    bool GetBlankNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    bool GetBlankNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    vector<COutput> SelectCoinsBlanknode();
    vector<COutput> SelectCoinsBlanknodeForPubKey(std::string collateralAddress);
    bool GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey);

    //bool SelectCoinsBlanknode(CTxIn& vin, int64& nValueIn, CScript& pubScript, std::string strTxHash, std::string strOutputIndex);

    // enable hot wallet mode (run a blanknode with no funds)
    bool EnableHotColdBlankNode(CTxIn& vin, CService& addr);
};

#endif
