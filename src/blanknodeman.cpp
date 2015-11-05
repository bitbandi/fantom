#include "blanknodeman.h"
#include "blanknode.h"
#include "activeblanknode.h"
#include "zerosend.h"
#include "core.h"
#include "util.h"
#include "addrman.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

CCriticalSection cs_process_message;

/** Blanknode manager */
CBlanknodeMan snodeman;

struct CompareValueOnly
{
    bool operator()(const pair<int64_t, CTxIn>& t1,
                    const pair<int64_t, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};
struct CompareValueOnlySN
{
    bool operator()(const pair<int64_t, CBlanknode>& t1,
                    const pair<int64_t, CBlanknode>& t2) const
    {
        return t1.first < t2.first;
    }
};

//
// CBlanknodeDB
//

CBlanknodeDB::CBlanknodeDB()
{
    pathSN = GetDataDir() / "sncache.dat";
    strMagicMessage = "BlanknodeCache";
}

bool CBlanknodeDB::Write(const CBlanknodeMan& snodemanToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize addresses, checksum data up to that point, then append checksum
    CDataStream ssBlanknodes(SER_DISK, CLIENT_VERSION);
    ssBlanknodes << strMagicMessage; // blanknode cache file specific magic message
    ssBlanknodes << FLATDATA(Params().MessageStart()); // network specific magic number
    ssBlanknodes << snodemanToSave;
    uint256 hash = Hash(ssBlanknodes.begin(), ssBlanknodes.end());
    ssBlanknodes << hash;

    // open output file, and associate with CAutoFile
    FILE *file = fopen(pathSN.string().c_str(), "wb");
    CAutoFile fileout = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("%s : Failed to open file %s", __func__, pathSN.string());

    // Write and commit header, data
    try {
        fileout << ssBlanknodes;
    }
    catch (std::exception &e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
//    FileCommit(fileout);
    fileout.fclose();

    LogPrintf("Written info to sncache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", snodemanToSave.ToString());

    return true;
}

CBlanknodeDB::ReadResult CBlanknodeDB::Read(CBlanknodeMan& snodemanToLoad)
{   
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE *file = fopen(pathSN.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!filein)
    {
        error("%s : Failed to open file %s", __func__, pathSN.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathSN);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssBlanknodes(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssBlanknodes.begin(), ssBlanknodes.end());
    if (hashIn != hashTmp)
    {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (blanknode cache file specific magic message) and ..

        ssBlanknodes >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp)
        {
            error("%s : Invalid blanknode cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        ssBlanknodes >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
        {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CBlanknodeMan object
        ssBlanknodes >> snodemanToLoad;
    }
    catch (std::exception &e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
        snodemanToLoad.Clear();
    }

    snodemanToLoad.CheckAndRemove(); // clean out expired
    LogPrintf("Loaded info from sncache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", snodemanToLoad.ToString());

    return Ok;
}

void DumpBlanknodes()
{
    int64_t nStart = GetTimeMillis();

    CBlanknodeDB sndb;
    CBlanknodeMan tempSnodeman;

    LogPrintf("Verifying sncache.dat format...\n");
    CBlanknodeDB::ReadResult readResult = sndb.Read(tempSnodeman);
    // there was an error and it was not an error on file openning => do not proceed
    if (readResult == CBlanknodeDB::FileError)
        LogPrintf("Missing blanknode cache file - sncache.dat, will try to recreate\n");
    else if (readResult != CBlanknodeDB::Ok)
    {
        LogPrintf("Error reading sncache.dat: ");
        if(readResult == CBlanknodeDB::IncorrectFormat)
            LogPrintf("magic is ok but data has invalid format, will try to recreate\n");
        else
        {
            LogPrintf("file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrintf("Writing info to sncache.dat...\n");
    sndb.Write(snodeman);

    LogPrintf("Blanknode dump finished  %dms\n", GetTimeMillis() - nStart);
}

CBlanknodeMan::CBlanknodeMan() 
{
    nSsqCount = 0;
}

bool CBlanknodeMan::Add(CBlanknode &sn)
{
    LOCK(cs);

    if (!sn.IsEnabled())
        return false;

    CBlanknode *psn = Find(sn.vin);

    if (psn == NULL)
    {
        if(fDebug) LogPrintf("CBlanknodeMan: Adding new Blanknode %s - %i now\n", sn.addr.ToString().c_str(), size() + 1);
        vBlanknodes.push_back(sn);
        return true;
    }

    return false;
}

void CBlanknodeMan::Check()
{
    LOCK(cs);

    BOOST_FOREACH(CBlanknode& sn, vBlanknodes)
        sn.Check();
}

void CBlanknodeMan::CheckAndRemove()
{
    LOCK(cs);

    Check();

    //remove inactive
    vector<CBlanknode>::iterator it = vBlanknodes.begin();
    while(it != vBlanknodes.end()){
        if((*it).activeState == CBlanknode::BLANKNODE_REMOVE || (*it).activeState == CBlanknode::BLANKNODE_VIN_SPENT){
            if(fDebug) LogPrintf("CBlanknodeMan: Removing inactive Blanknode %s - %i now\n", (*it).addr.ToString().c_str(), size() - 1);
            it = vBlanknodes.erase(it);
        } else {
            ++it;
        }
    }

    // check who's asked for the Blanknode list
    map<CNetAddr, int64_t>::iterator it1 = mAskedUsForBlanknodeList.begin();
    while(it1 != mAskedUsForBlanknodeList.end()){
        if((*it1).second < GetTime()) {
            mAskedUsForBlanknodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check who we asked for the Blanknode list
    it1 = mWeAskedForBlanknodeList.begin();
    while(it1 != mWeAskedForBlanknodeList.end()){
        if((*it1).second < GetTime()){
            mWeAskedForBlanknodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check which Blanknodes we've asked for
    map<COutPoint, int64_t>::iterator it2 = mWeAskedForBlanknodeListEntry.begin();
    while(it2 != mWeAskedForBlanknodeListEntry.end()){
        if((*it2).second < GetTime()){
            mWeAskedForBlanknodeListEntry.erase(it2++);
        } else {
            ++it2;
        }
    }

}

void CBlanknodeMan::Clear()
{
    LOCK(cs);
    vBlanknodes.clear();
    mAskedUsForBlanknodeList.clear();
    mWeAskedForBlanknodeList.clear();
    mWeAskedForBlanknodeListEntry.clear();
    nSsqCount = 0;
}

int CBlanknodeMan::CountEnabled()
{
    int i = 0;

    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {
        sn.Check();
        if(sn.IsEnabled()) i++;
    }

    return i;
}

int CBlanknodeMan::CountBlanknodesAboveProtocol(int protocolVersion)
{
    int i = 0;

    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {
        sn.Check();
        if(sn.protocolVersion < protocolVersion || !sn.IsEnabled()) continue;
        i++;
    }

    return i;
}

void CBlanknodeMan::SsegUpdate(CNode* pnode)
{
    LOCK(cs);

    std::map<CNetAddr, int64_t>::iterator it = mWeAskedForBlanknodeList.find(pnode->addr);
    if (it != mWeAskedForBlanknodeList.end())
    {
        if (GetTime() < (*it).second) {
            LogPrintf("sseg - we already asked %s for the list; skipping...\n", pnode->addr.ToString());
            return;
        }
    }
    pnode->PushMessage("sseg", CTxIn());
    int64_t askAgain = GetTime() + BLANKNODES_SSEG_SECONDS;
    mWeAskedForBlanknodeList[pnode->addr] = askAgain;
}

CBlanknode *CBlanknodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CBlanknode& sn, vBlanknodes)
    {
        if(sn.vin.prevout == vin.prevout)
            return &sn;
    }
    return NULL;
}

CBlanknode *CBlanknodeMan::Find(const CPubKey &pubKeyBlanknode)
{
    LOCK(cs);

    BOOST_FOREACH(CBlanknode& sn, vBlanknodes)
    {
        if(sn.pubkey2 == pubKeyBlanknode)
            return &sn;
    }
    return NULL;
}


CBlanknode* CBlanknodeMan::FindOldestNotInVec(const std::vector<CTxIn> &vVins, int nMinimumAge)
{
    LOCK(cs);

    CBlanknode *pOldestBlanknode = NULL;

    BOOST_FOREACH(CBlanknode &sn, vBlanknodes)
    {   
        sn.Check();
        if(!sn.IsEnabled()) continue;

        if(sn.GetBlanknodeInputAge() < nMinimumAge) continue;

        bool found = false;
        BOOST_FOREACH(const CTxIn& vin, vVins)
            if(sn.vin.prevout == vin.prevout)
            {   
                found = true;
                break;
            }

        if(found) continue;

        if(pOldestBlanknode == NULL || pOldestBlanknode->SecondsSincePayment() < sn.SecondsSincePayment()){
            pOldestBlanknode = &sn;
        }
    }

    return pOldestBlanknode;
}
CBlanknode *CBlanknodeMan::FindRandom()
{
    LOCK(cs);

    if(size() == 0) return NULL;

    return &vBlanknodes[GetRandInt(vBlanknodes.size())];
}

CBlanknode* CBlanknodeMan::GetCurrentBlankNode(int mod, int64_t nBlockHeight, int minProtocol)
{
    unsigned int score = 0;
    CBlanknode* winner = NULL;

    // scan for winner
    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {
        sn.Check();
        if(sn.protocolVersion < minProtocol || !sn.IsEnabled()) continue;

        // calculate the score for each blanknode
        uint256 n = sn.CalculateScore(mod, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        // determine the winner
        if(n2 > score){
            score = n2;
            winner = &sn;
        }
    }

    return winner;
}

int CBlanknodeMan::GetBlanknodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<unsigned int, CTxIn> > vecBlanknodeScores;

    //make sure we know about this block
    uint256 hash = 0;
    if(!GetBlockHash(hash, nBlockHeight)) return -1;

    // scan for winner
    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {

        if(sn.protocolVersion < minProtocol) continue;
        if(fOnlyActive) {
            sn.Check();
            if(!sn.IsEnabled()) continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecBlanknodeScores.push_back(make_pair(n2, sn.vin));
    }

    sort(vecBlanknodeScores.rbegin(), vecBlanknodeScores.rend(), CompareValueOnly());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CTxIn)& s, vecBlanknodeScores){
        rank++;
        if(s.second == vin) {
            return rank;
        }
    }

    return -1;
}

std::vector<pair<int, CBlanknode> > CBlanknodeMan::GetBlanknodeRanks(int64_t nBlockHeight, int minProtocol)
{
    std::vector<pair<unsigned int, CBlanknode> > vecBlanknodeScores;
    std::vector<pair<int, CBlanknode> > vecBlanknodeRanks;

    //make sure we know about this block
    uint256 hash = 0;
    if(!GetBlockHash(hash, nBlockHeight)) return vecBlanknodeRanks;

    // scan for winner
    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {

        sn.Check();

        if(sn.protocolVersion < minProtocol) continue;
        if(!sn.IsEnabled()) {
            continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecBlanknodeScores.push_back(make_pair(n2, sn));
    }

    sort(vecBlanknodeScores.rbegin(), vecBlanknodeScores.rend(), CompareValueOnlySN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CBlanknode)& s, vecBlanknodeScores){
        rank++;
        vecBlanknodeRanks.push_back(make_pair(rank, s.second));
    }

    return vecBlanknodeRanks;
}

CBlanknode* CBlanknodeMan::GetBlanknodeByRank(int nRank, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<unsigned int, CTxIn> > vecBlanknodeScores;

    // scan for winner
    BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {

        if(sn.protocolVersion < minProtocol) continue;
        if(fOnlyActive) {
            sn.Check();
            if(!sn.IsEnabled()) continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecBlanknodeScores.push_back(make_pair(n2, sn.vin));
    }

    sort(vecBlanknodeScores.rbegin(), vecBlanknodeScores.rend(), CompareValueOnly());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CTxIn)& s, vecBlanknodeScores){
        rank++;
        if(rank == nRank) {
            return Find(s.second);
        }
    }

    return NULL;
}

void CBlanknodeMan::ProcessBlanknodeConnections()
{
    LOCK(cs_vNodes);

    if(!zeroSendPool.pSubmittedToBlanknode) return;
    
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(zeroSendPool.pSubmittedToBlanknode->addr == pnode->addr) continue;

        if(pnode->fZeroSendMaster){
            LogPrintf("Closing Blanknode connection %s \n", pnode->addr.ToString().c_str());
            pnode->CloseSocketDisconnect();
        }
    }
}

void CBlanknodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

    if(fLiteMode) return; //disable all zerosend/blanknode related functionality
    if(IsInitialBlockDownload()) return;

    LOCK(cs_process_message);

    if (strCommand == "ssee") { //Zerosend Election Entry

        CTxIn vin;
        CService addr;
        CPubKey pubkey;
        CPubKey pubkey2;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        int count;
        int current;
        int64_t lastUpdated;
        int protocolVersion;
        CScript donationAddress;
        int donationPercentage;
        std::string strMessage;

        // 60022 and greater
        vRecv >> vin >> addr >> vchSig >> sigTime >> pubkey >> pubkey2 >> count >> current >> lastUpdated >> protocolVersion >> donationAddress >> donationPercentage;
 
        // make sure signature isn't in the future (past is OK)
        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("ssee - Signature rejected, too far into the future %s\n", vin.ToString().c_str());
            return;
        }

        bool isLocal = addr.IsRFC1918() || addr.IsLocal();

        std::string vchPubKey(pubkey.begin(), pubkey.end());
        std::string vchPubKey2(pubkey2.begin(), pubkey2.end());

        strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion)  + donationAddress.ToString() + boost::lexical_cast<std::string>(donationPercentage);

        if(donationPercentage < 0 || donationPercentage > 100){
            LogPrintf("ssee - donation percentage out of range %d\n", donationPercentage);
            return;     
        }

        if(protocolVersion < MIN_SN_PROTO_VERSION) {
            LogPrintf("ssee - ignoring outdated Blanknode %s protocol version %d\n", vin.ToString().c_str(), protocolVersion);
            return;
        }

        CScript pubkeyScript;
        pubkeyScript.SetDestination(pubkey.GetID());

        if(pubkeyScript.size() != 25) {
            LogPrintf("ssee - pubkey the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        CScript pubkeyScript2;
        pubkeyScript2.SetDestination(pubkey2.GetID());

        if(pubkeyScript2.size() != 25) {
            LogPrintf("ssee - pubkey2 the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if(!vin.scriptSig.empty()) {
            LogPrintf("dsee - Ignore Not Empty ScriptSig %s\n",vin.ToString().c_str());
            return;
        }

        std::string errorMessage = "";
        if(!zeroSendSigner.VerifyMessage(pubkey, vchSig, strMessage, errorMessage)){
            LogPrintf("ssee - Got bad Blanknode address signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        //search existing blanknode list, this is where we update existing blanknodes with new ssee broadcasts
        CBlanknode* psn = this->Find(vin);
        // if we are a blanknode but with undefined vin and this ssee is ours (matches our Blanknode privkey) then just skip this part
        if(psn != NULL && !(fBlankNode && activeBlanknode.vin == CTxIn() && pubkey2 == activeBlanknode.pubKeyBlanknode))
        {
            // count == -1 when it's a new entry
            //   e.g. We don't want the entry relayed/time updated when we're syncing the list
            // sn.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
            //   after that they just need to match
            if(count == -1 && psn->pubkey == pubkey && !psn->UpdatedWithin(BLANKNODE_MIN_SSEE_SECONDS)){
                psn->UpdateLastSeen();

                if(psn->sigTime < sigTime){ //take the newest entry
                    LogPrintf("ssee - Got updated entry for %s\n", addr.ToString().c_str());
                    psn->pubkey2 = pubkey2;
                    psn->sigTime = sigTime;
                    psn->sig = vchSig;
                    psn->protocolVersion = protocolVersion;
                    psn->addr = addr;
                    psn->donationAddress = donationAddress;
                    psn->donationPercentage = donationPercentage;
                    psn->Check();
                    if(psn->IsEnabled())
                        snodeman.RelayBlanknodeEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
                 }
            }

            return;
        }

        // make sure the vout that was signed is related to the transaction that spawned the blanknode
        //  - this is expensive, so it's only done once per blanknode
        if(!zeroSendSigner.IsVinAssociatedWithPubkey(vin, pubkey)) {
            LogPrintf("ssee - Got mismatched pubkey and vin\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if(fDebug) LogPrintf("ssee - Got NEW Blanknode entry %s\n", addr.ToString().c_str());

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckZeroSendPool()

        CValidationState state;
        CTransaction tx = CTransaction();
        CTxOut vout = CTxOut(BLANKNODE_COLLATERAL*COIN, zeroSendPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);
        if(AcceptableInputs(mempool, tx, false)){
            if(fDebug) LogPrintf("ssee - Accepted Blanknode entry %i %i\n", count, current);

            if(GetInputAge(vin) < BLANKNODE_MIN_CONFIRMATIONS){
                LogPrintf("ssee - Input must have least %d confirmations\n", BLANKNODE_MIN_CONFIRMATIONS);
                Misbehaving(pfrom->GetId(), 20);
                return;
            }

            // verify that sig time is legit in past
            // should be at least not earlier than block when 1000 FNX tx got BLANKNODE_MIN_CONFIRMATIONS
            uint256 hashBlock = 0;
            GetTransaction(vin.prevout.hash, tx, hashBlock);
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
            if (mi != mapBlockIndex.end() && (*mi).second)
            {
                CBlockIndex* pSNIndex = (*mi).second; // block for 1000 FNX tx -> 1 confirmation
                CBlockIndex* pConfIndex = FindBlockByHeight((pSNIndex->nHeight + BLANKNODE_MIN_CONFIRMATIONS - 1)); // block where tx got BLANKNODE_MIN_CONFIRMATIONS
               if(pConfIndex->GetBlockTime() > sigTime)
               {
                    LogPrintf("ssee - Bad sigTime %d for Blanknode %20s %105s (%i conf block is at %d)\n",
                              sigTime, addr.ToString(), vin.ToString(), BLANKNODE_MIN_CONFIRMATIONS, pConfIndex->GetBlockTime());
                    return;
                }
            }

            // use this as a peer
            addrman.Add(CAddress(addr), pfrom->addr, 2*60*60);

            //doesn't support multisig addresses
            if(donationAddress.IsPayToScriptHash()){
                donationAddress = CScript();
                donationPercentage = 0;
            }

            // add our blanknode
            CBlanknode sn(addr, vin, pubkey, vchSig, sigTime, pubkey2, protocolVersion, donationAddress, donationPercentage);
            sn.UpdateLastSeen(lastUpdated);
            this->Add(sn);

            // if it matches our blanknodeprivkey, then we've been remotely activated
            if(pubkey2 == activeBlanknode.pubKeyBlanknode && protocolVersion == PROTOCOL_VERSION){
                activeBlanknode.EnableHotColdBlankNode(vin, addr);
            }

            if(count == -1 && !isLocal)
                snodeman.RelayBlanknodeEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
 
        } else {
            LogPrintf("ssee - Rejected Blanknode entry %s\n", addr.ToString().c_str());

            int nDoS = 0;
            if (state.IsInvalid(nDoS))
            {
                LogPrintf("ssee - %s from %s %s was not accepted into the memory pool\n", tx.GetHash().ToString().c_str(),
                    pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str());
                if (nDoS > 0)
                    Misbehaving(pfrom->GetId(), nDoS);
            }
        }
    }

    else if (strCommand == "sseep") { //ZeroSend Election Entry Ping

        CTxIn vin;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        bool stop;
        vRecv >> vin >> vchSig >> sigTime >> stop;

        //LogPrintf("sseep - Received: vin: %s sigTime: %lld stop: %s\n", vin.ToString().c_str(), sigTime, stop ? "true" : "false");

        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("sseep - Signature rejected, too far into the future %s\n", vin.ToString().c_str());
            return;
        }

        if (sigTime <= GetAdjustedTime() - 60 * 60) {
            LogPrintf("sseep - Signature rejected, too far into the past %s - %d %d \n", vin.ToString().c_str(), sigTime, GetAdjustedTime());
            return;
        }

        // see if we have this blanknode
        CBlanknode* psn = this->Find(vin);
        if(psn != NULL && psn->protocolVersion >= MIN_SN_PROTO_VERSION)
        {
            // LogPrintf("sseep - Found corresponding sn for vin: %s\n", vin.ToString().c_str());
            // take this only if it's newer
            if(psn->lastSseep < sigTime)
            {
                std::string strMessage = psn->addr.ToString() + boost::lexical_cast<std::string>(sigTime) + boost::lexical_cast<std::string>(stop);

                std::string errorMessage = "";
                if(!zeroSendSigner.VerifyMessage(psn->pubkey2, vchSig, strMessage, errorMessage))
                {
                    LogPrintf("sseep - Got bad Blanknode address signature %s \n", vin.ToString().c_str());
                    //Misbehaving(pfrom->GetId(), 100);
                    return;
                }

                psn->lastSseep = sigTime;

                if(!psn->UpdatedWithin(BLANKNODE_MIN_SSEEP_SECONDS))
                {   
                    if(stop) psn->Disable();
                    else
                    {
                        psn->UpdateLastSeen();
                        psn->Check();
                        if(!psn->IsEnabled()) return;
                    }
                    snodeman.RelayBlanknodeEntryPing(vin, vchSig, sigTime, stop);
                }
            }
            return;
        }

        if(fDebug) LogPrintf("sseep - Couldn't find Blanknode entry %s\n", vin.ToString().c_str());

        std::map<COutPoint, int64_t>::iterator i = mWeAskedForBlanknodeListEntry.find(vin.prevout);
        if (i != mWeAskedForBlanknodeListEntry.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t) return; // we've asked recently
        }

        // ask for the ssee info once from the node that sent sseep

        LogPrintf("sseep - Asking source node for missing entry %s\n", vin.ToString().c_str());
        pfrom->PushMessage("sseg", vin);
        int64_t askAgain = GetTime() + BLANKNODE_MIN_SSEEP_SECONDS;
        mWeAskedForBlanknodeListEntry[vin.prevout] = askAgain;

    } else if (strCommand == "svote") { //Blanknode Vote

        CTxIn vin;
        vector<unsigned char> vchSig;
        int nVote;
        vRecv >> vin >> vchSig >> nVote;

        // see if we have this Blanknode
        CBlanknode* psn = this->Find(vin);
        if(psn != NULL)
        {
            if((GetAdjustedTime() - psn->lastVote) > (60*60))
            {
                std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nVote);

                std::string errorMessage = "";
                if(!zeroSendSigner.VerifyMessage(psn->pubkey2, vchSig, strMessage, errorMessage))
                {
                    LogPrintf("svote - Got bad Blanknode address signature %s \n", vin.ToString().c_str());
                    return;
                }

                psn->nVote = nVote;
                psn->lastVote = GetAdjustedTime();

                //send to all peers
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                    pnode->PushMessage("svote", vin, vchSig, nVote);
            }

            return;
        }

    } else if (strCommand == "sseg") { //Get blanknode list or specific entry

        CTxIn vin;
        vRecv >> vin;

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            if(!pfrom->addr.IsRFC1918() && Params().NetworkID() == CChainParams::MAIN)
            {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForBlanknodeList.find(pfrom->addr);
                if (i != mAskedUsForBlanknodeList.end())
                {
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("sseg - peer already asked me for the list\n");
                        return;
                    }
                }

                int64_t askAgain = GetTime() + BLANKNODES_SSEG_SECONDS;
                mAskedUsForBlanknodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int count = this->size();
        int i = 0;

        BOOST_FOREACH(CBlanknode& sn, vBlanknodes) {

            if(sn.addr.IsRFC1918()) continue; //local network

            if(sn.IsEnabled())
            {
                if(fDebug) LogPrintf("sseg - Sending blanknode entry - %s \n", sn.addr.ToString().c_str());
                if(vin == CTxIn()){
                    pfrom->PushMessage("ssee", sn.vin, sn.addr, sn.sig, sn.sigTime, sn.pubkey, sn.pubkey2, count, i, sn.lastTimeSeen, sn.protocolVersion, sn.donationAddress, sn.donationPercentage);
                } else if (vin == sn.vin) {
                    pfrom->PushMessage("ssee", sn.vin, sn.addr, sn.sig, sn.sigTime, sn.pubkey, sn.pubkey2, count, i, sn.lastTimeSeen, sn.protocolVersion, sn.donationAddress, sn.donationPercentage);
                    LogPrintf("sseg - Sent 1 Blanknode entries to %s\n", pfrom->addr.ToString().c_str());
                    return;
                }
                i++;
            }
        }

        LogPrintf("sseg - Sent %d Blanknode entries to %s\n", i, pfrom->addr.ToString().c_str());
    }

}

void CBlanknodeMan::RelayBlanknodeEntry(const CTxIn vin, const CService addr, const std::vector<unsigned char> vchSig, const int64_t nNow, const CPubKey pubkey, const CPubKey pubkey2, const int count, const int current, const int64_t lastUpdated, const int protocolVersion, CScript donationAddress, int donationPercentage)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("ssee", vin, addr, vchSig, nNow, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
}

void CBlanknodeMan::RelayBlanknodeEntryPing(const CTxIn vin, const std::vector<unsigned char> vchSig, const int64_t nNow, const bool stop)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("sseep", vin, vchSig, nNow, stop);
}

void CBlanknodeMan::Remove(CTxIn vin)
{
    LOCK(cs);

    vector<CBlanknode>::iterator it = vBlanknodes.begin();
    while(it != vBlanknodes.end()){
        if((*it).vin == vin){
            if(fDebug) LogPrintf("CBlanknodeMan: Removing Blanknode %s - %i now\n", (*it).addr.ToString().c_str(), size() - 1);
            vBlanknodes.erase(it);
            break;
        }
    }
}

std::string CBlanknodeMan::ToString() const
{
    std::ostringstream info;

    info << "blanknodes: " << (int)vBlanknodes.size() <<
            ", peers who asked us for Blanknode list: " << (int)mAskedUsForBlanknodeList.size() <<
            ", peers we asked for Blanknode list: " << (int)mWeAskedForBlanknodeList.size() <<
            ", entries in Blanknode list we asked for: " << (int)mWeAskedForBlanknodeListEntry.size() <<
            ", nSsqCount: " << (int)nSsqCount;
    return info.str();
}
