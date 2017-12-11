// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <validation.h>

#include <boost/assign/list_of.hpp>
#include <rpc/blockchain.h>
#include <txdb.h>
#include <timedata.h>
#include <kernel.h>
#include <post.h>

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
static bool SelectBlockFromCandidates(vector<pair<int64_t, uint256> >& vSortedByTimestamp, map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev, const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    uint256 hashBest = uint256();
    *pindexSelected = (const CBlockIndex*) 0;
    for (const PAIRTYPE(int64_t, uint256)& item : vSortedByTimestamp)
    {
        if (!mapBlockIndex.count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());
        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        uint256 hashProof = pindex->IsProofOfStake() ? pindex->hashProofOfStake : pindex->GetBlockHash();
        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake()) {
            hashSelection = ArithToUint256(UintToArith256(hashSelection) >>= 32);
        }
        if (fSelected && hashSelection < hashBest)
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
    if (fDebug && gArgs.GetBoolArg("-printstakemodifier", false))
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
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");

    if (fDebug)
        LogPrintf("%s(): prev modifier=%u time=%u\n", __func__, nStakeModifier, nModifierTime);

    if (nModifierTime / params.nModifierInterval >= pindexPrev->GetBlockTime() / params.nModifierInterval)
    {
        if (fDebug)
        {
            LogPrintf("%s(): no new interval keep current modifier: pindexPrev nHeight=%d nTime=%u\n", __func__, pindexPrev->nHeight, (unsigned int)pindexPrev->GetBlockTime());
        }
        return true;
    }

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * params.nModifierInterval / params.nTargetSpacing);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval(params);
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / params.nModifierInterval) * params.nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound, params);
        // select a block from the candidates of current round (pindex is returned)
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);

        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (fDebug && gArgs.GetBoolArg("-printstakemodifier", false))
            LogPrintf("%s(): selected round %d stop=%ld height=%d bit=%d\n", __func__, nRound, nSelectionIntervalStop, pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (fDebug && gArgs.GetBoolArg("-printstakemodifier", false))
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const PAIRTYPE(uint256, const CBlockIndex*)& item : mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake()? "S" : "W");
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
        if (!pNext)
        {
            // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake || (pindex->GetBlockTime() + params.nStakeMinAge - nStakeModifierSelectionInterval > GetAdjustedTime()))
            {
                return error("GetKernelStakeModifier() : reached best block %s at height %d from block %s",
                    pindex->GetBlockHash().ToString().c_str(), pindex->nHeight, hashBlockFrom.ToString().c_str());
            }
            else
            {
                if (fDebug && gArgs.GetBoolArg("-printstakemodifier", false))
                    LogPrintf("%s(): Nothing! Ending modifier height=%d time=%u target=%u\n", __func__,
                        nStakeModifierHeight, nStakeModifierTime, nStakeModifierTargetTime);
                return false;
            }
        }
        pindex = pNext;
        pNext = chainActive.Next(pindex);
        if (pindex->GeneratedStakeModifier())
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
    if (nTimeTx < txPrev.nTime)  // Transaction timestamp violation
        return error("CheckStakeTimeKernelHash() : nTime violation");

    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    if (nTimeBlockFrom + params.nStakeMinAge > nTimeTx) // Min age requirement
        return error("CheckStakeTimeKernelHash() : min age violation");

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    int64_t nValueIn = txPrev.vout[prevout.n].nValue;
    uint256 hashBlockFrom = blockFrom.GetHash();
    CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    int heightBlockFrom = pindexFrom->nHeight;
    int64_t timeWeight = GetWeight((int64_t)txPrev.nTime, (int64_t)nTimeTx, params);
    int64_t bnCoinDayWeight = nValueIn * timeWeight / COIN / (24 * 60 * 60);

    // Stake Time factored weight
    int64_t factoredTimeWeight = GetStakeTimeFactoredWeight(timeWeight, bnCoinDayWeight, pindexPrev, params);
    LogPrintf("DEBUG: CheckStakeTimeKernelHash() : nValueIn=%d factoredTimeWeight=%d.\n", nValueIn, factoredTimeWeight);
    arith_uint256 bnStakeTimeWeight = arith_uint256(nValueIn) * factoredTimeWeight / COIN / (24 * 60 * 60);
    int64_t stakeTimeWeight = ArithToUint256(bnStakeTimeWeight).GetUint64(0);
    targetProofOfStake = ArithToUint256(bnStakeTimeWeight * bnTargetPerCoinDay);
    LogPrintf("DEBUG: CheckStakeTimeKernelHash() : stakeTimeWeight=%d (bnStakeTimeWeight=%s * bnTargetPerCoinDay=%s) => targetProofOfStake=%s\n", stakeTimeWeight, bnStakeTimeWeight.ToString(), bnTargetPerCoinDay.ToString(), targetProofOfStake.GetHex());

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;

    if (!GetKernelStakeModifier(hashBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake, params))
        return false;

    LogPrintf("DEBUG: CheckStakeTimeKernelHash() : Got nStakeModifier=%d\n", nStakeModifier);

    // DEBUG: Loop to find the known hash
    for (int i=0; i<256; i++) {
        ss << nStakeModifier;

        ss << nTimeBlockFrom << nTxOffset << txPrev.nTime << prevout.n << nTimeTx;
        hashProofOfStake = Hash(ss.begin(), ss.end());
        LogPrintf("DEBUG: Trying nTxOffset=%d hashProofOfStake=%s\n", nTxOffset, hashProofOfStake.GetHex());
        if (UintToArith256(hashProofOfStake) <= UintToArith256(targetProofOfStake) ||
            UintToArith256(hashProofOfStake) == UintToArith256(uint256S("007aecf5f3d21fcb8b84826de4a89591c3da063d4312b3926c66319d561e1894"))) {
            LogPrintf("DEBUG: FOUND IT! : nTxOffset=%d hashProofOfStake=%s\n", nTxOffset, hashProofOfStake.GetHex());
            break;
        }
        nTxOffset++;
        ss.clear();
    }
    LogPrintf("DEBUG: CheckStakeTimeKernelHash() : Got hashProofOfStake=%s\n", hashProofOfStake.GetHex());

    if (fPrintProofOfStake)
    {
        LogPrintf("%s(): using modifier %u at height=%d timestamp=%u for block from height=%d timestamp=%u\n stakeTime=%d, coinDay=%d\n", __func__,
            nStakeModifier, nStakeModifierHeight,
            nStakeModifierTime,
            heightBlockFrom,
            blockFrom.GetBlockTime(),
            stakeTimeWeight, bnCoinDayWeight);
        LogPrintf("%s(): check modifier=%u nTimeBlockFrom=%u nTxOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
            nStakeModifier,
            nTimeBlockFrom, nTxOffset, txPrev.nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString().c_str());
    }

    // Now check if proof-of-stake hash meets target protocol
    if (UintToArith256(hashProofOfStake) > UintToArith256(targetProofOfStake))
        return false;

    if (fDebug && !fPrintProofOfStake)
    {
        LogPrintf("%s(): using modifier %u at height=%d timestamp=%u for block from height=%d timestamp=%u\n", __func__,
            nStakeModifier, nStakeModifierHeight,
            nStakeModifierTime,
            heightBlockFrom,
            blockFrom.GetBlockTime());
        LogPrintf("%s(): pass modifier=%u nTimeBlockFrom=%u nTxOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
            nStakeModifier,
            nTimeBlockFrom, nTxOffset, txPrev.nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString().c_str());
    }
    return true;
}

// TODO: This need to be called when processing a block
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
    if (!GetTransaction(hashTx, txPrevRef, nTxOffset, Params().GetConsensus(), hashBlock, true)) {
        LogPrintf("%s(): INFO: read txPrev failed\n", __func__);  // previous transaction not in main chain, may occur during initial download
        return false;
    }
    const CTransaction& txPrev = *txPrevRef;
    //nTxOffset += 80; // DEBUG: Offset + block header
    LogPrintf("DEBUG: TxPrev: nTxOffset=%u txPrev=%s\n", nTxOffset, txPrev.ToString());

    // Verify signature (TODO: Add SolarCoin script function.)
    //LogPrintf("DEBUG: Calling VerifySignature.\n");
    //if (!VerifySignature(txPrev, tx, 0, 0)) {
    //    LogPrintf("%s(): VerifySignature failed on coinstake %s", __func__, tx.GetHash().ToString().c_str());
    //    return false;
    //}

    // Read block header
    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
    if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus(), false)) {
        return fDebug ? error("CheckProofOfStake() : read block failed") : false; // unable to read block of previous transaction
    }
    LogPrintf("DEBUG: BlockPrev: block=%s\n", block.ToString());

    if (!CheckStakeTimeKernelHash(nBits, block, nTxOffset, txPrev, txIn.prevout, tx.nTime, hashProofOfStake, targetProofOfStake, chainActive.Tip()->pprev, fDebug, params)) {
        LogPrintf("%s(): INFO: check kernel failed on coinstake %s, hashProof=%s\n", __func__, tx.GetHash().ToString().c_str(), hashProofOfStake.ToString().c_str()); // may occur during initial download or if behind on block chain sync
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
int64_t GetStakeTimeFactoredWeight(int64_t timeWeight, int64_t bnCoinDayWeight, CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    int64_t factoredTimeWeight;
    double weightFraction = (bnCoinDayWeight+1) / GetAverageStakeWeight(pindexPrev, params);
    if (weightFraction > 0.45)
    {
        factoredTimeWeight = params.nStakeMinAge+1;
    }
    else
    {
        double stakeTimeFactor = pow(cos(params.PI*weightFraction),2.0);
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

double GetPoSKernelPS(CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindexPrevStake = nullptr;

    while (pindexPrev && nStakesHandled < nPoSInterval)
    {
        if (pindexPrev->IsProofOfStake())
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
