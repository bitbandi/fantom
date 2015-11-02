// Copyright (c) 2009-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"
#include "activeblanknode.h"
#include "blanknodeman.h"
#include <boost/lexical_cast.hpp>
#include "clientversion.h"

//
// Bootup the blanknode, look for a 1000 FNX input and register on the network
//
void CActiveBlanknode::ManageStatus()
{
    std::string errorMessage;

    if(!fBlankNode) return;

    if (fDebug) LogPrintf("CActiveBlanknode::ManageStatus() - Begin\n");

    //need correct adjusted time to send ping
    bool fIsInitialDownload = IsInitialBlockDownload();
    if(fIsInitialDownload) {
        status = BLANKNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveBlanknode::ManageStatus() - Sync in progress. Must wait until sync is complete to start blanknode.\n");
        return;
    }

    if(status == BLANKNODE_INPUT_TOO_NEW || status == BLANKNODE_NOT_CAPABLE || status == BLANKNODE_SYNC_IN_PROCESS){
        status = BLANKNODE_NOT_PROCESSED;
    }

    if(status == BLANKNODE_NOT_PROCESSED) {
        if(strBlankNodeAddr.empty()) {
            if(!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the blanknodeaddr configuration option.";
                status = BLANKNODE_NOT_CAPABLE;
                LogPrintf("CActiveBlanknode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else {
        	service = CService(strBlankNodeAddr);
        }

        LogPrintf("CActiveBlanknode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString().c_str());

          // FNXNOTE: There is no logical reason to restrict this to a specific port.  Its a peer, what difference does it make.
          /*  if(service.GetPort() != 31000) {
                notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " -only 31000 is supported on mainnet.";
                status = BLANKNODE_NOT_CAPABLE;
                LogPrintf("CActiveBlanknode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else if(service.GetPort() == 31000) {
            notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " - 31000 is only supported on mainnet.";
            status = BLANKNODE_NOT_CAPABLE;
            LogPrintf("CActiveBlanknode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }
        */

            if(!ConnectNode((CAddress)service, service.ToString().c_str())){
                notCapableReason = "Could not connect to " + service.ToString();
                status = BLANKNODE_NOT_CAPABLE;
                LogPrintf("CActiveBlanknode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        

        if(pwalletMain->IsLocked()){
            notCapableReason = "Wallet is locked.";
            status = BLANKNODE_NOT_CAPABLE;
            LogPrintf("CActiveBlanknode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }

        // Set defaults
        status = BLANKNODE_NOT_CAPABLE;
        notCapableReason = "Unknown. Check debug.log for more information.\n";

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;

        if(GetBlankNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress)) {

            if(GetInputAge(vin) < BLANKNODE_MIN_CONFIRMATIONS){
                notCapableReason = "Input must have least " + boost::lexical_cast<string>(BLANKNODE_MIN_CONFIRMATIONS) +
                        " confirmations - " + boost::lexical_cast<string>(GetInputAge(vin)) + " confirmations";
                LogPrintf("CActiveBlanknode::ManageStatus() - %s\n", notCapableReason.c_str());
                status = BLANKNODE_INPUT_TOO_NEW;
                return;
            }

            LogPrintf("CActiveBlanknode::ManageStatus() - Is capable master node!\n");

            status = BLANKNODE_IS_CAPABLE;
            notCapableReason = "";

            pwalletMain->LockCoin(vin.prevout);

            // send to all nodes
            CPubKey pubKeyBlanknode;
            CKey keyBlanknode;

            if(!zeroSendSigner.SetKey(strBlankNodePrivKey, errorMessage, keyBlanknode, pubKeyBlanknode))
            {
            	LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
            	return;
            }

            /* donations are not supported in fantom.conf */
            CScript donationAddress = CScript();
            int donationPercentage = 0;

            if(!Register(vin, service, keyCollateralAddress, pubKeyCollateralAddress, keyBlanknode, pubKeyBlanknode, donationAddress, donationPercentage, errorMessage)) {
                LogPrintf("CActiveBlanknode::ManageStatus() - Error on Register: %s\n", errorMessage.c_str());
            }

            return;
        } else {
        	LogPrintf("CActiveBlanknode::ManageStatus() - Could not find suitable coins!\n");
        }
    }

    //send to all peers
    if(!Sseep(errorMessage)) {
       LogPrintf("CActiveBlanknode::ManageStatus() - Error on Ping: %s\n", errorMessage.c_str());    }
}

// Send stop sseep to network for remote blanknode
bool CActiveBlanknode::StopBlankNode(std::string strService, std::string strKeyBlanknode, std::string& errorMessage) {
	CTxIn vin;
    CKey keyBlanknode;
    CPubKey pubKeyBlanknode;

    if(!zeroSendSigner.SetKey(strKeyBlanknode, errorMessage, keyBlanknode, pubKeyBlanknode)) {
    	LogPrintf("CActiveBlanknode::StopBlankNode() - Error: %s\n", errorMessage.c_str());
		return false;
	}

	return StopBlankNode(vin, CService(strService), keyBlanknode, pubKeyBlanknode, errorMessage);
}

// Send stop sseep to network for main blanknode
bool CActiveBlanknode::StopBlankNode(std::string& errorMessage) {
	if(status != BLANKNODE_IS_CAPABLE && status != BLANKNODE_REMOTELY_ENABLED) {
		errorMessage = "blanknode is not in a running status";
    	LogPrintf("CActiveBlanknode::StopBlankNode() - Error: %s\n", errorMessage.c_str());
		return false;
	}

	status = BLANKNODE_STOPPED;

    CPubKey pubKeyBlanknode;
    CKey keyBlanknode;

    if(!zeroSendSigner.SetKey(strBlankNodePrivKey, errorMessage, keyBlanknode, pubKeyBlanknode))
    {
    	LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
    	return false;
    }

	return StopBlankNode(vin, service, keyBlanknode, pubKeyBlanknode, errorMessage);
}

// Send stop sseep to network for any blanknode
bool CActiveBlanknode::StopBlankNode(CTxIn vin, CService service, CKey keyBlanknode, CPubKey pubKeyBlanknode, std::string& errorMessage) {
   	pwalletMain->UnlockCoin(vin.prevout);
	return Sseep(vin, service, keyBlanknode, pubKeyBlanknode, errorMessage, true);
}

bool CActiveBlanknode::Sseep(std::string& errorMessage) {
	if(status != BLANKNODE_IS_CAPABLE && status != BLANKNODE_REMOTELY_ENABLED) {
		errorMessage = "blanknode is not in a running status";
    	LogPrintf("CActiveBlanknode::Sseep() - Error: %s\n", errorMessage.c_str());
		return false;
	}

    CPubKey pubKeyBlanknode;
    CKey keyBlanknode;

    if(!zeroSendSigner.SetKey(strBlankNodePrivKey, errorMessage, keyBlanknode, pubKeyBlanknode))
    {
    	LogPrintf("ActiveBlanknode::Sseep() - Error upon calling SetKey: %s\n", errorMessage.c_str());
    	return false;
    }

	return Sseep(vin, service, keyBlanknode, pubKeyBlanknode, errorMessage, false);
}

bool CActiveBlanknode::Sseep(CTxIn vin, CService service, CKey keyBlanknode, CPubKey pubKeyBlanknode, std::string &retErrorMessage, bool stop) {
    std::string errorMessage;
    std::vector<unsigned char> vchBlankNodeSignature;
    std::string strBlankNodeSignMessage;
    int64_t blankNodeSignatureTime = GetAdjustedTime();

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(blankNodeSignatureTime) + boost::lexical_cast<std::string>(stop);

    if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchBlankNodeSignature, keyBlanknode)) {
    	retErrorMessage = "sign message failed: " + errorMessage;
    	LogPrintf("CActiveBlanknode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubKeyBlanknode, vchBlankNodeSignature, strMessage, errorMessage)) {
    	retErrorMessage = "Verify message failed: " + errorMessage;
    	LogPrintf("CActiveBlanknode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    CBlanknode* psn = snodeman.Find(vin);
    if(psn != NULL)
    {
        if(stop)
            snodeman.Remove(psn->vin);
        else
            psn->UpdateLastSeen();
    } else {
    	// Seems like we are trying to send a ping while the blanknode is not registered in the network
    	retErrorMessage = "Zerosend Blanknode List doesn't include our blanknode, Shutting down blanknode pinging service! " + vin.ToString();
    	LogPrintf("CActiveBlanknode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        status = BLANKNODE_NOT_CAPABLE;
        notCapableReason = retErrorMessage;
        return false;
    }

    //send to all peers
    LogPrintf("CActiveBlanknode::Sseep() - RelayBlanknodeEntryPing vin = %s\n", vin.ToString().c_str());
    snodeman.RelayBlanknodeEntryPing(vin, vchBlankNodeSignature, blankNodeSignatureTime, stop);

    return true;
}

bool CActiveBlanknode::Register(std::string strService, std::string strKeyBlanknode, std::string txHash, std::string strOutputIndex, std::string strDonationAddress, std::string strDonationPercentage, std::string& errorMessage) {
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyBlanknode;
    CKey keyBlanknode;
    CScript donationAddress = CScript();
    int donationPercentage = 0;

    if(!zeroSendSigner.SetKey(strKeyBlanknode, errorMessage, keyBlanknode, pubKeyBlanknode))
    {
        LogPrintf("CActiveBlanknode::Register() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    if(!GetBlankNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress, txHash, strOutputIndex)) {
        errorMessage = "could not allocate vin";
        LogPrintf("CActiveBlanknode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    CFantomAddress address;
    if (strDonationAddress != "")
    {
        if(!address.SetString(strDonationAddress))
        {
            LogPrintf("ActiveBlanknode::Register - Invalid Donation Address\n");
            return false;
        }
        donationAddress.SetDestination(address.Get());

        try {
            donationPercentage = boost::lexical_cast<int>( strDonationPercentage );
        } catch( boost::bad_lexical_cast const& ) {
            LogPrintf("ActiveBlanknode::Register - Invalid Donation Percentage (Couldn't cast)\n");
            return false;
        }

        if(donationPercentage < 0 || donationPercentage > 100)
        {
            LogPrintf("ActiveBlanknode::Register - Donation Percentage Out Of Range\n");
            return false;
        }
    }

    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyBlanknode, pubKeyBlanknode, donationAddress, donationPercentage, errorMessage);
}

bool CActiveBlanknode::Register(CTxIn vin, CService service, CKey keyCollateralAddress, CPubKey pubKeyCollateralAddress, CKey keyBlanknode, CPubKey pubKeyBlanknode, CScript donationAddress, int donationPercentage, std::string &retErrorMessage) {
    std::string errorMessage;
    std::vector<unsigned char> vchBlankNodeSignature;
    std::string strBlankNodeSignMessage;
    int64_t blankNodeSignatureTime = GetAdjustedTime();

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyBlanknode.begin(), pubKeyBlanknode.end());

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(blankNodeSignatureTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(PROTOCOL_VERSION) + donationAddress.ToString() + boost::lexical_cast<std::string>(donationPercentage);

    if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchBlankNodeSignature, keyCollateralAddress)) {
        retErrorMessage = "sign message failed: " + errorMessage;
        LogPrintf("CActiveBlanknode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubKeyCollateralAddress, vchBlankNodeSignature, strMessage, errorMessage)) {
        retErrorMessage = "Verify message failed: " + errorMessage;
        LogPrintf("CActiveBlanknode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    CBlanknode* psn = snodeman.Find(vin);
    if(psn == NULL)
    {
        LogPrintf("CActiveBlanknode::Register() - Adding to Blanknode list service: %s - vin: %s\n", service.ToString().c_str(), vin.ToString().c_str());
        CBlanknode sn(service, vin, pubKeyCollateralAddress, vchBlankNodeSignature, blankNodeSignatureTime, pubKeyBlanknode, PROTOCOL_VERSION, donationAddress, donationPercentage);
        sn.UpdateLastSeen(blankNodeSignatureTime);
        snodeman.Add(sn);
    }

    //send to all peers
    LogPrintf("CActiveBlanknode::Register() - RelayElectionEntry vin = %s\n", vin.ToString().c_str());
    snodeman.RelayBlanknodeEntry(vin, service, vchBlankNodeSignature, blankNodeSignatureTime, pubKeyCollateralAddress, pubKeyBlanknode, -1, -1, blankNodeSignatureTime, PROTOCOL_VERSION, donationAddress, donationPercentage);

    return true;
}

bool CActiveBlanknode::RegisterByPubKey(std::string strService, std::string strKeyBlanknode, std::string collateralAddress, std::string& errorMessage) {
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyBlanknode;
    CKey keyBlanknode;
    CScript donationAddress = CScript();
    int donationPercentage = 0;

    if(!zeroSendSigner.SetKey(strKeyBlanknode, errorMessage, keyBlanknode, pubKeyBlanknode))
    {
        LogPrintf("CActiveBlanknode::RegisterByPubKey() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    if(!GetBlankNodeVinForPubKey(collateralAddress, vin, pubKeyCollateralAddress, keyCollateralAddress)) {
        errorMessage = "could not allocate vin for collateralAddress";
        LogPrintf("Register::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }
    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyBlanknode, pubKeyBlanknode, donationAddress, donationPercentage, errorMessage);
}

bool CActiveBlanknode::GetBlankNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {
	return GetBlankNodeVin(vin, pubkey, secretKey, "", "");
}

bool CActiveBlanknode::GetBlankNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex) {
    CScript pubScript;

    // Find possible candidates
    vector<COutput> possibleCoins = SelectCoinsBlanknode();
    COutput *selectedOutput;

    // Find the vin
	if(!strTxHash.empty()) {
		// Let's find it
		uint256 txHash(strTxHash);
        int outputIndex = boost::lexical_cast<int>(strOutputIndex);
		bool found = false;
		BOOST_FOREACH(COutput& out, possibleCoins) {
			if(out.tx->GetHash() == txHash && out.i == outputIndex)
			{
				selectedOutput = &out;
				found = true;
				break;
			}
		}
		if(!found) {
			LogPrintf("CActiveBlanknode::GetBlankNodeVin - Could not locate valid vin\n");
			return false;
		}
	} else {
		// No output specified,  Select the first one
		if(possibleCoins.size() > 0) {
			selectedOutput = &possibleCoins[0];
		} else {
			LogPrintf("CActiveBlanknode::GetBlankNodeVin - Could not locate specified vin from possible list\n");
			return false;
		}
    }

	// At this point we have a selected output, retrieve the associated info
	return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}

bool CActiveBlanknode::GetBlankNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {
	return GetBlankNodeVinForPubKey(collateralAddress, vin, pubkey, secretKey, "", "");
}

bool CActiveBlanknode::GetBlankNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex) {
    CScript pubScript;

    // Find possible candidates
    vector<COutput> possibleCoins = SelectCoinsBlanknodeForPubKey(collateralAddress);
    COutput *selectedOutput;

    // Find the vin
	if(!strTxHash.empty()) {
		// Let's find it
		uint256 txHash(strTxHash);
        int outputIndex = boost::lexical_cast<int>(strOutputIndex);
		bool found = false;
		BOOST_FOREACH(COutput& out, possibleCoins) {
			if(out.tx->GetHash() == txHash && out.i == outputIndex)
			{
				selectedOutput = &out;
				found = true;
				break;
			}
		}
		if(!found) {
			LogPrintf("CActiveBlanknode::GetBlankNodeVinForPubKey - Could not locate valid vin\n");
			return false;
		}
	} else {
		// No output specified,  Select the first one
		if(possibleCoins.size() > 0) {
			selectedOutput = &possibleCoins[0];
		} else {
			LogPrintf("CActiveBlanknode::GetBlankNodeVinForPubKey - Could not locate specified vin from possible list\n");
			return false;
		}
    }

	// At this point we have a selected output, retrieve the associated info
	return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}


// Extract blanknode vin information from output
bool CActiveBlanknode::GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {

    CScript pubScript;

	vin = CTxIn(out.tx->GetHash(),out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

	CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CFantomAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CActiveBlanknode::GetBlankNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf ("CActiveBlanknode::GetBlankNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running blanknode
vector<COutput> CActiveBlanknode::SelectCoinsBlanknode()
{
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;

    // Retrieve all possible outputs
    pwalletMain->AvailableCoinsSN(vCoins);

    // Filter
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        if(out.tx->vout[out.i].nValue == BLANKNODE_COLLATERAL*COIN) { //exactly
        	filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}

// get all possible outputs for running blanknode for a specific pubkey
vector<COutput> CActiveBlanknode::SelectCoinsBlanknodeForPubKey(std::string collateralAddress)
{
    CFantomAddress address(collateralAddress);
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address.Get());
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(vCoins);

    // Filter
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        if(out.tx->vout[out.i].scriptPubKey == scriptPubKey && out.tx->vout[out.i].nValue == 512*COIN) { //exactly
        	filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}


// when starting a blanknode, this can enable to run as a hot wallet with no funds
bool CActiveBlanknode::EnableHotColdBlankNode(CTxIn& newVin, CService& newService)
{
    if(!fBlankNode) return false;

    status = BLANKNODE_REMOTELY_ENABLED;

    //The values below are needed for signing sseep messages going forward
    this->vin = newVin;
    this->service = newService;

    LogPrintf("CActiveBlanknode::EnableHotColdBlankNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}
