// Copyright (c) 2014-2015 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blanknode-payments.h"
#include "blanknodeman.h"
#include "zerosend.h"
#include "util.h"
#include "sync.h"
#include "addrman.h"
#include <boost/lexical_cast.hpp>

CCriticalSection cs_blanknodepayments;
/** Object for who's going to get paid on which blocks */
CBlanknodePayments blanknodePayments;
// keep track of blanknode votes I've seen
map<uint256, CBlanknodePaymentWinner> mapSeenBlanknodeVotes;

void ProcessMessageBlanknodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(IsInitialBlockDownload()) return;

    if (strCommand == "snget") {//Blanknode Payments Request Sync
        if(fLiteMode) return; //disable all zerosend/blanknode related functionality

        if(pfrom->HasFulfilledRequest("snget")) {
            LogPrintf("snget - peer already asked me for the list\n");
            Misbehaving(pfrom->GetId(), 20);
            return;
        }
       

        pfrom->FulfilledRequest("snget");
        blanknodePayments.Sync(pfrom);
        LogPrintf("snget - Sent blanknode winners to %s\n", pfrom->addr.ToString().c_str());
    }
    else if (strCommand == "snw") { //Blanknode Payments Declare Winner

        LOCK(cs_blanknodepayments);

        //this is required in litemode
        CBlanknodePaymentWinner winner;
        vRecv >> winner;

        if(pindexBest == NULL) return;

        CTxDestination address1;
        ExtractDestination(winner.payee, address1);
        CFantomAddress address2(address1);

        uint256 hash = winner.GetHash();
        if(mapSeenBlanknodeVotes.count(hash)) {
            if(fDebug) LogPrintf("snw - seen vote %s Addr %s Height %d bestHeight %d\n", hash.ToString().c_str(), address2.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);
            return;
        }

        if(winner.nBlockHeight < pindexBest->nHeight - 10 || winner.nBlockHeight > pindexBest->nHeight+20){
            LogPrintf("snw - winner out of range %s Addr %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), address2.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);
            return;
        }

        if(winner.vin.nSequence != std::numeric_limits<unsigned int>::max()){
            LogPrintf("snw - invalid nSequence\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        LogPrintf("snw - winning vote - Vin %s Addr %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), address2.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);
 
        if(!blanknodePayments.CheckSignature(winner)){
            LogPrintf("snw - invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSeenBlanknodeVotes.insert(make_pair(hash, winner));

        if(blanknodePayments.AddWinningBlanknode(winner)){
            blanknodePayments.Relay(winner);
        }
    }
}

bool CBlanknodePayments::CheckSignature(CBlanknodePaymentWinner& winner)
{
    //note: need to investigate why this is failing
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight) + winner.payee.ToString();
    std::string strPubKey = strMainPubKey ;
    CPubKey pubkey(ParseHex(strPubKey));

    std::string errorMessage = "";
    if(!zeroSendSigner.VerifyMessage(pubkey, winner.vchSig, strMessage, errorMessage)){
        return false;
    }

    return true;
}

bool CBlanknodePayments::Sign(CBlanknodePaymentWinner& winner)
{
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight) + winner.payee.ToString();

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!zeroSendSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CBlanknodePayments::Sign - ERROR: Invalid blanknodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if(!zeroSendSigner.SignMessage(strMessage, errorMessage, winner.vchSig, key2)) {
        LogPrintf("CBlanknodePayments::Sign - Sign message failed");
        return false;
    }

    if(!zeroSendSigner.VerifyMessage(pubkey2, winner.vchSig, strMessage, errorMessage)) {
        LogPrintf("CBlanknodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

uint64_t CBlanknodePayments::CalculateScore(uint256 blockHash, CTxIn& vin)
{
    uint256 n1 = blockHash;
    uint256 n2 = Hash(BEGIN(n1), END(n1));
    uint256 n3 = Hash(BEGIN(vin.prevout.hash), END(vin.prevout.hash));
    uint256 n4 = n3 > n2 ? (n3 - n2) : (n2 - n3);

    //printf(" -- CBlanknodePayments CalculateScore() n2 = %d \n", n2.Get64());
    //printf(" -- CBlanknodePayments CalculateScore() n3 = %d \n", n3.Get64());
    //printf(" -- CBlanknodePayments CalculateScore() n4 = %d \n", n4.Get64());

    return n4.Get64();
}

bool CBlanknodePayments::GetBlockPayee(int nBlockHeight, CScript& payee, CTxIn& vin)
{
    BOOST_FOREACH(CBlanknodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == nBlockHeight) {
            payee = winner.payee;
            vin = winner.vin;
            return true;
        }
    }

    return false;
}

bool CBlanknodePayments::GetWinningBlanknode(int nBlockHeight, CTxIn& vinOut)
{
    BOOST_FOREACH(CBlanknodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == nBlockHeight) {
            vinOut = winner.vin;
            return true;
        }
    }

    return false;
}

bool CBlanknodePayments::AddWinningBlanknode(CBlanknodePaymentWinner& winnerIn)
{
    uint256 blockHash = 0;
    if(!GetBlockHash(blockHash, winnerIn.nBlockHeight-576)) {
        return false;
    }

    winnerIn.score = CalculateScore(blockHash, winnerIn.vin);

    bool foundBlock = false;
    BOOST_FOREACH(CBlanknodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == winnerIn.nBlockHeight) {
            foundBlock = true;
            if(winner.score < winnerIn.score){
                winner.score = winnerIn.score;
                winner.vin = winnerIn.vin;
                winner.payee = winnerIn.payee;
                winner.vchSig = winnerIn.vchSig;

                mapSeenBlanknodeVotes.insert(make_pair(winnerIn.GetHash(), winnerIn));

                return true;
            }
        }
    }

    // if it's not in the vector
    if(!foundBlock){
        vWinning.push_back(winnerIn);
        mapSeenBlanknodeVotes.insert(make_pair(winnerIn.GetHash(), winnerIn));

        return true;
    }

    return false;
}

void CBlanknodePayments::CleanPaymentList()
{
    LOCK(cs_blanknodepayments);

    if(pindexBest == NULL) return;

    int nLimit = std::max(((int)snodeman.size())*2, 1000);

    vector<CBlanknodePaymentWinner>::iterator it;
    for(it=vWinning.begin();it<vWinning.end();it++){
        if(pindexBest->nHeight - (*it).nBlockHeight > nLimit){
            if(fDebug) LogPrintf("CBlanknodePayments::CleanPaymentList - Removing old blanknode payment - block %d\n", (*it).nBlockHeight);
            vWinning.erase(it);
            break;
        }
    }
}

bool CBlanknodePayments::ProcessBlock(int nBlockHeight)
{
    LOCK(cs_blanknodepayments);

    if(nBlockHeight <= nLastBlockHeight) return false;
    if(!enabled) return false;
    CBlanknodePaymentWinner newWinner;
    int nMinimumAge = snodeman.CountEnabled();
    CScript payeeSource;

    uint256 hash;
    if(!GetBlockHash(hash, nBlockHeight-10)) return false;
    unsigned int nHash;
    memcpy(&hash, &nHash, 2);

    LogPrintf(" ProcessBlock Start nHeight %d. \n", nBlockHeight);

    std::vector<CTxIn> vecLastPayments;
    BOOST_REVERSE_FOREACH(CBlanknodePaymentWinner& winner, vWinning)
    {
        //if we already have the same vin - we have one full payment cycle, break
        if(vecLastPayments.size() > nMinimumAge) break;
        vecLastPayments.push_back(winner.vin);
    }

    // pay to the oldest SN that still had no payment but its input is old enough and it was active long enough
    CBlanknode *psn = snodeman.FindOldestNotInVec(vecLastPayments, nMinimumAge);
    if(psn != NULL)
    {
        LogPrintf(" Found by FindOldestNotInVec \n");

        newWinner.score = 0;
        newWinner.nBlockHeight = nBlockHeight;
        newWinner.vin = psn->vin;
        psn->nLastPaid = GetAdjustedTime();

        if(psn->donationPercentage > 0 && (nHash % 100) <= (unsigned int)psn->donationPercentage) {
            newWinner.payee = psn->donationAddress;
        } else {
            newWinner.payee.SetDestination(psn->pubkey.GetID());
        }

        payeeSource.SetDestination(psn->pubkey.GetID());
    }

    //if we can't find new SN to get paid, pick the first active SN counting back from the end of vecLastPayments list
    if(newWinner.nBlockHeight == 0 && nMinimumAge > 0)
    {   
        LogPrintf(" Find by reverse \n");

        BOOST_REVERSE_FOREACH(CTxIn& vinLP, vecLastPayments)
        {
            CBlanknode* psn = snodeman.Find(vinLP);
            if(psn != NULL)
            {
                psn->Check();
                if(!psn->IsEnabled()) continue;

                newWinner.score = 0;
                newWinner.nBlockHeight = nBlockHeight;
                newWinner.vin = psn->vin;
                psn->nLastPaid = GetAdjustedTime();
                
                if(psn->donationPercentage > 0 && (nHash % 100) <= (unsigned int)psn->donationPercentage) {
                    newWinner.payee = psn->donationAddress;
                } else {
                    newWinner.payee.SetDestination(psn->pubkey.GetID());
                }

                payeeSource.SetDestination(psn->pubkey.GetID());

                break; // we found active SN
            }
        }
    }

    if(newWinner.nBlockHeight == 0) return false;

    CTxDestination address1;
    ExtractDestination(newWinner.payee, address1);
    CFantomAddress address2(address1);

    CTxDestination address3;
    ExtractDestination(payeeSource, address3);
    CFantomAddress address4(address3);

    LogPrintf("Winner payee %s nHeight %d vin source %s. \n", address2.ToString().c_str(), newWinner.nBlockHeight, address4.ToString().c_str());
 
    if(Sign(newWinner))
    {
        if(AddWinningBlanknode(newWinner))
        {
            Relay(newWinner);
            nLastBlockHeight = nBlockHeight;
            return true;
        }
    }

    return false;
}


void CBlanknodePayments::Relay(CBlanknodePaymentWinner& winner)
{
    CInv inv(MSG_BLANKNODE_WINNER, winner.GetHash());

    vector<CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        pnode->PushMessage("inv", vInv);
    }
}

void CBlanknodePayments::Sync(CNode* node)
{
    LOCK(cs_blanknodepayments);

    BOOST_FOREACH(CBlanknodePaymentWinner& winner, vWinning)
        if(winner.nBlockHeight >= pindexBest->nHeight-10 && winner.nBlockHeight <= pindexBest->nHeight + 20)
            node->PushMessage("snw", winner);
}


bool CBlanknodePayments::SetPrivKey(std::string strPrivKey)
{
    CBlanknodePaymentWinner winner;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(winner);

    if(CheckSignature(winner)){
        LogPrintf("CBlanknodePayments::SetPrivKey - Successfully initialized as blanknode payments master\n");
        enabled = true;
        return true;
    } else {
        return false;
    }
}
