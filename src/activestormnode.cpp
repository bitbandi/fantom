// Copyright (c) 2009-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"
#include "activestormnode.h"
#include "stormnodeman.h"
#include <boost/lexical_cast.hpp>
#include "clientversion.h"

//
// Bootup the stormnode, look for a 1000 DRKSLK input and register on the network
//
void CActiveStormnode::ManageStatus()
{
    std::string errorMessage;

    if(!fStormNode) return;

    if (fDebug) LogPrintf("CActiveStormnode::ManageStatus() - Begin\n");

    //need correct adjusted time to send ping
    bool fIsInitialDownload = IsInitialBlockDownload();
    if(fIsInitialDownload) {
        status = STORMNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveStormnode::ManageStatus() - Sync in progress. Must wait until sync is complete to start stormnode.\n");
        return;
    }

    if(status == STORMNODE_INPUT_TOO_NEW || status == STORMNODE_NOT_CAPABLE || status == STORMNODE_SYNC_IN_PROCESS){
        status = STORMNODE_NOT_PROCESSED;
    }

    if(status == STORMNODE_NOT_PROCESSED) {
        if(strStormNodeAddr.empty()) {
            if(!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the stormnodeaddr configuration option.";
                status = STORMNODE_NOT_CAPABLE;
                LogPrintf("CActiveStormnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else {
        	service = CService(strStormNodeAddr);
        }

        LogPrintf("CActiveStormnode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString().c_str());

          // DRKSLKNOTE: There is no logical reason to restrict this to a specific port.  Its a peer, what difference does it make.
          /*  if(service.GetPort() != 31000) {
                notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " -only 31000 is supported on mainnet.";
                status = STORMNODE_NOT_CAPABLE;
                LogPrintf("CActiveStormnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else if(service.GetPort() == 31000) {
            notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " - 31000 is only supported on mainnet.";
            status = STORMNODE_NOT_CAPABLE;
            LogPrintf("CActiveStormnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }
        */

            if(!ConnectNode((CAddress)service, service.ToString().c_str())){
                notCapableReason = "Could not connect to " + service.ToString();
                status = STORMNODE_NOT_CAPABLE;
                LogPrintf("CActiveStormnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        

        if(pwalletMain->IsLocked()){
            notCapableReason = "Wallet is locked.";
            status = STORMNODE_NOT_CAPABLE;
            LogPrintf("CActiveStormnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }

        // Set defaults
        status = STORMNODE_NOT_CAPABLE;
        notCapableReason = "Unknown. Check debug.log for more information.\n";

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;

        if(GetStormNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress)) {

            if(GetInputAge(vin) < STORMNODE_MIN_CONFIRMATIONS){
                notCapableReason = "Input must have least " + boost::lexical_cast<string>(STORMNODE_MIN_CONFIRMATIONS) +
                        " confirmations - " + boost::lexical_cast<string>(GetInputAge(vin)) + " confirmations";
                LogPrintf("CActiveStormnode::ManageStatus() - %s\n", notCapableReason.c_str());
                status = STORMNODE_INPUT_TOO_NEW;
                return;
            }

            LogPrintf("CActiveStormnode::ManageStatus() - Is capable Stormnode!\n");

            status = STORMNODE_IS_CAPABLE;
            notCapableReason = "";

            pwalletMain->LockCoin(vin.prevout);

            // send to all nodes
            CPubKey pubKeyStormnode;
            CKey keyStormnode;

            if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, keyStormnode, pubKeyStormnode))
            {
            	LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
            	return;
            }

            /* donations are not supported in darksilk.conf */
            CScript donationAddress = CScript();
            int donationPercentage = 0;

            if(!Register(vin, service, keyCollateralAddress, pubKeyCollateralAddress, keyStormnode, pubKeyStormnode, donationAddress, donationPercentage, errorMessage)) {
                LogPrintf("CActiveStormnode::ManageStatus() - Error on Register: %s\n", errorMessage.c_str());
            }

            return;
        } else {
        	LogPrintf("CActiveStormnode::ManageStatus() - Could not find suitable coins!\n");
        }
    }

    //send to all peers
    if(!Sseep(errorMessage)) {
       LogPrintf("CActiveStormnode::ManageStatus() - Error on Ping: %s\n", errorMessage.c_str());    }
}

// Send stop sseep to network for remote stormnode
bool CActiveStormnode::StopStormNode(std::string strService, std::string strKeyStormnode, std::string& errorMessage) {
	CTxIn vin;
    CKey keyStormnode;
    CPubKey pubKeyStormnode;

    if(!sandStormSigner.SetKey(strKeyStormnode, errorMessage, keyStormnode, pubKeyStormnode)) {
    	LogPrintf("CActiveStormnode::StopStormNode() - Error: %s\n", errorMessage.c_str());
		return false;
	}

	return StopStormNode(vin, CService(strService), keyStormnode, pubKeyStormnode, errorMessage);
}

// Send stop sseep to network for main stormnode
bool CActiveStormnode::StopStormNode(std::string& errorMessage) {
	if(status != STORMNODE_IS_CAPABLE && status != STORMNODE_REMOTELY_ENABLED) {
		errorMessage = "stormnode is not in a running status";
    	LogPrintf("CActiveStormnode::StopStormNode() - Error: %s\n", errorMessage.c_str());
		return false;
	}

	status = STORMNODE_STOPPED;

    CPubKey pubKeyStormnode;
    CKey keyStormnode;

    if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, keyStormnode, pubKeyStormnode))
    {
    	LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
    	return false;
    }

	return StopStormNode(vin, service, keyStormnode, pubKeyStormnode, errorMessage);
}

// Send stop sseep to network for any stormnode
bool CActiveStormnode::StopStormNode(CTxIn vin, CService service, CKey keyStormnode, CPubKey pubKeyStormnode, std::string& errorMessage) {
   	pwalletMain->UnlockCoin(vin.prevout);
	return Sseep(vin, service, keyStormnode, pubKeyStormnode, errorMessage, true);
}

bool CActiveStormnode::Sseep(std::string& errorMessage) {
	if(status != STORMNODE_IS_CAPABLE && status != STORMNODE_REMOTELY_ENABLED) {
		errorMessage = "stormnode is not in a running status";
    	LogPrintf("CActiveStormnode::Sseep() - Error: %s\n", errorMessage.c_str());
		return false;
	}

    CPubKey pubKeyStormnode;
    CKey keyStormnode;

    if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, keyStormnode, pubKeyStormnode))
    {
    	LogPrintf("ActiveStormnode::Sseep() - Error upon calling SetKey: %s\n", errorMessage.c_str());
    	return false;
    }

	return Sseep(vin, service, keyStormnode, pubKeyStormnode, errorMessage, false);
}

bool CActiveStormnode::Sseep(CTxIn vin, CService service, CKey keyStormnode, CPubKey pubKeyStormnode, std::string &retErrorMessage, bool stop) {
    std::string errorMessage;
    std::vector<unsigned char> vchStormNodeSignature;
    std::string strStormNodeSignMessage;
    int64_t stormNodeSignatureTime = GetAdjustedTime();

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(stormNodeSignatureTime) + boost::lexical_cast<std::string>(stop);

    if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchStormNodeSignature, keyStormnode)) {
    	retErrorMessage = "sign message failed: " + errorMessage;
    	LogPrintf("CActiveStormnode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!sandStormSigner.VerifyMessage(pubKeyStormnode, vchStormNodeSignature, strMessage, errorMessage)) {
    	retErrorMessage = "Verify message failed: " + errorMessage;
    	LogPrintf("CActiveStormnode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    CStormnode* psn = snodeman.Find(vin);
    if(psn != NULL)
    {
        if(stop)
            snodeman.Remove(psn->vin);
        else
            psn->UpdateLastSeen();
    } else {
    	// Seems like we are trying to send a ping while the stormnode is not registered in the network
    	retErrorMessage = "Sandstorm Stormnode List doesn't include our stormnode, Shutting down stormnode pinging service! " + vin.ToString();
    	LogPrintf("CActiveStormnode::Sseep() - Error: %s\n", retErrorMessage.c_str());
        status = STORMNODE_NOT_CAPABLE;
        notCapableReason = retErrorMessage;
        return false;
    }

    //send to all peers
    LogPrintf("CActiveStormnode::Sseep() - RelayStormnodeEntryPing vin = %s\n", vin.ToString().c_str());
    snodeman.RelayStormnodeEntryPing(vin, vchStormNodeSignature, stormNodeSignatureTime, stop);

    return true;
}

bool CActiveStormnode::Register(std::string strService, std::string strKeyStormnode, std::string txHash, std::string strOutputIndex, std::string strDonationAddress, std::string strDonationPercentage, std::string& errorMessage) {
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyStormnode;
    CKey keyStormnode;
    CScript donationAddress = CScript();
    int donationPercentage = 0;

    if(!sandStormSigner.SetKey(strKeyStormnode, errorMessage, keyStormnode, pubKeyStormnode))
    {
        LogPrintf("CActiveStormnode::Register() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    if(!GetStormNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress, txHash, strOutputIndex)) {
        errorMessage = "could not allocate vin";
        LogPrintf("CActiveStormnode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    CDarkSilkAddress address;
    if (strDonationAddress != "")
    {
        if(!address.SetString(strDonationAddress))
        {
            LogPrintf("ActiveStormnode::Register - Invalid Donation Address\n");
            return false;
        }
        donationAddress.SetDestination(address.Get());

        try {
            donationPercentage = boost::lexical_cast<int>( strDonationPercentage );
        } catch( boost::bad_lexical_cast const& ) {
            LogPrintf("ActiveStormnode::Register - Invalid Donation Percentage (Couldn't cast)\n");
            return false;
        }

        if(donationPercentage < 0 || donationPercentage > 100)
        {
            LogPrintf("ActiveStormnode::Register - Donation Percentage Out Of Range\n");
            return false;
        }
    }

    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyStormnode, pubKeyStormnode, donationAddress, donationPercentage, errorMessage);
}

bool CActiveStormnode::Register(CTxIn vin, CService service, CKey keyCollateralAddress, CPubKey pubKeyCollateralAddress, CKey keyStormnode, CPubKey pubKeyStormnode, CScript donationAddress, int donationPercentage, std::string &retErrorMessage) {
    std::string errorMessage;
    std::vector<unsigned char> vchStormNodeSignature;
    std::string strStormNodeSignMessage;
    int64_t stormNodeSignatureTime = GetAdjustedTime();

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyStormnode.begin(), pubKeyStormnode.end());

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(stormNodeSignatureTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(PROTOCOL_VERSION) + donationAddress.ToString() + boost::lexical_cast<std::string>(donationPercentage);

    if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchStormNodeSignature, keyCollateralAddress)) {
        retErrorMessage = "sign message failed: " + errorMessage;
        LogPrintf("CActiveStormnode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!sandStormSigner.VerifyMessage(pubKeyCollateralAddress, vchStormNodeSignature, strMessage, errorMessage)) {
        retErrorMessage = "Verify message failed: " + errorMessage;
        LogPrintf("CActiveStormnode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    CStormnode* psn = snodeman.Find(vin);
    if(psn == NULL)
    {
        LogPrintf("CActiveStormnode::Register() - Adding to Stormnode list service: %s - vin: %s\n", service.ToString().c_str(), vin.ToString().c_str());
        CStormnode mn(service, vin, pubKeyCollateralAddress, vchStormNodeSignature, stormNodeSignatureTime, pubKeyStormnode, PROTOCOL_VERSION, donationAddress, donationPercentage);
        mn.UpdateLastSeen(stormNodeSignatureTime);
        snodeman.Add(mn);
    }

    //send to all peers
    LogPrintf("CActiveStormnode::Register() - RelayElectionEntry vin = %s\n", vin.ToString().c_str());
    snodeman.RelayStormnodeEntry(vin, service, vchStormNodeSignature, stormNodeSignatureTime, pubKeyCollateralAddress, pubKeyStormnode, -1, -1, stormNodeSignatureTime, PROTOCOL_VERSION, donationAddress, donationPercentage);

    return true;
}

bool CActiveStormnode::RegisterByPubKey(std::string strService, std::string strKeyStormnode, std::string collateralAddress, std::string& errorMessage) {
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyStormnode;
    CKey keyStormnode;
    CScript donationAddress = CScript();
    int donationPercentage = 0;

    if(!sandStormSigner.SetKey(strKeyStormnode, errorMessage, keyStormnode, pubKeyStormnode))
    {
        LogPrintf("CActiveStormnode::RegisterByPubKey() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    if(!GetStormNodeVinForPubKey(collateralAddress, vin, pubKeyCollateralAddress, keyCollateralAddress)) {
        errorMessage = "could not allocate vin for collateralAddress";
        LogPrintf("Register::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }
    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyStormnode, pubKeyStormnode, donationAddress, donationPercentage, errorMessage);
}

bool CActiveStormnode::GetStormNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {
	return GetStormNodeVin(vin, pubkey, secretKey, "", "");
}

bool CActiveStormnode::GetStormNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex) {
    CScript pubScript;

    // Find possible candidates
    vector<COutput> possibleCoins = SelectCoinsStormnode();
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
			LogPrintf("CActiveStormnode::GetStormNodeVin - Could not locate valid vin\n");
			return false;
		}
	} else {
		// No output specified,  Select the first one
		if(possibleCoins.size() > 0) {
			selectedOutput = &possibleCoins[0];
		} else {
			LogPrintf("CActiveStormnode::GetStormNodeVin - Could not locate specified vin from possible list\n");
			return false;
		}
    }

	// At this point we have a selected output, retrieve the associated info
	return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}

bool CActiveStormnode::GetStormNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {
	return GetStormNodeVinForPubKey(collateralAddress, vin, pubkey, secretKey, "", "");
}

bool CActiveStormnode::GetStormNodeVinForPubKey(std::string collateralAddress, CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex) {
    CScript pubScript;

    // Find possible candidates
    vector<COutput> possibleCoins = SelectCoinsStormnodeForPubKey(collateralAddress);
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
			LogPrintf("CActiveStormnode::GetStormNodeVinForPubKey - Could not locate valid vin\n");
			return false;
		}
	} else {
		// No output specified,  Select the first one
		if(possibleCoins.size() > 0) {
			selectedOutput = &possibleCoins[0];
		} else {
			LogPrintf("CActiveStormnode::GetStormNodeVinForPubKey - Could not locate specified vin from possible list\n");
			return false;
		}
    }

	// At this point we have a selected output, retrieve the associated info
	return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}


// Extract stormnode vin information from output
bool CActiveStormnode::GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {

    CScript pubScript;

	vin = CTxIn(out.tx->GetHash(),out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

	CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CDarkSilkAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CActiveStormnode::GetStormNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf ("CActiveStormnode::GetStormNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running stormnode
vector<COutput> CActiveStormnode::SelectCoinsStormnode()
{
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;

    // Retrieve all possible outputs
    pwalletMain->AvailableCoinsSN(vCoins);

    // Filter
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        if(out.tx->vout[out.i].nValue == STORMNODE_COLLATERAL*COIN) { //exactly
        	filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}

// get all possible outputs for running stormnode for a specific pubkey
vector<COutput> CActiveStormnode::SelectCoinsStormnodeForPubKey(std::string collateralAddress)
{
    CDarkSilkAddress address(collateralAddress);
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address.Get());
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(vCoins);

    // Filter
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        if(out.tx->vout[out.i].scriptPubKey == scriptPubKey && out.tx->vout[out.i].nValue == 42000*COIN) { //exactly
        	filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}


// when starting a stormnode, this can enable to run as a hot wallet with no funds
bool CActiveStormnode::EnableHotColdStormNode(CTxIn& newVin, CService& newService)
{
    if(!fStormNode) return false;

    status = STORMNODE_REMOTELY_ENABLED;

    //The values below are needed for signing sseep messages going forward
    this->vin = newVin;
    this->service = newService;

    LogPrintf("CActiveStormnode::EnableHotColdStormNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}
