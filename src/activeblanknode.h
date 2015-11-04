// Copyright (c) 2009-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The DarkCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ACTIVESTORMNODE_H
#define ACTIVESTORMNODE_H

#include "uint256.h"
#include "sync.h"
#include "net.h"
#include "key.h"
//#include "primitives/transaction.h"
#include "main.h"
#include "init.h"
#include "wallet.h"
#include "sandstorm.h"

// Responsible for activating the stormnode and pinging the network
class CActiveStormnode
{
public:
	// Initialized by init.cpp
	// Keys for the main stormnode
	CPubKey pubKeyStormnode;

	// Initialized while registering stormnode
	CTxIn vin;
    CService service;

    int status;
    std::string notCapableReason;

    CActiveStormnode()
    {        
        status = STORMNODE_NOT_PROCESSED;
    }

    void ManageStatus(); // manage status of main stormnode

    bool Sseep(std::string& errorMessage); // ping for main stormnode
    bool Sseep(CTxIn vin, CService service, CKey key, CPubKey pubKey, std::string &retErrorMessage, bool stop); // ping for any stormnode

    bool StopStormNode(std::string& errorMessage); // stop main stormnode
    bool StopStormNode(std::string strService, std::string strKeyStormnode, std::string& errorMessage); // stop remote stormnode
    bool StopStormNode(CTxIn vin, CService service, CKey key, CPubKey pubKey, std::string& errorMessage); // stop any stormnode

    /// Register remote Stormnode
    bool Register(std::string strService, std::string strKey, std::string txHash, std::string strOutputIndex, std::string strDonationAddress, std::string strDonationPercentage, std::string& errorMessage); 
    /// Register any Stormnode
    bool Register(CTxIn vin, CService service, CKey key, CPubKey pubKey, CKey keyStormnode, CPubKey pubKeyStormnode, CScript donationAddress, int donationPercentage, std::string &retErrorMessage); 
    bool RegisterByPubKey(std::string strService, std::string strKeyStormnode, std::string collateralAddress, std::string& errorMessage); // register for a specific collateral address    
    
    // get 512FNX input that can be used for the stormnode
    bool GetStormNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    bool GetStormNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    bool GetStormNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    bool GetStormNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    vector<COutput> SelectCoinsStormnode();
    vector<COutput> SelectCoinsStormnodeForPubKey(std::string collateralAddress);
    bool GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey);

    //bool SelectCoinsStormnode(CTxIn& vin, int64& nValueIn, CScript& pubScript, std::string strTxHash, std::string strOutputIndex);

    // enable hot wallet mode (run a stormnode with no funds)
    bool EnableHotColdStormNode(CTxIn& vin, CService& addr);
};

#endif
