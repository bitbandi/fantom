// Copyright (c) 2010-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "core.h"
#include "db.h"
#include "init.h"
#include "activeblanknode.h"
#include "blanknodeman.h"
#include "blanknodeconfig.h"
#include "rpcserver.h"
#include <boost/lexical_cast.hpp>
//#include "amount.h"
#include "util.h"
//#include "utilmoneystr.h"

#include <fstream>
using namespace json_spirit;
using namespace std;

void SendMoney(const CTxDestination &address, CAmount nValue, CWalletTx& wtxNew, AvailableCoinsType coin_type)
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse fantom address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    int64_t nFeeRequired;
    std::string sNarr;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, sNarr, wtxNew, reservekey, nFeeRequired, NULL))
    {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

Value zerosend(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0)
        throw runtime_error(
            "zerosend <fantomaddress> <amount>\n"
            "fantomaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if(params[0].get_str() == "auto"){
        if(fBlankNode)
            return "ZeroSend is not supported from blanknodes";

        zeroSendPool.DoAutomaticDenominating();
        return "DoAutomaticDenominating";
    }

    if(params[0].get_str() == "reset"){
        zeroSendPool.SetNull(true);
        zeroSendPool.UnlockCoins();
        return "successfully reset zerosend";
    }

    if (params.size() != 2)
        throw runtime_error(
            "zerosend <fantomaddress> <amount>\n"
            "fantomaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    CFantomAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid fantom address");

    // Amount
    int64_t nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    std::string sNarr;
    if (params.size() > 6 && params[6].type() != null_type && !params[6].get_str().empty())
        sNarr = params[6].get_str();
    
    if (sNarr.length() > 24)
        throw runtime_error("Narration must be 24 characters or less.");
    
    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, sNarr, wtx, ONLY_DENOMINATED);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
   
    return wtx.GetHash().GetHex();
}


Value getpoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    Object obj;
    obj.push_back(Pair("current_blanknode",        snodeman.GetCurrentBlankNode()->addr.ToString()));
    obj.push_back(Pair("state",        zeroSendPool.GetState()));
    obj.push_back(Pair("entries",      zeroSendPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted",      zeroSendPool.GetCountEntriesAccepted()));
    return obj;
}


Value blanknode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "stop" && strCommand != "stop-alias" && strCommand != "stop-many" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count"  && strCommand != "enforce"
            && strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" && strCommand != "outputs" && strCommand != "vote-many" && strCommand != "vote"))
        throw runtime_error(
                "blanknode \"command\"... ( \"passphrase\" )\n"
                "Set of commands to execute blanknode related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
                "\nAvailable commands:\n"
                "  count        - Print number of all known blanknodes (optional: 'enabled', 'both')\n"
                "  current      - Print info on current blanknode winner\n"
                "  debug        - Print blanknode status\n"
                "  genkey       - Generate new blanknodeprivkey\n"
                "  enforce      - Enforce blanknode payments\n"
                "  outputs      - Print blanknode compatible outputs\n"
                "  start        - Start blanknode configured in fantom.conf\n"
                "  start-alias  - Start single blanknode by assigned alias configured in blanknode.conf\n"
                "  start-many   - Start all blanknodes configured in blanknode.conf\n"
                "  stop         - Stop blanknode configured in fantom.conf\n"
                "  stop-alias   - Stop single blanknode by assigned alias configured in blanknode.conf\n"
                "  stop-many    - Stop all blanknodes configured in blanknode.conf\n"
                "  list         - Print list of all known blanknodes (see blanknodelist for more info)\n"
                "  list-conf    - Print blanknode.conf in JSON format\n"
                "  winners      - Print list of blanknode winners\n"
                "  vote-many    - Vote on a Fantom initiative\n"
                "  vote         - Vote on a Fantom initiative\n"
                );

    if (strCommand == "stop")
    {
        if(!fBlankNode) return "you must set blanknode=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        std::string errorMessage;
        if(!activeBlanknode.StopBlankNode(errorMessage)) {
        	return "stop failed: " + errorMessage;
        }
        pwalletMain->Lock();

        if(activeBlanknode.status == BLANKNODE_STOPPED) return "successfully stopped blanknode";
        if(activeBlanknode.status == BLANKNODE_NOT_CAPABLE) return "not capable blanknode";

        return "unknown";
    }

    if (strCommand == "stop-alias")
    {
	    if (params.size() < 2){
			throw runtime_error(
			"command needs at least 2 parameters\n");
	    }

	    std::string alias = params[1].get_str().c_str();

    	if(pwalletMain->IsLocked()) {
    		SecureString strWalletPass;
    	    strWalletPass.reserve(100);

			if (params.size() == 3){
				strWalletPass = params[2].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
        }

    	bool found = false;

		Object statusObj;
		statusObj.push_back(Pair("alias", alias));

    	BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
    		if(sne.getAlias() == alias) {
    			found = true;
    			std::string errorMessage;
    			bool result = activeBlanknode.StopBlankNode(sne.getIp(), sne.getPrivKey(), errorMessage);

				statusObj.push_back(Pair("result", result ? "successful" : "failed"));
    			if(!result) {
   					statusObj.push_back(Pair("errorMessage", errorMessage));
   				}
    			break;
    		}
    	}

    	if(!found) {
    		statusObj.push_back(Pair("result", "failed"));
    		statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
    	}

    	pwalletMain->Lock();
    	return statusObj;
    }

    if (strCommand == "stop-many")
    {
    	if(pwalletMain->IsLocked()) {
			SecureString strWalletPass;
			strWalletPass.reserve(100);

			if (params.size() == 2){
				strWalletPass = params[1].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
		}

		int total = 0;
		int successful = 0;
		int fail = 0;


		Object resultsObj;

		BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
			total++;

			std::string errorMessage;
			bool result = activeBlanknode.StopBlankNode(sne.getIp(), sne.getPrivKey(), errorMessage);

			Object statusObj;
			statusObj.push_back(Pair("alias", sne.getAlias()));
			statusObj.push_back(Pair("result", result ? "successful" : "failed"));

			if(result) {
				successful++;
			} else {
				fail++;
				statusObj.push_back(Pair("errorMessage", errorMessage));
			}

			resultsObj.push_back(Pair("status", statusObj));
		}
		pwalletMain->Lock();

		Object returnObj;
		returnObj.push_back(Pair("overall", "Successfully stopped " + boost::lexical_cast<std::string>(successful) + " blanknodes, failed to stop " +
				boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
		returnObj.push_back(Pair("detail", resultsObj));

		return returnObj;

    }

    if (strCommand == "list")
    {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return blanknodelist(newParams, fHelp);
    }

    if (strCommand == "count")
    {
        if (params.size() > 2){            
            throw runtime_error(
                "too many parameters\n");
        }

        if (params.size() == 2)
        {
            if(params[1] == "enabled") return snodeman.CountEnabled();
            if(params[1] == "both") return boost::lexical_cast<std::string>(snodeman.CountEnabled()) + " / " + boost::lexical_cast<std::string>(snodeman.size());        
        }
        return snodeman.size();
    }

    if (strCommand == "start")
    {
        if(!fBlankNode) return "you must set blanknode=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        if(activeBlanknode.status != BLANKNODE_REMOTELY_ENABLED && activeBlanknode.status != BLANKNODE_IS_CAPABLE){
            activeBlanknode.status = BLANKNODE_NOT_PROCESSED; // TODO: consider better way
            std::string errorMessage;
            activeBlanknode.ManageStatus();
            pwalletMain->Lock();
        }

        if(activeBlanknode.status == BLANKNODE_REMOTELY_ENABLED) return "blanknode started remotely";
        if(activeBlanknode.status == BLANKNODE_INPUT_TOO_NEW) return "blanknode input must have at least 15 confirmations";
        if(activeBlanknode.status == BLANKNODE_STOPPED) return "blanknode is stopped";
        if(activeBlanknode.status == BLANKNODE_IS_CAPABLE) return "successfully started blanknode";
        if(activeBlanknode.status == BLANKNODE_NOT_CAPABLE) return "not capable blanknode: " + activeBlanknode.notCapableReason;
        if(activeBlanknode.status == BLANKNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        return "unknown";
    }

    if (strCommand == "start-alias")
    {
	    if (params.size() < 2){
			throw runtime_error(
			"command needs at least 2 parameters\n");
	    }

	    std::string alias = params[1].get_str().c_str();

    	if(pwalletMain->IsLocked()) {
    		SecureString strWalletPass;
    	    strWalletPass.reserve(100);

			if (params.size() == 3){
				strWalletPass = params[2].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
        }

    	bool found = false;

		Object statusObj;
		statusObj.push_back(Pair("alias", alias));

    	BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
    		if(sne.getAlias() == alias) {
    			found = true;
    			std::string errorMessage;
                std::string strDonateAddress = sne.getDonationAddress();
                std::string strDonationPercentage = sne.getDonationPercentage();

                bool result = activeBlanknode.Register(sne.getIp(), sne.getPrivKey(), sne.getTxHash(), sne.getOutputIndex(), strDonateAddress, strDonationPercentage, errorMessage);
  
      			statusObj.push_back(Pair("result", result ? "successful" : "failed"));
    			if(!result) {
					statusObj.push_back(Pair("errorMessage", errorMessage));
				}
    			break;
    		}
    	}

    	if(!found) {
    		statusObj.push_back(Pair("result", "failed"));
    		statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
    	}

    	pwalletMain->Lock();
    	return statusObj;

    }

    if (strCommand == "start-many")
    {
    	if(pwalletMain->IsLocked()) {
			SecureString strWalletPass;
			strWalletPass.reserve(100);

			if (params.size() == 2){
				strWalletPass = params[1].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
		}

		std::vector<CBlanknodeConfig::CBlanknodeEntry> snEntries;
		snEntries = blanknodeConfig.getEntries();

		int total = 0;
		int successful = 0;
		int fail = 0;

		Object resultsObj;

		BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
			total++;

			std::string errorMessage;
            std::string strDonateAddress = sne.getDonationAddress();
            std::string strDonationPercentage = sne.getDonationPercentage();

            bool result = activeBlanknode.Register(sne.getIp(), sne.getPrivKey(), sne.getTxHash(), sne.getOutputIndex(), strDonateAddress, strDonationPercentage, errorMessage);

			Object statusObj;
			statusObj.push_back(Pair("alias", sne.getAlias()));
			statusObj.push_back(Pair("result", result ? "succesful" : "failed"));

			if(result) {
				successful++;
			} else {
				fail++;
				statusObj.push_back(Pair("errorMessage", errorMessage));
			}

			resultsObj.push_back(Pair("status", statusObj));
		}
		pwalletMain->Lock();

		Object returnObj;
		returnObj.push_back(Pair("overall", "Successfully started " + boost::lexical_cast<std::string>(successful) + " blanknodes, failed to start " +
				boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
		returnObj.push_back(Pair("detail", resultsObj));

		return returnObj;
    }

    if (strCommand == "debug")
    {
        if(activeBlanknode.status == BLANKNODE_REMOTELY_ENABLED) return "blanknode started remotely";
        if(activeBlanknode.status == BLANKNODE_INPUT_TOO_NEW) return "blanknode input must have at least 15 confirmations";
        if(activeBlanknode.status == BLANKNODE_IS_CAPABLE) return "successfully started blanknode";
        if(activeBlanknode.status == BLANKNODE_STOPPED) return "blanknode is stopped";
        if(activeBlanknode.status == BLANKNODE_NOT_CAPABLE) return "not capable blanknode: " + activeBlanknode.notCapableReason;
        if(activeBlanknode.status == BLANKNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = activeBlanknode.GetBlankNodeVin(vin, pubkey, key);
        if(!found){
            return "Missing blanknode input, please look at the documentation for instructions on blanknode creation";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "create")
    {

        return "Not implemented yet, please look at the documentation for instructions on blanknode creation";
    }

    if (strCommand == "current")
    {
        CBlanknode* winner = snodeman.GetCurrentBlankNode(1);
        if(winner) {
            Object obj;
            CScript pubkey;
            pubkey.SetDestination(winner->pubkey.GetID());
            CTxDestination address1;
            ExtractDestination(pubkey, address1);
            CFantomAddress address2(address1);

            obj.push_back(Pair("IP:port",       winner->addr.ToString().c_str()));
            obj.push_back(Pair("protocol",      (int64_t)winner->protocolVersion));
            obj.push_back(Pair("vin",           winner->vin.prevout.hash.ToString().c_str()));
            obj.push_back(Pair("pubkey",        address2.ToString().c_str()));
            obj.push_back(Pair("lastseen",      (int64_t)winner->lastTimeSeen));
            obj.push_back(Pair("activeseconds", (int64_t)(winner->lastTimeSeen - winner->sigTime)));
            return obj;
        }

        return "unknown";
    }

    if (strCommand == "genkey")
    {
        CKey secret;
        secret.MakeNewKey(false);

        return CFantomSecret(secret).ToString();
    }

    if (strCommand == "winners")
    {
        Object obj;
        std::string strMode = "addr";
    
        if (params.size() >= 1) strMode = params[0].get_str();

        for(int nHeight = pindexBest->nHeight-10; nHeight < pindexBest->nHeight+20; nHeight++)
        {
            CScript payee;
            CTxIn vin;
            if(blanknodePayments.GetBlockPayee(nHeight, payee, vin)){
                CTxDestination address1;
                ExtractDestination(payee, address1);
                CFantomAddress address2(address1);
                if(strMode == "addr")
                    obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       address2.ToString().c_str()));

                if(strMode == "vin")
                    obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       vin.ToString().c_str()));
            } else {
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       ""));
            }
        }

        return obj;
    }

    if(strCommand == "enforce")
    {
        return (uint64_t)enforceBlanknodePaymentsTime;
    }

    if(strCommand == "connect")
    {
        std::string strAddress = "";
        if (params.size() == 2){
            strAddress = params[1].get_str().c_str();
        } else {
            throw runtime_error(
                "Blanknode address required\n");
        }

        CService addr = CService(strAddress);

        if(ConnectNode((CAddress)addr, NULL, true)){
            return "successfully connected";
        } else {
            return "error connecting";
        }
    }

    if(strCommand == "list-conf")
    {
    	std::vector<CBlanknodeConfig::CBlanknodeEntry> snEntries;
    	snEntries = blanknodeConfig.getEntries();

        Object resultObj;

        BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
    		Object snObj;
    		snObj.push_back(Pair("alias", sne.getAlias()));
    		snObj.push_back(Pair("address", sne.getIp()));
    		snObj.push_back(Pair("privateKey", sne.getPrivKey()));
    		snObj.push_back(Pair("txHash", sne.getTxHash()));
    		snObj.push_back(Pair("outputIndex", sne.getOutputIndex()));
            snObj.push_back(Pair("donationAddress", sne.getDonationAddress()));
    		snObj.push_back(Pair("donationPercent", sne.getDonationPercentage()));
            resultObj.push_back(Pair("blanknode", snObj));
    	}

    	return resultObj;
    }

    if (strCommand == "outputs"){
        // Find possible candidates
        vector<COutput> possibleCoins = activeBlanknode.SelectCoinsBlanknode();

        Object obj;
        BOOST_FOREACH(COutput& out, possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }

        return obj;

    }

    if(strCommand == "vote-many")
    {
        std::vector<CBlanknodeConfig::CBlanknodeEntry> snEntries;
        snEntries = blanknodeConfig.getEntries();

        if (params.size() != 2)
            throw runtime_error("You can only vote 'yay' or 'nay'");

        std::string vote = params[1].get_str().c_str();
        if(vote != "yay" && vote != "nay") return "You can only vote 'yay' or 'nay'";
        int nVote = 0;
        if(vote == "yay") nVote = 1;
        if(vote == "nay") nVote = -1;

        int success = 0;
        int failed = 0;

        Object resultObj;

        BOOST_FOREACH(CBlanknodeConfig::CBlanknodeEntry sne, blanknodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchBlankNodeSignature;
            std::string strBlankNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyBlanknode;
            CKey keyBlanknode;

            if(!zeroSendSigner.SetKey(sne.getPrivKey(), errorMessage, keyBlanknode, pubKeyBlanknode)){
                printf(" Error upon calling SetKey for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }
            
            CBlanknode* psn = snodeman.Find(pubKeyBlanknode);
            if(psn == NULL)
            {
                printf("Can't find blanknode by pubkey for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            std::string strMessage = psn->vin.ToString() + boost::lexical_cast<std::string>(nVote);

            if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchBlankNodeSignature, keyBlanknode)){
                printf(" Error upon calling SignMessage for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            if(!zeroSendSigner.VerifyMessage(pubKeyBlanknode, vchBlankNodeSignature, strMessage, errorMessage)){
                printf(" Error upon calling VerifyMessage for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            success++;

            //send to all peers
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                pnode->PushMessage("svote", psn->vin, vchBlankNodeSignature, nVote);
        }

        return("Voted successfully " + boost::lexical_cast<std::string>(success) + " time(s) and failed " + boost::lexical_cast<std::string>(failed) + " time(s).");
     }

    if(strCommand == "vote")
    {
        std::vector<CBlanknodeConfig::CBlanknodeEntry> snEntries;
        snEntries = blanknodeConfig.getEntries();

        if (params.size() != 2)
            throw runtime_error("You can only vote 'yay' or 'nay'");

        std::string vote = params[1].get_str().c_str();
        if(vote != "yay" && vote != "nay") return "You can only vote 'yay' or 'nay'";
        int nVote = 0;
        if(vote == "yay") nVote = 1;
        if(vote == "nay") nVote = -1;

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;
        CPubKey pubKeyBlanknode;
        CKey keyBlanknode;

        std::string errorMessage;
        std::vector<unsigned char> vchBlankNodeSignature;
        std::string strMessage = activeBlanknode.vin.ToString() + boost::lexical_cast<std::string>(nVote);

        if(!zeroSendSigner.SetKey(strBlankNodePrivKey, errorMessage, keyBlanknode, pubKeyBlanknode))
            return(" Error upon calling SetKey");

        if(!zeroSendSigner.SignMessage(strMessage, errorMessage, vchBlankNodeSignature, keyBlanknode))
            return(" Error upon calling SignMessage");

        if(!zeroSendSigner.VerifyMessage(pubKeyBlanknode, vchBlankNodeSignature, strMessage, errorMessage))
            return(" Error upon calling VerifyMessage");

        //send to all peers
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            pnode->PushMessage("svote", activeBlanknode.vin, vchBlankNodeSignature, nVote);

    }

    return Value::null;
}

Value blanknodelist(const Array& params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp ||
            (strMode != "status" && strMode != "vin" && strMode != "pubkey" && strMode != "lastseen" && strMode != "activeseconds" && strMode != "rank"
                 && strMode != "protocol" && strMode != "full" && strMode != "votes" && strMode != "donation" && strMode != "pose" && strMode != "lastpaid"))     {
        throw runtime_error(
                "blanknodelist ( \"mode\" \"filter\" )\n"
                "Get a list of blanknodes in different modes\n"
                "\nArguments:\n"
                "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                "2. \"filter\"    (string, optional) Filter results. Partial match by IP by default in all modes, additional matches in some modes\n"
                "\nAvailable modes:\n"
                "  activeseconds  - Print number of seconds blanknode recognized by the network as enabled\n"
                "  donation       - Show donation settings\n"
                "  full           - Print info in format 'status protocol pubkey vin lastseen activeseconds' (can be additionally filtered, partial match)\n"
                "  lastseen       - Print timestamp of when a blanknode was last seen on the network\n"
                "  pose           - Print Proof-of-Service score\n"
                "  protocol       - Print protocol of a blanknode (can be additionally filtered, exact match))\n"
                "  pubkey         - Print public key associated with a blanknode (can be additionally filtered, partial match)\n"
                "  rank           - Print rank of a blanknode based on current block\n"
                "  status         - Print blanknode status: ENABLED / EXPIRED / VIN_SPENT / REMOVE / POS_ERROR (can be additionally filtered, partial match)\n"
                "  vin            - Print vin associated with a blanknode (can be additionally filtered, partial match)\n"
                "  votes          - Print all blanknode votes for a Fantom initiative (can be additionally filtered, partial match)\n"
                "  lastpaid       - The last time a node was paid on the network\n"
                );
    }

    Object obj;
    if (strMode == "rank") {
        std::vector<pair<int, CBlanknode> > vBlanknodeRanks = snodeman.GetBlanknodeRanks(pindexBest->nHeight);
        BOOST_FOREACH(PAIRTYPE(int, CBlanknode)& s, vBlanknodeRanks) {
            std::string strVin = s.second.vin.prevout.ToStringShort();
            if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
            obj.push_back(Pair(strVin,       s.first));
        }
    } else {
        std::vector<CBlanknode> vBlanknodes = snodeman.GetFullBlanknodeVector();
        BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {
            std::string strVin = sn.vin.prevout.ToStringShort();
            if (strMode == "activeseconds") {
                if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)(sn.lastTimeSeen - sn.sigTime)));
            } else if (strMode == "donation") {
                CTxDestination address1;
                ExtractDestination(sn.donationAddress, address1);
                CFantomAddress address2(address1);

                if(strFilter !="" && address2.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;

                std::string strOut = "";

                if(sn.donationPercentage != 0){
                    strOut = address2.ToString().c_str();
                    strOut += ":";
                    strOut += boost::lexical_cast<std::string>(sn.donationPercentage);
                }
                obj.push_back(Pair(strVin,       strOut.c_str()));
            } else if (strMode == "full") {
                CScript pubkey;
                pubkey.SetDestination(sn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CFantomAddress address2(address1);

                std::ostringstream addrStream;
                addrStream << setw(21) << strVin;

                std::ostringstream stringStream;
                stringStream << setw(10) <<
                               sn.Status() << " " <<
                               sn.protocolVersion << " " <<
                               address2.ToString() << " " <<
                               sn.addr.ToString() << " " <<
                               sn.lastTimeSeen << " " << setw(8) <<
                               (sn.lastTimeSeen - sn.sigTime) << " " <<
                               sn.nLastPaid;
                std::string output = stringStream.str();
                stringStream << " " << strVin;
                if(strFilter !="" && stringStream.str().find(strFilter) == string::npos &&
                        strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(addrStream.str(), output));
            } else if (strMode == "lastseen") {
                if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)sn.lastTimeSeen));
            } else if (strMode == "protocol") {
                if(strFilter !="" && strFilter != boost::lexical_cast<std::string>(sn.protocolVersion) &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)sn.protocolVersion));
            } else if (strMode == "pubkey") {
                CScript pubkey;
                pubkey.SetDestination(sn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CFantomAddress address2(address1);

                if(strFilter !="" && address2.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       address2.ToString().c_str()));
            } else if (strMode == "pose") {
                if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
                std::string strOut = boost::lexical_cast<std::string>(sn.nScanningErrorCount);
                obj.push_back(Pair(strVin,       strOut.c_str()));
            } else if(strMode == "status") {
                std::string strStatus = sn.Status();
                if(strFilter !="" && strVin.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       strStatus.c_str()));
            } else if (strMode == "addr") {
                if(strFilter !="" && sn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       sn.addr.ToString().c_str()));
            } else if(strMode == "votes"){
                std::string strStatus = "ABSTAIN";

                //voting lasts 7 days, ignore the last vote if it was older than that
                if((GetAdjustedTime() - sn.lastVote) < (60*60*8))
                {
                    if(sn.nVote == -1) strStatus = "NAY";
                    if(sn.nVote == 1) strStatus = "YAY";
                }

                if(strFilter !="" && (strVin.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos)) continue;
                obj.push_back(Pair(strVin,       strStatus.c_str()));
            } else if(strMode == "lastpaid"){
                if(strFilter !="" && sn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,      (int64_t)sn.nLastPaid));
            }
        }
    }
    return obj;

}
