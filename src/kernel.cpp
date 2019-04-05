// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <validation.h>

#include <boost/assign/list_of.hpp>
#include <rpc/server.h>
#include <txdb.h>
#include <timedata.h>
#include <kernel.h>
#include <pow.h>

using namespace std;

int nAverageStakeWeightHeightCached = 0;
double dAverageStakeWeightCached = 0;

typedef std::map<int, unsigned int> MapModifierCheckpoints;

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
    boost::assign::map_list_of
        ( 0, 0x0fd11f4e7 )
       // ( 20700, 0x0ad1bd786 )
    ;

// Hard checkpoints of stake modifiers to ensure they are deterministic (testNet)
static std::map<int, unsigned int> mapStakeModifierCheckpointsTestNet =
    boost::assign::map_list_of
        ( 0, 0)
   ;

// Get time weight
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd, const Consensus::Params& params)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low

    return nIntervalEnd - nIntervalBeginning - params.nStakeMinAge;
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: non generated stake modifier found");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
int64_t GetStakeModifierSelectionIntervalSection(int nSection, const Consensus::Params& params)
{
    assert (nSection >= 0 && nSection < 64);
    return (params.nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
int64_t GetStakeModifierSelectionInterval(const Consensus::Params& params)
{
    int64_t nSelectionInterval = 0;
    for (int nSection=0; nSection < 64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection, params);
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(vector<pair<int64_t, arith_uint256> >& vSortedByTimestamp, map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev, const CBlockIndex** pindexSelected, const Consensus::Params& params)
{
    bool fSelected = false;
    uint256 hashBest = uint256();
    *pindexSelected = (const CBlockIndex*) 0;
    for (const PAIRTYPE(int64_t, arith_uint256)& item : vSortedByTimestamp)
    {
        uint256 hash = ArithToUint256(item.second);
        if (!mapBlockIndex.count(hash)) {
            LogPrintf("%s: failed to find block index for candidate block %s\n", __func__, hash.ToString().c_str());
            return false;
        }
        const CBlockIndex* pindex = mapBlockIndex[hash];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
        uint256 hashProof = pindex->nHeight > params.LAST_POW_BLOCK ? pindex->hashProofOfStake : pindex->GetBlockHash();
        if(pindex->nHeight > params.LAST_POW_BLOCK) {
            //LogPrintf("%s: Checking candidate block %s\n", __func__, hash.ToString().c_str());
            //LogPrintf("%s(): candidate hashproof=%s\n", __func__, hashProof.ToString().c_str());
        }
        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
        if (pindex->nHeight > params.LAST_POW_BLOCK)
            hashSelection = ArithToUint256(UintToArith256(hashSelection) >>= 32);
        if (fSelected && UintToArith256(hashSelection) < UintToArith256(hashBest))
        {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
    }
    if (fDebug && GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s(): selection hash=%s\n", __func__, hashBest.ToString().c_str());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every 
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexCurrent, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier, const Consensus::Params& params)
{
    const CBlockIndex* pindexPrev = pindexCurrent->pprev;
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;

    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime)) {
        LogPrintf("%s: unable to get last modifier\n", __func__);
        return false;
    }

    if (fDebug)
        LogPrintf("%s(): prev modifier=%016x time=%u\n", __func__, nStakeModifier, nModifierTime);

    if (nModifierTime / params.nModifierInterval >= pindexPrev->GetBlockTime() / params.nModifierInterval)
    {
        if (fDebug)
        {
            LogPrintf("%s(): no new interval keep current modifier: pindexPrev nHeight=%d nTime=%u\n", __func__, pindexPrev->nHeight, (unsigned int)pindexPrev->GetBlockTime());
        }
        return true;
    }

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, arith_uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * params.nModifierInterval / params.nTargetSpacing);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval(params);
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / params.nModifierInterval) * params.nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        // SolarCoin: The 'sort' function in bitcoin core behaves differently than it did in the legacy code.
        // If two blocks have the same timestamp (should not have happened but it did at 63967 and 63968 for example),
        // the hash is sorted "as an ascii string" data type instead of an arith_uint256 data type as in bitcoin core.
        // To solve this, I converted the vector uint256 hash to an arith_uint256 so we can do a valid sort comparison.
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), UintToArith256(pindex->GetBlockHash())));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // DEBUG: dump the vSortedByTimestamp
    if(nHeightFirstCandidate == 835323) {
    LogPrintf("%s(): vSortedByTimestamp:[",__func__);
    for (const PAIRTYPE(int64_t, arith_uint256)& item : vSortedByTimestamp) {
        LogPrintf("%s, ",item.second.GetHex().c_str());
    }
    LogPrintf("]\n");
}
    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound=0; nRound < min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound, params);
        // select a block from the candidates of current round (pindex is returned)
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex, params)) {
            LogPrintf("%s: unable to select block at round %d", __func__, nRound);
            return false;
        }
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);

        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (fDebug || GetBoolArg("-printstakemodifier", false))
            LogPrintf("%s(): selected modifier=0x%016x round %d stop=%ld height=%d entropybit=%d\n", __func__, nStakeModifierNew, nRound, nSelectionIntervalStop, pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (fDebug || GetBoolArg("-printstakemodifier", false))
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
            if (pindex->nHeight > params.LAST_POW_BLOCK)
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const PAIRTYPE(uint256, const CBlockIndex*)& item : mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->nHeight > params.LAST_POW_BLOCK ? "S" : "W");
        }
        LogPrintf("%s(): selection height [%d, %d] map %s\n", __func__, nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap.c_str());
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;

    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
static bool GetKernelStakeModifier(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake, const Consensus::Params& params)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval(params);
    int64_t nStakeModifierTargetTime = nStakeModifierTime + nStakeModifierSelectionInterval;

    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex *pNext = chainActive.Next(pindex);

    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < nStakeModifierTargetTime)
    {
        if (pNext == nullptr)
        {
            // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake || (pindex->GetBlockTime() + params.nStakeMinAge - nStakeModifierSelectionInterval > GetAdjustedTime()))
            {
                LogPrintf("%s: reached best block %s at height %d from block %s\n", __func__,
                    pindex->GetBlockHash().ToString().c_str(), pindex->nHeight, hashBlockFrom.ToString().c_str());
                return false;
            }
            else
            {
                if (fDebug || GetBoolArg("-printstakemodifier", false))
                    LogPrintf("%s: Nothing! Ending modifier=%u height=%d time=%u target=%u\n", __func__,
                        nStakeModifier, nStakeModifierHeight, nStakeModifierTime, nStakeModifierTargetTime);
                return false;
            }
        }
        pindex = pNext;
        pNext = chainActive.Next(pindex);
        //LogPrintf("DEBUG: pindex=%s\n", pindex->ToVerboseString());
        if (pindex && pindex->GeneratedStakeModifier())
        {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

// SolarCoin kernel protocol PoST
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.offset + txPrev.nTime + txPrev.vout.n + nTime) < bnTarget * nStakeTimeWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age owned, time factored by the current network strength.
// The reason this hash is chosen is the following:
//   nStakeModifier: scrambles computation to make it very difficult to precompute
//                  future proof-of-stake at the time of the coin's confirmation
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.offset: offset of txPrev inside block, to reduce the chance of
//                  nodes generating coinstake at the same time
//   txPrev.nTime: reduce the chance of nodes generating coinstake at the same
//                 time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//
bool CheckStakeTimeKernelHash(unsigned int nBits, const CBlock& blockFrom, unsigned int nTxOffset, const CTransaction& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, uint256& targetProofOfStake, CBlockIndex* pindexPrev, bool fPrintProofOfStake, const Consensus::Params& params)
{
    if (nTimeTx < txPrev.nTime) {  // Transaction timestamp violation
        LogPrintf("%s(): nTime violation\n", __func__);
        return false;
    }

    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    if (nTimeBlockFrom + params.nStakeMinAge > nTimeTx) { // Min age requirement
        LogPrintf("%s(): min age violation\n", __func__);
        return false;
    }

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    int64_t nValueIn = txPrev.vout[prevout.n].nValue;
    uint256 hashBlockFrom = blockFrom.GetHash();

    CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    int heightBlockFrom = pindexFrom->nHeight;
    int64_t timeWeight = GetWeight((int64_t)txPrev.nTime, (int64_t)nTimeTx, params);
    int64_t nCoinDayWeight = nValueIn * timeWeight / COIN / (24 * 60 * 60);

    // Stake Time factored weight
    int64_t factoredTimeWeight = GetStakeTimeFactoredWeight(timeWeight, nCoinDayWeight, pindexPrev, params);
    arith_uint256 bnStakeTimeWeight = arith_uint256(nValueIn) * factoredTimeWeight / COIN / (24 * 60 * 60);
    targetProofOfStake = ArithToUint256(bnStakeTimeWeight * bnTargetPerCoinDay);

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;

    if (!GetKernelStakeModifier(hashBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake, params)) {
        LogPrintf("%s(): GetKernelStakeModifier failed\n", __func__);
        return false;
    }

    ss << nStakeModifier;

    ss << nTimeBlockFrom << nTxOffset << txPrev.nTime << prevout.n << nTimeTx;
    hashProofOfStake = Hash(ss.begin(), ss.end());

    if (fPrintProofOfStake)
    {
        LogPrintf("%s(): using modifier %016x at height=%d timestamp=%u for block from height=%d timestamp=%u\n timeWeight=%d coinDayWeight=%d\n", __func__,
            nStakeModifier, nStakeModifierHeight,
            nStakeModifierTime,
            heightBlockFrom,
            blockFrom.GetBlockTime(),
            timeWeight, nCoinDayWeight);
        LogPrintf("%s(): check modifier=%016x nTimeBlockFrom=%u nTxOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s targetProof=%s\n", __func__,
            nStakeModifier,
            nTimeBlockFrom, nTxOffset, txPrev.nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString().c_str(), targetProofOfStake.ToString().c_str());
    }

    // SolarCoin: Version 3.14.2 Bug fix to get past PoW. Somehow this check passes in 2.1.8.
    if (heightBlockFrom > params.LAST_POW_BLOCK) {
        // Now check if proof-of-stake hash meets target protocol
        if (UintToArith256(hashProofOfStake) > UintToArith256(targetProofOfStake)) {
            LogPrintf("DEBUG: BUG: hashProofOfStake=%s > targetProofOfStake=%s (%08x > %08x) at height=%d\n", hashProofOfStake.ToString(), targetProofOfStake.ToString(), UintToArith256(hashProofOfStake).GetCompact(), UintToArith256(targetProofOfStake).GetCompact(), pindexFrom->nHeight);
                return false;
        }
    }
    return true;
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(const CTransaction& tx, unsigned int nBits, uint256& hashProofOfStake, uint256& targetProofOfStake, const Consensus::Params& params)
{
    if (!tx.IsCoinStake())
        return error("CheckProofOfStake() : called on non-coinstake %s", tx.GetHash().ToString().c_str());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txIn = tx.vin[0];
    const uint256& hashTx = txIn.prevout.hash;
    uint256 hashBlock;
    CTransactionRef txPrevRef;

    // First try finding the previous transaction in database
    unsigned int nTxOffset = 0;
    if (!GetTransaction(hashTx, txPrevRef, nTxOffset, params, hashBlock, true)) {
        LogPrintf("%s(): INFO: read txPrev failed\n", __func__);  // previous transaction not in main chain, may occur during initial download
        return false;
    }
    // Add the block header offset
    nTxOffset += 80;

    const CTransaction& txPrev = *txPrevRef;

    // TODO: Verify signature
    //if (!VerifySignature(txPrev, tx, 0, 0)) {
    //    LogPrintf("%s(): VerifySignature failed on coinstake %s", __func__, tx.GetHash().ToString().c_str());
    //    return false;
    //}

    // Read block header
    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
    if(!ReadBlockFromDisk(block, pblockindex, params, false)) {
        return fDebug ? error("CheckProofOfStake() : read block failed") : false; // unable to read block of previous transaction
    }

    if (!CheckStakeTimeKernelHash(nBits, block, nTxOffset, txPrev, txIn.prevout, tx.nTime, hashProofOfStake, targetProofOfStake, chainActive.Tip()->pprev, fDebug, params)) {
        LogPrintf("%s(): INFO: check kernel failed on coinstake %s, hashProof=%s\n", __func__, tx.GetHash().ToString(), hashProofOfStake.ToString()); // may occur during initial download or if behind on block chain sync
        return false;
    }
    return true;
}

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    // v0.3 protocol
    return (nTimeBlock == nTimeTx);
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex, const Consensus::Params& params)
{
    assert (pindex->pprev || pindex->GetBlockHash() == params.hashGenesisBlock);
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << pindex->hashProofOfStake << pindex->nStakeModifier;
    uint256 hashChecksum = Hash(ss.begin(), ss.end());
    hashChecksum = ArithToUint256(UintToArith256(hashChecksum) >>= (256 - 32));
    return hashChecksum.GetUint64(0);
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    MapModifierCheckpoints& checkpoints = (fTestNet ? mapStakeModifierCheckpointsTestNet : mapStakeModifierCheckpoints);
    if (checkpoints.count(nHeight))
        return nStakeModifierChecksum == checkpoints[nHeight];
    return true;
}

// get stake time factored weight for reward and hash PoST
int64_t GetStakeTimeFactoredWeight(int64_t timeWeight, int64_t nCoinDayWeight, CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    int64_t factoredTimeWeight;
    double weightFraction = (nCoinDayWeight+1) / GetAverageStakeWeight(pindexPrev, params);
    if (weightFraction > 0.45)
    {
        factoredTimeWeight = params.nStakeMinAge+1;
    }
    else
    {
        double stakeTimeFactor = pow(cos((params.PI*weightFraction)),2.0);
        factoredTimeWeight = stakeTimeFactor*timeWeight;
    }
    return factoredTimeWeight;
}

// get average stake weight of last 60 blocks PoST
double GetAverageStakeWeight(CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    double weightSum = 0.0, weightAve = 0.0;
    if (chainActive.Height() < 1)
        return weightAve;

    // Use cached weight if it's still valid
    if (pindexPrev->nHeight == nAverageStakeWeightHeightCached)
    {
        return dAverageStakeWeightCached;
    }
    nAverageStakeWeightHeightCached = pindexPrev->nHeight;

    int i;
    CBlockIndex* currentBlockIndex = pindexPrev;
    for (i = 0; currentBlockIndex && i < 60; i++)
    {
        double tempWeight = GetPoSKernelPS(currentBlockIndex, params);
        weightSum += tempWeight;
        currentBlockIndex = currentBlockIndex->pprev;
    }
    weightAve = (weightSum/i)+21;

    // Cache the stake weight value
    dAverageStakeWeightCached = weightAve;

    return weightAve;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are 
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool GetCoinAge(const CTransaction& tx, uint64_t& nCoinAge, const Consensus::Params& params)
{
    arith_uint256 bnCentSecond = 0;  // coin age in the unit of cent-seconds
    nCoinAge = 0;

    if (tx.IsCoinBase())
        return true;

    for (unsigned int i=0; i < tx.vin.size(); i++) {
        const CTxIn& txIn = tx.vin[i];
        const uint256& hashTx = txIn.prevout.hash;
        uint256 hashBlock;
        CTransactionRef txPrevRef;

        // First try finding the previous transaction in database
        unsigned int nTxOffset = 0;
        if (!GetTransaction(hashTx, txPrevRef, nTxOffset, params, hashBlock, true)) {
            LogPrintf("%s(): INFO: read txPrev failed\n", __func__);  // previous transaction not in main chain, may occur during initial download
            return false;
        }

        const CTransaction& txPrev = *txPrevRef;

        if (tx.nTime < txPrev.nTime)
            return false;  // Transaction timestamp violation

        // Read block header
        CBlock block;
        CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
        if(!ReadBlockFromDisk(block, pblockindex, params, false)) {
            return fDebug ? error("GetStakeTime() : read block failed") : false; // unable to read block of previous transaction
        }

        if (block.GetBlockTime() + params.nStakeMinAge > tx.nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txIn.prevout.n].nValue;
        bnCentSecond += arith_uint256(nValueIn) * (tx.nTime - txPrev.nTime) / CENT;

        if (fDebug || GetBoolArg("-printcoinage", false))
            LogPrintf("coin age nValueIn=%ld nTimeDiff=%ld bnCentSecond=%s\n", nValueIn, tx.nTime - txPrev.nTime, bnCentSecond.ToString());
    }

    arith_uint256 bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug || GetBoolArg("-printcoinage", false))
        LogPrintf("coin age bnCoinDay=%s\n", bnCoinDay.ToString());

    nCoinAge = ArithToUint256(bnCoinDay).GetUint64(0);
    return true;
}

// SolarCoin: total stake time spent in transaction that is accepted by the network, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches. PoST
bool GetStakeTime(const CTransaction& tx, uint64_t& nStakeTime, CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    arith_uint256 bnStakeTime = 0;  // coin age in the unit of cent-seconds
    nStakeTime = 0;

    if (tx.IsCoinBase())
        return true;

    for (unsigned int i=0; i < tx.vin.size(); i++) {
        const CTxIn& txIn = tx.vin[i];
        const uint256& hashTx = txIn.prevout.hash;
        uint256 hashBlock;
        CTransactionRef txPrevRef;

        // First try finding the previous transaction in database
        unsigned int nTxOffset = 0;
        if (!GetTransaction(hashTx, txPrevRef, nTxOffset, params, hashBlock, true)) {
            LogPrintf("%s(): INFO: read txPrev failed\n", __func__);  // previous transaction not in main chain, may occur during initial download
            return false;
        }

        const CTransaction& txPrev = *txPrevRef;

        if (tx.nTime < txPrev.nTime)
            return false;  // Transaction timestamp violation

        // Read block header
        CBlock block;
        CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
        if(!ReadBlockFromDisk(block, pblockindex, params, false)) {
            return fDebug ? error("GetStakeTime() : read block failed") : false; // unable to read block of previous transaction
        }

        if (block.GetBlockTime() + params.nStakeMinAge > tx.nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txIn.prevout.n].nValue;
        int64_t timeWeight = tx.nTime - txPrev.nTime;

        // Prevent really large stake weights by maxing at 30 days weight (2.0.2 restriction)
        if (timeWeight > 30 * (24 * 60 * 60))
            timeWeight = 30 * (24 * 60 * 60);

        int64_t CoinDay = nValueIn * timeWeight / COIN / (24 * 60 * 60);
        int64_t factoredTimeWeight = GetStakeTimeFactoredWeight(timeWeight, CoinDay, pindexPrev, params);
        bnStakeTime += arith_uint256(nValueIn) * factoredTimeWeight / COIN / (24 * 60 * 60);
        if (fDebug || GetBoolArg("-printcoinage", false))
            LogPrintf("  nValueIn=%ld timeWeight=%ld CoinDay=%ld factoredTimeWeight=%ld\n",
               nValueIn, timeWeight, CoinDay, factoredTimeWeight);
    }
    if (fDebug || GetBoolArg("-printcoinage", false))
        LogPrintf("stake time bnStakeTime=%s\n", bnStakeTime.ToString());

    nStakeTime = ArithToUint256(bnStakeTime).GetUint64(0);
    return true;
}

double GetPoSKernelPS(CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindexPrevStake = nullptr;

    while (pindexPrev && nStakesHandled < nPoSInterval)
    {
        // SolarCoin: CBlockIndex::IsProofOfStake is not valid during header download. Use height instead.
        if (pindexPrev->nHeight > params.LAST_POW_BLOCK)
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindexPrev) * 4294967296.0;
            if (pindexPrev->nHeight >= params.FORK_HEIGHT_2)
                // Bug fix: Prevent negative stake weight
                nStakesTime += std::max((int)(pindexPrevStake ? (pindexPrevStake->nTime - pindexPrev->nTime) : 0), 0);
            else
                nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindexPrev->nTime) : 0;
            pindexPrevStake = pindexPrev;
            nStakesHandled++;
        }
        pindexPrev = pindexPrev->pprev;
    }

   return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}
