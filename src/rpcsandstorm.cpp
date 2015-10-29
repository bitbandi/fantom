// Copyright (c) 2010-2015 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "core.h"
#include "db.h"
#include "init.h"
#include "activestormnode.h"
#include "stormnodeman.h"
#include "stormnodeconfig.h"
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

    // Parse darksilk address
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

Value sandstorm(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0)
        throw runtime_error(
            "sandstorm <darksilkaddress> <amount>\n"
            "darksilkaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if(params[0].get_str() == "auto"){
        if(fStormNode)
            return "SandStorm is not supported from stormnodes";

        sandStormPool.DoAutomaticDenominating();
        return "DoAutomaticDenominating";
    }

    if(params[0].get_str() == "reset"){
        sandStormPool.SetNull(true);
        sandStormPool.UnlockCoins();
        return "successfully reset sandstorm";
    }

    if (params.size() != 2)
        throw runtime_error(
            "sandstorm <darksilkaddress> <amount>\n"
            "darksilkaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    CDarkSilkAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid darksilk address");

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
    obj.push_back(Pair("current_stormnode",        snodeman.GetCurrentStormNode()->addr.ToString()));
    obj.push_back(Pair("state",        sandStormPool.GetState()));
    obj.push_back(Pair("entries",      sandStormPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted",      sandStormPool.GetCountEntriesAccepted()));
    return obj;
}


Value stormnode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "stop" && strCommand != "stop-alias" && strCommand != "stop-many" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count"  && strCommand != "enforce"
            && strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" && strCommand != "outputs" && strCommand != "vote-many" && strCommand != "vote"))
        throw runtime_error(
                "stormnode \"command\"... ( \"passphrase\" )\n"
                "Set of commands to execute stormnode related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
                "\nAvailable commands:\n"
                "  count        - Print number of all known stormnodes (optional: 'enabled', 'both')\n"
                "  current      - Print info on current stormnode winner\n"
                "  debug        - Print stormnode status\n"
                "  genkey       - Generate new stormnodeprivkey\n"
                "  enforce      - Enforce stormnode payments\n"
                "  outputs      - Print stormnode compatible outputs\n"
                "  start        - Start stormnode configured in darksilk.conf\n"
                "  start-alias  - Start single stormnode by assigned alias configured in stormnode.conf\n"
                "  start-many   - Start all stormnodes configured in stormnode.conf\n"
                "  stop         - Stop stormnode configured in darksilk.conf\n"
                "  stop-alias   - Stop single stormnode by assigned alias configured in stormnode.conf\n"
                "  stop-many    - Stop all stormnodes configured in stormnode.conf\n"
                "  list         - Print list of all known stormnodes (see stormnodelist for more info)\n"
                "  list-conf    - Print stormnode.conf in JSON format\n"
                "  winners      - Print list of stormnode winners\n"
                "  vote-many    - Vote on a DarkSilk initiative\n"
                "  vote         - Vote on a DarkSilk initiative\n"
                );

    if (strCommand == "stop")
    {
        if(!fStormNode) return "you must set stormnode=1 in the configuration";

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
        if(!activeStormnode.StopStormNode(errorMessage)) {
        	return "stop failed: " + errorMessage;
        }
        pwalletMain->Lock();

        if(activeStormnode.status == STORMNODE_STOPPED) return "successfully stopped stormnode";
        if(activeStormnode.status == STORMNODE_NOT_CAPABLE) return "not capable stormnode";

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

    	BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
    		if(sne.getAlias() == alias) {
    			found = true;
    			std::string errorMessage;
    			bool result = activeStormnode.StopStormNode(sne.getIp(), sne.getPrivKey(), errorMessage);

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

		BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
			total++;

			std::string errorMessage;
			bool result = activeStormnode.StopStormNode(sne.getIp(), sne.getPrivKey(), errorMessage);

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
		returnObj.push_back(Pair("overall", "Successfully stopped " + boost::lexical_cast<std::string>(successful) + " stormnodes, failed to stop " +
				boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
		returnObj.push_back(Pair("detail", resultsObj));

		return returnObj;

    }

    if (strCommand == "list")
    {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return stormnodelist(newParams, fHelp);
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
        if(!fStormNode) return "you must set stormnode=1 in the configuration";

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

        if(activeStormnode.status != STORMNODE_REMOTELY_ENABLED && activeStormnode.status != STORMNODE_IS_CAPABLE){
            activeStormnode.status = STORMNODE_NOT_PROCESSED; // TODO: consider better way
            std::string errorMessage;
            activeStormnode.ManageStatus();
            pwalletMain->Lock();
        }

        if(activeStormnode.status == STORMNODE_REMOTELY_ENABLED) return "stormnode started remotely";
        if(activeStormnode.status == STORMNODE_INPUT_TOO_NEW) return "stormnode input must have at least 15 confirmations";
        if(activeStormnode.status == STORMNODE_STOPPED) return "stormnode is stopped";
        if(activeStormnode.status == STORMNODE_IS_CAPABLE) return "successfully started stormnode";
        if(activeStormnode.status == STORMNODE_NOT_CAPABLE) return "not capable stormnode: " + activeStormnode.notCapableReason;
        if(activeStormnode.status == STORMNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

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

    	BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
    		if(sne.getAlias() == alias) {
    			found = true;
    			std::string errorMessage;
                std::string strDonateAddress = sne.getDonationAddress();
                std::string strDonationPercentage = sne.getDonationPercentage();

                bool result = activeStormnode.Register(sne.getIp(), sne.getPrivKey(), sne.getTxHash(), sne.getOutputIndex(), strDonateAddress, strDonationPercentage, errorMessage);
  
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

		std::vector<CStormnodeConfig::CStormnodeEntry> snEntries;
		snEntries = stormnodeConfig.getEntries();

		int total = 0;
		int successful = 0;
		int fail = 0;

		Object resultsObj;

		BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
			total++;

			std::string errorMessage;
            std::string strDonateAddress = sne.getDonationAddress();
            std::string strDonationPercentage = sne.getDonationPercentage();

            bool result = activeStormnode.Register(sne.getIp(), sne.getPrivKey(), sne.getTxHash(), sne.getOutputIndex(), strDonateAddress, strDonationPercentage, errorMessage);

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
		returnObj.push_back(Pair("overall", "Successfully started " + boost::lexical_cast<std::string>(successful) + " stormnodes, failed to start " +
				boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
		returnObj.push_back(Pair("detail", resultsObj));

		return returnObj;
    }

    if (strCommand == "debug")
    {
        if(activeStormnode.status == STORMNODE_REMOTELY_ENABLED) return "stormnode started remotely";
        if(activeStormnode.status == STORMNODE_INPUT_TOO_NEW) return "stormnode input must have at least 15 confirmations";
        if(activeStormnode.status == STORMNODE_IS_CAPABLE) return "successfully started stormnode";
        if(activeStormnode.status == STORMNODE_STOPPED) return "stormnode is stopped";
        if(activeStormnode.status == STORMNODE_NOT_CAPABLE) return "not capable stormnode: " + activeStormnode.notCapableReason;
        if(activeStormnode.status == STORMNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = activeStormnode.GetStormNodeVin(vin, pubkey, key);
        if(!found){
            return "Missing stormnode input, please look at the documentation for instructions on stormnode creation";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "create")
    {

        return "Not implemented yet, please look at the documentation for instructions on stormnode creation";
    }

    if (strCommand == "current")
    {
        CStormnode* winner = snodeman.GetCurrentStormNode(1);
        if(winner) {
            Object obj;
            CScript pubkey;
            pubkey.SetDestination(winner->pubkey.GetID());
            CTxDestination address1;
            ExtractDestination(pubkey, address1);
            CDarkSilkAddress address2(address1);

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

        return CDarkSilkSecret(secret).ToString();
    }

    if (strCommand == "winners")
    {
        Object obj;

        for(int nHeight = pindexBest->nHeight-10; nHeight < pindexBest->nHeight+20; nHeight++)
        {
            CScript payee;
            if(stormnodePayments.GetBlockPayee(nHeight, payee)){
                CTxDestination address1;
                ExtractDestination(payee, address1);
                CDarkSilkAddress address2(address1);
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       address2.ToString().c_str()));
            } else {
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       ""));
            }
        }

        return obj;
    }

    if(strCommand == "enforce")
    {
        return (uint64_t)enforceStormnodePaymentsTime;
    }

    if(strCommand == "connect")
    {
        std::string strAddress = "";
        if (params.size() == 2){
            strAddress = params[1].get_str().c_str();
        } else {
            throw runtime_error(
                "Stormnode address required\n");
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
    	std::vector<CStormnodeConfig::CStormnodeEntry> snEntries;
    	snEntries = stormnodeConfig.getEntries();

        Object resultObj;

        BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
    		Object snObj;
    		snObj.push_back(Pair("alias", sne.getAlias()));
    		snObj.push_back(Pair("address", sne.getIp()));
    		snObj.push_back(Pair("privateKey", sne.getPrivKey()));
    		snObj.push_back(Pair("txHash", sne.getTxHash()));
    		snObj.push_back(Pair("outputIndex", sne.getOutputIndex()));
            snObj.push_back(Pair("donationAddress", sne.getDonationAddress()));
    		snObj.push_back(Pair("donationPercent", sne.getDonationPercentage()));
            resultObj.push_back(Pair("stormnode", snObj));
    	}

    	return resultObj;
    }

    if (strCommand == "outputs"){
        // Find possible candidates
        vector<COutput> possibleCoins = activeStormnode.SelectCoinsStormnode();

        Object obj;
        BOOST_FOREACH(COutput& out, possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }

        return obj;

    }

    if(strCommand == "vote-many")
    {
        std::vector<CStormnodeConfig::CStormnodeEntry> snEntries;
        snEntries = stormnodeConfig.getEntries();

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

        BOOST_FOREACH(CStormnodeConfig::CStormnodeEntry sne, stormnodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchStormNodeSignature;
            std::string strStormNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyStormnode;
            CKey keyStormnode;

            if(!sandStormSigner.SetKey(sne.getPrivKey(), errorMessage, keyStormnode, pubKeyStormnode)){
                printf(" Error upon calling SetKey for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }
            
            CStormnode* psn = snodeman.Find(pubKeyStormnode);
            if(psn == NULL)
            {
                printf("Can't find stormnode by pubkey for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            std::string strMessage = psn->vin.ToString() + boost::lexical_cast<std::string>(nVote);

            if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchStormNodeSignature, keyStormnode)){
                printf(" Error upon calling SignMessage for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            if(!sandStormSigner.VerifyMessage(pubKeyStormnode, vchStormNodeSignature, strMessage, errorMessage)){
                printf(" Error upon calling VerifyMessage for %s\n", sne.getAlias().c_str());
                failed++;
                continue;
            }

            success++;

            //send to all peers
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                pnode->PushMessage("svote", psn->vin, vchStormNodeSignature, nVote);
        }

        return("Voted successfully " + boost::lexical_cast<std::string>(success) + " time(s) and failed " + boost::lexical_cast<std::string>(failed) + " time(s).");
     }

    if(strCommand == "vote")
    {
        std::vector<CStormnodeConfig::CStormnodeEntry> snEntries;
        snEntries = stormnodeConfig.getEntries();

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
        CPubKey pubKeyStormnode;
        CKey keyStormnode;

        std::string errorMessage;
        std::vector<unsigned char> vchStormNodeSignature;
        std::string strMessage = activeStormnode.vin.ToString() + boost::lexical_cast<std::string>(nVote);

        if(!sandStormSigner.SetKey(strStormNodePrivKey, errorMessage, keyStormnode, pubKeyStormnode))
            return(" Error upon calling SetKey");

        if(!sandStormSigner.SignMessage(strMessage, errorMessage, vchStormNodeSignature, keyStormnode))
            return(" Error upon calling SignMessage");

        if(!sandStormSigner.VerifyMessage(pubKeyStormnode, vchStormNodeSignature, strMessage, errorMessage))
            return(" Error upon calling VerifyMessage");

        //send to all peers
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            pnode->PushMessage("svote", activeStormnode.vin, vchStormNodeSignature, nVote);

    }

    return Value::null;
}

Value stormnodelist(const Array& params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp ||
            (strMode != "status" && strMode != "vin" && strMode != "pubkey" && strMode != "lastseen" && strMode != "activeseconds" && strMode != "rank"
                && strMode != "protocol" && strMode != "full" && strMode != "votes" && strMode != "donation" && strMode != "pose"))
    {
        throw runtime_error(
                "stormnodelist ( \"mode\" \"filter\" )\n"
                "Get a list of masternodes in different modes\n"
                "\nArguments:\n"
                "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                "2. \"filter\"    (string, optional) Filter results. Partial match by IP by default in all modes, additional matches in some modes\n"
                "\nAvailable modes:\n"
                "  activeseconds  - Print number of seconds stormnode recognized by the network as enabled\n"
                "  donation       - Show donation settings\n"
                "  full           - Print info in format 'status protocol pubkey vin lastseen activeseconds' (can be additionally filtered, partial match)\n"
                "  lastseen       - Print timestamp of when a stormnode was last seen on the network\n"
                "  pose           - Print Proof-of-Service score\n"
                "  protocol       - Print protocol of a stormnode (can be additionally filtered, exact match))\n"
                "  pubkey         - Print public key associated with a stormnode (can be additionally filtered, partial match)\n"
                "  rank           - Print rank of a stormnode based on current block\n"
                "  status         - Print stormnode status: ENABLED / EXPIRED / VIN_SPENT / REMOVE / POS_ERROR (can be additionally filtered, partial match)\n"
                "  vin            - Print vin associated with a stormnode (can be additionally filtered, partial match)\n"
                "  votes          - Print all stormnode votes for a DarkSilk initiative (can be additionally filtered, partial match)\n"
                );
    }

    Object obj;
    if (strMode == "rank") {
        std::vector<pair<int, CStormnode> > vStormnodeRanks = snodeman.GetStormnodeRanks(pindexBest->nHeight);
        BOOST_FOREACH(PAIRTYPE(int, CStormnode)& s, vStormnodeRanks) {
            std::string strAddr = s.second.addr.ToString();
            if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
            obj.push_back(Pair(strAddr,       s.first));
        }
    } else {
        std::vector<CStormnode> vStormnodes = snodeman.GetFullStormnodeVector();
        BOOST_FOREACH(CStormnode& sn, vStormnodes) {
            std::string strAddr = sn.addr.ToString();
            if (strMode == "activeseconds") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)(sn.lastTimeSeen - sn.sigTime)));
            } else if (strMode == "donation") {
                CTxDestination address1;
                ExtractDestination(sn.donationAddress, address1);
                CDarkSilkAddress address2(address1);

                if(strFilter !="" && address2.ToString().find(strFilter) == string::npos &&
                    strAddr.find(strFilter) == string::npos) continue;

                std::string strOut = "";

                if(sn.donationPercentage != 0){
                    strOut = address2.ToString().c_str();
                    strOut += ":";
                    strOut += boost::lexical_cast<std::string>(sn.donationPercentage);
                }
                obj.push_back(Pair(strAddr,       strOut.c_str()));
            } else if (strMode == "full") {
                CScript pubkey;
                pubkey.SetDestination(sn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CDarkSilkAddress address2(address1);

                std::ostringstream addrStream;
                addrStream << setw(21) << strAddr;

                std::ostringstream stringStream;
                stringStream << setw(10) <<
                               sn.Status() << " " <<
                               sn.protocolVersion << " " <<
                               address2.ToString() << " " <<
                               sn.vin.prevout.hash.ToString() << " " <<
                               sn.lastTimeSeen << " " << setw(8) <<
                               (sn.lastTimeSeen - sn.sigTime);
                std::string output = stringStream.str();
                stringStream << " " << strAddr;
                if(strFilter !="" && stringStream.str().find(strFilter) == string::npos &&
                        strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(addrStream.str(), output));
            } else if (strMode == "lastseen") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)sn.lastTimeSeen));
            } else if (strMode == "protocol") {
                if(strFilter !="" && strFilter != boost::lexical_cast<std::string>(sn.protocolVersion) &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)sn.protocolVersion));
            } else if (strMode == "pubkey") {
                CScript pubkey;
                pubkey.SetDestination(sn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CDarkSilkAddress address2(address1);

                if(strFilter !="" && address2.ToString().find(strFilter) == string::npos &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       address2.ToString().c_str()));
            } else if (strMode == "pose") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                std::string strOut = boost::lexical_cast<std::string>(sn.nScanningErrorCount);
                obj.push_back(Pair(strAddr,       strOut.c_str()));
            } else if(strMode == "status") {
                std::string strStatus = sn.Status();
                if(strFilter !="" && strAddr.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       strStatus.c_str()));
            } else if (strMode == "vin") {
                if(strFilter !="" && sn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       sn.vin.prevout.hash.ToString().c_str()));
            } else if(strMode == "votes"){
                std::string strStatus = "ABSTAIN";

                //voting lasts 7 days, ignore the last vote if it was older than that
                if((GetAdjustedTime() - sn.lastVote) < (60*60*8))
                {
                    if(sn.nVote == -1) strStatus = "NAY";
                    if(sn.nVote == 1) strStatus = "YAY";
                }

                if(strFilter !="" && (strAddr.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos)) continue;
                obj.push_back(Pair(strAddr,       strStatus.c_str()));
            }
        }
    }
    return obj;

}