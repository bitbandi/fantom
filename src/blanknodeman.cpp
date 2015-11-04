#include "stormnodeman.h"
#include "stormnode.h"
#include "activestormnode.h"
#include "sandstorm.h"
#include "core.h"
#include "util.h"
#include "addrman.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

CCriticalSection cs_process_message;

/** Stormnode manager */
CStormnodeMan snodeman;

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
    bool operator()(const pair<int64_t, CStormnode>& t1,
                    const pair<int64_t, CStormnode>& t2) const
    {
        return t1.first < t2.first;
    }
};

//
// CStormnodeDB
//

CStormnodeDB::CStormnodeDB()
{
    pathSN = GetDataDir() / "sncache.dat";
    strMagicMessage = "StormnodeCache";
}

bool CStormnodeDB::Write(const CStormnodeMan& snodemanToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize addresses, checksum data up to that point, then append checksum
    CDataStream ssStormnodes(SER_DISK, CLIENT_VERSION);
    ssStormnodes << strMagicMessage; // stormnode cache file specific magic message
    ssStormnodes << FLATDATA(Params().MessageStart()); // network specific magic number
    ssStormnodes << snodemanToSave;
    uint256 hash = Hash(ssStormnodes.begin(), ssStormnodes.end());
    ssStormnodes << hash;

    // open output file, and associate with CAutoFile
    FILE *file = fopen(pathSN.string().c_str(), "wb");
    CAutoFile fileout = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("%s : Failed to open file %s", __func__, pathSN.string());

    // Write and commit header, data
    try {
        fileout << ssStormnodes;
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

CStormnodeDB::ReadResult CStormnodeDB::Read(CStormnodeMan& snodemanToLoad)
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

    CDataStream ssStormnodes(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssStormnodes.begin(), ssStormnodes.end());
    if (hashIn != hashTmp)
    {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (stormnode cache file specific magic message) and ..

        ssStormnodes >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp)
        {
            error("%s : Invalid stormnode cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        ssStormnodes >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
        {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CStormnodeMan object
        ssStormnodes >> snodemanToLoad;
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

void DumpStormnodes()
{
    int64_t nStart = GetTimeMillis();

    CStormnodeDB sndb;
    CStormnodeMan tempSnodeman;

    LogPrintf("Verifying sncache.dat format...\n");
    CStormnodeDB::ReadResult readResult = sndb.Read(tempSnodeman);
    // there was an error and it was not an error on file openning => do not proceed
    if (readResult == CStormnodeDB::FileError)
        LogPrintf("Missing stormnode cache file - sncache.dat, will try to recreate\n");
    else if (readResult != CStormnodeDB::Ok)
    {
        LogPrintf("Error reading sncache.dat: ");
        if(readResult == CStormnodeDB::IncorrectFormat)
            LogPrintf("magic is ok but data has invalid format, will try to recreate\n");
        else
        {
            LogPrintf("file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrintf("Writing info to sncache.dat...\n");
    sndb.Write(snodeman);

    LogPrintf("Stormnode dump finished  %dms\n", GetTimeMillis() - nStart);
}

CStormnodeMan::CStormnodeMan() 
{
    nSsqCount = 0;
}

bool CStormnodeMan::Add(CStormnode &sn)
{
    LOCK(cs);

    if (!sn.IsEnabled())
        return false;

    CStormnode *psn = Find(sn.vin);

    if (psn == NULL)
    {
        if(fDebug) LogPrintf("CStormnodeMan: Adding new Stormnode %s - %i now\n", sn.addr.ToString().c_str(), size() + 1);
        vStormnodes.push_back(sn);
        return true;
    }

    return false;
}

void CStormnodeMan::Check()
{
    LOCK(cs);

    BOOST_FOREACH(CStormnode& sn, vStormnodes)
        sn.Check();
}

void CStormnodeMan::CheckAndRemove()
{
    LOCK(cs);

    Check();

    //remove inactive
    vector<CStormnode>::iterator it = vStormnodes.begin();
    while(it != vStormnodes.end()){
        if((*it).activeState == CStormnode::STORMNODE_REMOVE || (*it).activeState == CStormnode::STORMNODE_VIN_SPENT){
            if(fDebug) LogPrintf("CStormnodeMan: Removing inactive Stormnode %s - %i now\n", (*it).addr.ToString().c_str(), size() - 1);
            it = vStormnodes.erase(it);
        } else {
            ++it;
        }
    }

    // check who's asked for the Stormnode list
    map<CNetAddr, int64_t>::iterator it1 = mAskedUsForStormnodeList.begin();
    while(it1 != mAskedUsForStormnodeList.end()){
        if((*it1).second < GetTime()) {
            mAskedUsForStormnodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check who we asked for the Stormnode list
    it1 = mWeAskedForStormnodeList.begin();
    while(it1 != mWeAskedForStormnodeList.end()){
        if((*it1).second < GetTime()){
            mWeAskedForStormnodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check which Stormnodes we've asked for
    map<COutPoint, int64_t>::iterator it2 = mWeAskedForStormnodeListEntry.begin();
    while(it2 != mWeAskedForStormnodeListEntry.end()){
        if((*it2).second < GetTime()){
            mWeAskedForStormnodeListEntry.erase(it2++);
        } else {
            ++it2;
        }
    }

}

void CStormnodeMan::Clear()
{
    LOCK(cs);
    vStormnodes.clear();
    mAskedUsForStormnodeList.clear();
    mWeAskedForStormnodeList.clear();
    mWeAskedForStormnodeListEntry.clear();
    nSsqCount = 0;
}

int CStormnodeMan::CountEnabled()
{
    int i = 0;

    BOOST_FOREACH(CStormnode& sn, vStormnodes) {
        sn.Check();
        if(sn.IsEnabled()) i++;
    }

    return i;
}

int CStormnodeMan::CountStormnodesAboveProtocol(int protocolVersion)
{
    int i = 0;

    BOOST_FOREACH(CStormnode& sn, vStormnodes) {
        sn.Check();
        if(sn.protocolVersion < protocolVersion || !sn.IsEnabled()) continue;
        i++;
    }

    return i;
}

void CStormnodeMan::SsegUpdate(CNode* pnode)
{
    LOCK(cs);

    std::map<CNetAddr, int64_t>::iterator it = mWeAskedForStormnodeList.find(pnode->addr);
    if (it != mWeAskedForStormnodeList.end())
    {
        if (GetTime() < (*it).second) {
            LogPrintf("sseg - we already asked %s for the list; skipping...\n", pnode->addr.ToString());
            return;
        }
    }
    pnode->PushMessage("sseg", CTxIn());
    int64_t askAgain = GetTime() + STORMNODES_SSEG_SECONDS;
    mWeAskedForStormnodeList[pnode->addr] = askAgain;
}

CStormnode *CStormnodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CStormnode& sn, vStormnodes)
    {
        if(sn.vin.prevout == vin.prevout)
            return &sn;
    }
    return NULL;
}

CStormnode *CStormnodeMan::Find(const CPubKey &pubKeyStormnode)
{
    LOCK(cs);

    BOOST_FOREACH(CStormnode& sn, vStormnodes)
    {
        if(sn.pubkey2 == pubKeyStormnode)
            return &sn;
    }
    return NULL;
}


CStormnode* CStormnodeMan::FindOldestNotInVec(const std::vector<CTxIn> &vVins, int nMinimumAge, int nMinimumActiveSeconds)
{
    LOCK(cs);

    CStormnode *pOldestStormnode = NULL;

    BOOST_FOREACH(CStormnode &sn, vStormnodes)
    {   
        sn.Check();
        if(!sn.IsEnabled()) continue;

        bool found = false;
        BOOST_FOREACH(const CTxIn& vin, vVins)
            if(sn.vin == vin)
            {   
                found = true;
                break;
            }

        if(found) continue;

        if(pOldestStormnode == NULL || pOldestStormnode->GetStormnodeInputAge() < sn.GetStormnodeInputAge()){
            pOldestStormnode = &sn;
        }
    }

    return pOldestStormnode;
}

CStormnode *CStormnodeMan::FindRandom()
{
    LOCK(cs);

    if(size() == 0) return NULL;

    return &vStormnodes[GetRandInt(vStormnodes.size())];
}

CStormnode* CStormnodeMan::GetCurrentStormNode(int mod, int64_t nBlockHeight, int minProtocol)
{
    unsigned int score = 0;
    CStormnode* winner = NULL;

    // scan for winner
    BOOST_FOREACH(CStormnode& sn, vStormnodes) {
        sn.Check();
        if(sn.protocolVersion < minProtocol || !sn.IsEnabled()) continue;

        // calculate the score for each stormnode
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

int CStormnodeMan::GetStormnodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<unsigned int, CTxIn> > vecStormnodeScores;

    //make sure we know about this block
    uint256 hash = 0;
    if(!GetBlockHash(hash, nBlockHeight)) return -1;

    // scan for winner
    BOOST_FOREACH(CStormnode& sn, vStormnodes) {

        if(sn.protocolVersion < minProtocol) continue;
        if(fOnlyActive) {
            sn.Check();
            if(!sn.IsEnabled()) continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecStormnodeScores.push_back(make_pair(n2, sn.vin));
    }

    sort(vecStormnodeScores.rbegin(), vecStormnodeScores.rend(), CompareValueOnly());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CTxIn)& s, vecStormnodeScores){
        rank++;
        if(s.second == vin) {
            return rank;
        }
    }

    return -1;
}

std::vector<pair<int, CStormnode> > CStormnodeMan::GetStormnodeRanks(int64_t nBlockHeight, int minProtocol)
{
    std::vector<pair<unsigned int, CStormnode> > vecStormnodeScores;
    std::vector<pair<int, CStormnode> > vecStormnodeRanks;

    //make sure we know about this block
    uint256 hash = 0;
    if(!GetBlockHash(hash, nBlockHeight)) return vecStormnodeRanks;

    // scan for winner
    BOOST_FOREACH(CStormnode& sn, vStormnodes) {

        sn.Check();

        if(sn.protocolVersion < minProtocol) continue;
        if(!sn.IsEnabled()) {
            continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecStormnodeScores.push_back(make_pair(n2, sn));
    }

    sort(vecStormnodeScores.rbegin(), vecStormnodeScores.rend(), CompareValueOnlySN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CStormnode)& s, vecStormnodeScores){
        rank++;
        vecStormnodeRanks.push_back(make_pair(rank, s.second));
    }

    return vecStormnodeRanks;
}

CStormnode* CStormnodeMan::GetStormnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<unsigned int, CTxIn> > vecStormnodeScores;

    // scan for winner
    BOOST_FOREACH(CStormnode& sn, vStormnodes) {

        if(sn.protocolVersion < minProtocol) continue;
        if(fOnlyActive) {
            sn.Check();
            if(!sn.IsEnabled()) continue;
        }

        uint256 n = sn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecStormnodeScores.push_back(make_pair(n2, sn.vin));
    }

    sort(vecStormnodeScores.rbegin(), vecStormnodeScores.rend(), CompareValueOnly());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CTxIn)& s, vecStormnodeScores){
        rank++;
        if(rank == nRank) {
            return Find(s.second);
        }
    }

    return NULL;
}

void CStormnodeMan::ProcessStormnodeConnections()
{
    LOCK(cs_vNodes);

    if(!sandStormPool.pSubmittedToStormnode) return;
    
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(sandStormPool.pSubmittedToStormnode->addr == pnode->addr) continue;

        if(pnode->fSandStormMaster){
            LogPrintf("Closing Stormnode connection %s \n", pnode->addr.ToString().c_str());
            pnode->CloseSocketDisconnect();
        }
    }
}

void CStormnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

    if(fLiteMode) return; //disable all sandstorm/stormnode related functionality
    if(IsInitialBlockDownload()) return;

    LOCK(cs_process_message);

    if (strCommand == "ssee") { //Sandstorm Election Entry

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

        // 60021 and greater
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
            LogPrintf("ssee - ignoring outdated Stormnode %s protocol version %d\n", vin.ToString().c_str(), protocolVersion);
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

        std::string errorMessage = "";
        if(!sandStormSigner.VerifyMessage(pubkey, vchSig, strMessage, errorMessage)){
            LogPrintf("ssee - Got bad Stormnode address signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        //search existing stormnode list, this is where we update existing stormnodes with new ssee broadcasts
        CStormnode* psn = this->Find(vin);
        // if we are a stormnode but with undefined vin and this ssee is ours (matches our Stormnode privkey) then just skip this part
        if(psn != NULL && !(fStormNode && activeStormnode.vin == CTxIn() && pubkey2 == activeStormnode.pubKeyStormnode))
        {
            // count == -1 when it's a new entry
            //   e.g. We don't want the entry relayed/time updated when we're syncing the list
            // sn.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
            //   after that they just need to match
            if(count == -1 && psn->pubkey == pubkey && !psn->UpdatedWithin(STORMNODE_MIN_SSEE_SECONDS)){
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
                        snodeman.RelayStormnodeEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
                 }
            }

            return;
        }

        // make sure the vout that was signed is related to the transaction that spawned the stormnode
        //  - this is expensive, so it's only done once per stormnode
        if(!sandStormSigner.IsVinAssociatedWithPubkey(vin, pubkey)) {
            LogPrintf("ssee - Got mismatched pubkey and vin\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if(fDebug) LogPrintf("ssee - Got NEW Stormnode entry %s\n", addr.ToString().c_str());

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckSandStormPool()

        CValidationState state;
        CTransaction tx = CTransaction();
        CTxOut vout = CTxOut(STORMNODE_COLLATERAL*COIN, sandStormPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);
        if(AcceptableInputs(mempool, tx, false)){
            if(fDebug) LogPrintf("ssee - Accepted Stormnode entry %i %i\n", count, current);

            if(GetInputAge(vin) < STORMNODE_MIN_CONFIRMATIONS){
                LogPrintf("ssee - Input must have least %d confirmations\n", STORMNODE_MIN_CONFIRMATIONS);
                Misbehaving(pfrom->GetId(), 20);
                return;
            }

            // verify that sig time is legit in past
            // should be at least not earlier than block when 1000 FNX tx got STORMNODE_MIN_CONFIRMATIONS
            uint256 hashBlock = 0;
            GetTransaction(vin.prevout.hash, tx, hashBlock);
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
            if (mi != mapBlockIndex.end() && (*mi).second)
            {
                CBlockIndex* pSNIndex = (*mi).second; // block for 1000 FNX tx -> 1 confirmation
                CBlockIndex* pConfIndex = FindBlockByHeight((pSNIndex->nHeight + STORMNODE_MIN_CONFIRMATIONS - 1)); // block where tx got STORMNODE_MIN_CONFIRMATIONS
               if(pConfIndex->GetBlockTime() > sigTime)
               {
                    LogPrintf("ssee - Bad sigTime %d for Stormnode %20s %105s (%i conf block is at %d)\n",
                              sigTime, addr.ToString(), vin.ToString(), STORMNODE_MIN_CONFIRMATIONS, pConfIndex->GetBlockTime());
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

            // add our stormnode
            CStormnode sn(addr, vin, pubkey, vchSig, sigTime, pubkey2, protocolVersion, donationAddress, donationPercentage);
            sn.UpdateLastSeen(lastUpdated);
            this->Add(sn);

            // if it matches our stormnodeprivkey, then we've been remotely activated
            if(pubkey2 == activeStormnode.pubKeyStormnode && protocolVersion == PROTOCOL_VERSION){
                activeStormnode.EnableHotColdStormNode(vin, addr);
            }

            if(count == -1 && !isLocal)
                snodeman.RelayStormnodeEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
 
        } else {
            LogPrintf("ssee - Rejected Stormnode entry %s\n", addr.ToString().c_str());

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

    else if (strCommand == "sseep") { //SandStorm Election Entry Ping

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

        // see if we have this stormnode
        CStormnode* psn = this->Find(vin);
        if(psn != NULL && psn->protocolVersion >= MIN_SN_PROTO_VERSION)
        {
            // LogPrintf("sseep - Found corresponding sn for vin: %s\n", vin.ToString().c_str());
            // take this only if it's newer
            if(psn->lastSseep < sigTime)
            {
                std::string strMessage = psn->addr.ToString() + boost::lexical_cast<std::string>(sigTime) + boost::lexical_cast<std::string>(stop);

                std::string errorMessage = "";
                if(!sandStormSigner.VerifyMessage(psn->pubkey2, vchSig, strMessage, errorMessage))
                {
                    LogPrintf("sseep - Got bad Stormnode address signature %s \n", vin.ToString().c_str());
                    //Misbehaving(pfrom->GetId(), 100);
                    return;
                }

                psn->lastSseep = sigTime;

                if(!psn->UpdatedWithin(STORMNODE_MIN_SSEEP_SECONDS))
                {   
                    if(stop) psn->Disable();
                    else
                    {
                        psn->UpdateLastSeen();
                        psn->Check();
                        if(!psn->IsEnabled()) return;
                    }
                    snodeman.RelayStormnodeEntryPing(vin, vchSig, sigTime, stop);
                }
            }
            return;
        }

        if(fDebug) LogPrintf("sseep - Couldn't find Stormnode entry %s\n", vin.ToString().c_str());

        std::map<COutPoint, int64_t>::iterator i = mWeAskedForStormnodeListEntry.find(vin.prevout);
        if (i != mWeAskedForStormnodeListEntry.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t) return; // we've asked recently
        }

        // ask for the ssee info once from the node that sent sseep

        LogPrintf("sseep - Asking source node for missing entry %s\n", vin.ToString().c_str());
        pfrom->PushMessage("sseg", vin);
        int64_t askAgain = GetTime() + STORMNODE_MIN_SSEEP_SECONDS;
        mWeAskedForStormnodeListEntry[vin.prevout] = askAgain;

    } else if (strCommand == "svote") { //Stormnode Vote

        CTxIn vin;
        vector<unsigned char> vchSig;
        int nVote;
        vRecv >> vin >> vchSig >> nVote;

        // see if we have this Stormnode
        CStormnode* psn = this->Find(vin);
        if(psn != NULL)
        {
            if((GetAdjustedTime() - psn->lastVote) > (60*60))
            {
                std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nVote);

                std::string errorMessage = "";
                if(!sandStormSigner.VerifyMessage(psn->pubkey2, vchSig, strMessage, errorMessage))
                {
                    LogPrintf("svote - Got bad Stormnode address signature %s \n", vin.ToString().c_str());
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

    } else if (strCommand == "sseg") { //Get stormnode list or specific entry

        CTxIn vin;
        vRecv >> vin;

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            if(!pfrom->addr.IsRFC1918() && Params().NetworkID() == CChainParams::MAIN)
            {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForStormnodeList.find(pfrom->addr);
                if (i != mAskedUsForStormnodeList.end())
                {
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("sseg - peer already asked me for the list\n");
                        return;
                    }
                }

                int64_t askAgain = GetTime() + STORMNODES_SSEG_SECONDS;
                mAskedUsForStormnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int count = this->size();
        int i = 0;

        BOOST_FOREACH(CStormnode& sn, vStormnodes) {

            if(sn.addr.IsRFC1918()) continue; //local network

            if(sn.IsEnabled())
            {
                if(fDebug) LogPrintf("sseg - Sending stormnode entry - %s \n", sn.addr.ToString().c_str());
                if(vin == CTxIn()){
                    pfrom->PushMessage("ssee", sn.vin, sn.addr, sn.sig, sn.sigTime, sn.pubkey, sn.pubkey2, count, i, sn.lastTimeSeen, sn.protocolVersion, sn.donationAddress, sn.donationPercentage);
                } else if (vin == sn.vin) {
                    pfrom->PushMessage("ssee", sn.vin, sn.addr, sn.sig, sn.sigTime, sn.pubkey, sn.pubkey2, count, i, sn.lastTimeSeen, sn.protocolVersion, sn.donationAddress, sn.donationPercentage);
                    LogPrintf("sseg - Sent 1 Stormnode entries to %s\n", pfrom->addr.ToString().c_str());
                    return;
                }
                i++;
            }
        }

        LogPrintf("sseg - Sent %d Stormnode entries to %s\n", i, pfrom->addr.ToString().c_str());
    }

}

void CStormnodeMan::RelayStormnodeEntry(const CTxIn vin, const CService addr, const std::vector<unsigned char> vchSig, const int64_t nNow, const CPubKey pubkey, const CPubKey pubkey2, const int count, const int current, const int64_t lastUpdated, const int protocolVersion, CScript donationAddress, int donationPercentage)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("ssee", vin, addr, vchSig, nNow, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
}

void CStormnodeMan::RelayStormnodeEntryPing(const CTxIn vin, const std::vector<unsigned char> vchSig, const int64_t nNow, const bool stop)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("sseep", vin, vchSig, nNow, stop);
}

void CStormnodeMan::Remove(CTxIn vin)
{
    LOCK(cs);

    vector<CStormnode>::iterator it = vStormnodes.begin();
    while(it != vStormnodes.end()){
        if((*it).vin == vin){
            if(fDebug) LogPrintf("CStormnodeMan: Removing Stormnode %s - %i now\n", (*it).addr.ToString().c_str(), size() - 1);
            vStormnodes.erase(it);
            break;
        }
    }
}

std::string CStormnodeMan::ToString() const
{
    std::ostringstream info;

    info << "stormnodes: " << (int)vStormnodes.size() <<
            ", peers who asked us for Stormnode list: " << (int)mAskedUsForStormnodeList.size() <<
            ", peers we asked for Stormnode list: " << (int)mWeAskedForStormnodeList.size() <<
            ", entries in Stormnode list we asked for: " << (int)mWeAskedForStormnodeListEntry.size() <<
            ", nSsqCount: " << (int)nSsqCount;
    return info.str();
}
