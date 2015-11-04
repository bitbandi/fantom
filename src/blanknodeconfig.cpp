
#include "net.h"
#include "stormnodeconfig.h"
#include "util.h"
#include <base58.h>

CStormnodeConfig stormnodeConfig;

void CStormnodeConfig::add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex, std::string donationAddress, std::string donationPercent) {
    CStormnodeEntry cme(alias, ip, privKey, txHash, outputIndex, donationAddress, donationPercent);
    entries.push_back(cme);
}

bool CStormnodeConfig::read(std::string& strErr) {
    boost::filesystem::ifstream streamConfig(GetStormnodeConfigFile());
    if (!streamConfig.good()) {
        return true; // No stormnode.conf file is OK
    }

    for(std::string line; std::getline(streamConfig, line); )
    {
        if(line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string alias, ip, privKey, txHash, outputIndex, donation, donationAddress, donationPercent;
        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex >> donation)) {
            donationAddress = "";
            donationPercent = "";
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                strErr = "Could not parse masternode.conf line: " + line;
                streamConfig.close();
                return false;
            }
        } else {
            size_t pos = donation.find_first_of(":");
            if(pos == string::npos) { // no ":" found
                donationPercent = "100";
                donationAddress = donation;
            } else {
                donationPercent = donation.substr(pos + 1);
                donationAddress = donation.substr(0, pos);
            }
            CFantomAddress address(donationAddress);
            if (!address.IsValid()) {
                strErr = "Invalid Dash address in masternode.conf line: " + line;
                streamConfig.close();
                return false;
            }
        }
        

        /*if(Params().NetworkID() == CChainParams::MAIN){
            if(CService(ip).GetPort() != 31000) {
                strErr = "Invalid port detected in masternode.conf: " + line + " (must be 31000 for mainnet)";
                streamConfig.close();
                return false;
            }
        } else if(CService(ip).GetPort() == 31000) {
            strErr = "Invalid port detected in masternode.conf: " + line + " (31000 must be only on mainnet)";
            streamConfig.close();
            return false;
        }*/


        add(alias, ip, privKey, txHash, outputIndex, donationAddress, donationPercent);
    }

    streamConfig.close();
    return true;
}
