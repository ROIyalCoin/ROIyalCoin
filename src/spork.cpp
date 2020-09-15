// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2017 The PIVX developers
// Copyright (c) 2018-2020 The ROIyalCoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "spork.h"
#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "protocol.h"
#include "sync.h"
#include "sporkdb.h"
#include "util.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;
std::map<uint256, CSporkMessage> mapSporks;

std::map<int, CSporkMessage> mapSporksActive;

// PIVX: on startup load spork values from previous session if they exist in the sporkDB
void LoadSporksFromDB()
{
    for (int i = SPORK_START; i <= SPORK_END; ++i) {
        // Since not all spork IDs are in use, we have to exclude undefined IDs
        std::string strSpork = sporkManager.GetSporkNameByID(i);
        if (strSpork == "Unknown") continue;

        // attempt to read spork from sporkDB
        CSporkMessage spork;
        if (!pSporkDB->ReadSpork(i, spork)) {
            LogPrintf("%s : no previous value for %s found in database\n", __func__, strSpork);
            continue;
        }

        // add spork to memory
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        std::time_t result = spork.nValue;
        // If SPORK Value is greater than 1,000,000 assume it's actually a Date and then convert to a more readable format
        if (spork.nValue > 1000000) {
            LogPrintf("%s : loaded spork %s with value %d : %s", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue,
                      std::ctime(&result));
        } else {
            LogPrintf("%s : loaded spork %s with value %d\n", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue);
        }
    }
}

void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all obfuscation/masternode related functionality

    if (strCommand == "spork") {
        //LogPrintf("ProcessSpork::spork\n");
        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == NULL) return;

        // Ignore spork messages about unknown/deleted sporks
        std::string strSpork = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (strSpork == "Unknown") return;

        uint256 hash = spork.GetHash();
        if (mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                if (fDebug) LogPrintf("spork - seen %s block %d \n", hash.ToString(), chainActive.Tip()->nHeight);
                return;
            } else {
                if (fDebug) LogPrintf("spork - got updated spork %s block %d \n", hash.ToString(), chainActive.Tip()->nHeight);
            }
        }

        LogPrintf("spork - new %s ID %d Time %d bestHeight %d\n", hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Tip()->nHeight);

        if (!sporkManager.CheckSignature(spork)) {
            LogPrintf("spork - invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);

        // PIVX: add to spork database.
        pSporkDB->WriteSpork(spork.nSporkID, spork);
    }
    if (strCommand == "getsporks") {
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            it++;
        }
    }
}


// grab the value of the spork on the network, or the default
int64_t GetSporkValue(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_SWIFTTX) r = SPORK_1_SWIFTTX_DEFAULT;
        if (nSporkID == SPORK_2_SWIFTTX_BLOCK_FILTERING) r = SPORK_2_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        if (nSporkID == SPORK_3_MAX_VALUE) r = SPORK_3_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT) r = SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_5_RECONSIDER_BLOCKS) r = SPORK_5_RECONSIDER_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_6_MN_WINNER_MINIMUM_AGE) r = SPORK_6_MN_WINNER_MINIMUM_AGE_DEFAULT;
        if (nSporkID == SPORK_7_MN_REBROADCAST_ENFORCEMENT) r = SPORK_7_MN_REBROADCAST_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_8_NEW_PROTOCOL_ENFORCEMENT) r = SPORK_8_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2) r = SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT;
        if (nSporkID == SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3) r = SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3_DEFAULT;

        if (r == -1) LogPrintf("GetSpork::Unknown Spork %d\n", nSporkID);
    }

    return r;
}

// grab the spork value, and see if it's off
bool IsSporkActive(int nSporkID)
{
    int64_t r = GetSporkValue(nSporkID);
    if (r == -1) return false;
    return r < GetTime();
}

void ReprocessBlocks(int nBlocks)
{
    std::map<uint256, int64_t>::iterator it = mapRejectedBlocks.begin();
    while (it != mapRejectedBlocks.end()) {
        //use a window twice as large as is usual for the nBlocks we want to reset
        if ((*it).second > GetTime() - (nBlocks * 60 * 5)) {
            BlockMap::iterator mi = mapBlockIndex.find((*it).first);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                LOCK(cs_main);

                CBlockIndex* pindex = (*mi).second;
                LogPrintf("ReprocessBlocks - %s\n", (*it).first.ToString());

                CValidationState state;
                ReconsiderBlock(state, pindex);
            }
        }
        ++it;
    }

    CValidationState state;
    {
        LOCK(cs_main);
        DisconnectBlocksAndReprocess(nBlocks);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }
}

bool CSporkManager::CheckSignature(CSporkMessage& spork)
{
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);
    CPubKey pubkeynew(ParseHex(Params().SporkKey()));
    std::string errorMessage = "";
    if (obfuScationSigner.VerifyMessage(pubkeynew, spork.vchSig, strMessage, errorMessage)) {
        return true;
    }

    return false;
}

bool CSporkManager::Sign(CSporkMessage& spork)
{
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!obfuScationSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrint("masternode","CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrint("masternode","CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrint("masternode","CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{
    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg)
{
    CInv inv(MSG_SPORK, msg.GetHash());
    RelayInv(inv);
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage msg;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(msg);

    if (CheckSignature(msg)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_1_SWIFTTX") return SPORK_1_SWIFTTX;
    if (strName == "SPORK_2_SWIFTTX_BLOCK_FILTERING") return SPORK_2_SWIFTTX_BLOCK_FILTERING;
    if (strName == "SPORK_3_MAX_VALUE") return SPORK_3_MAX_VALUE;
    if (strName == "SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT") return SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_5_RECONSIDER_BLOCKS") return SPORK_5_RECONSIDER_BLOCKS;
    if (strName == "SPORK_6_MN_WINNER_MINIMUM_AGE") return SPORK_6_MN_WINNER_MINIMUM_AGE;
    if (strName == "SPORK_7_MN_REBROADCAST_ENFORCEMENT") return SPORK_7_MN_REBROADCAST_ENFORCEMENT;
    if (strName == "SPORK_8_NEW_PROTOCOL_ENFORCEMENT") return SPORK_8_NEW_PROTOCOL_ENFORCEMENT;
    if (strName == "SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2") return SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2;
    if (strName == "SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3") return SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3;


    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    if (id == SPORK_1_SWIFTTX) return "SPORK_1_SWIFTTX";
    if (id == SPORK_2_SWIFTTX_BLOCK_FILTERING) return "SPORK_2_SWIFTTX_BLOCK_FILTERING";
    if (id == SPORK_3_MAX_VALUE) return "SPORK_3_MAX_VALUE";
    if (id == SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT) return "SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT";
    if (id == SPORK_5_RECONSIDER_BLOCKS) return "SPORK_5_RECONSIDER_BLOCKS";
    if (id == SPORK_6_MN_WINNER_MINIMUM_AGE) return "SPORK_6_MN_WINNER_MINIMUM_AGE";
    if (id == SPORK_7_MN_REBROADCAST_ENFORCEMENT) return "SPORK_7_MN_REBROADCAST_ENFORCEMENT";
    if (id == SPORK_8_NEW_PROTOCOL_ENFORCEMENT) return "SPORK_8_NEW_PROTOCOL_ENFORCEMENT";
    if (id == SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2) return "SPORK_9_NEW_PROTOCOL_ENFORCEMENT_2";
    if (id == SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3) return "SPORK_10_NEW_PROTOCOL_ENFORCEMENT_3";

    return "Unknown";
}
