// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <uint256.h>
#include <limits>
#include <map>
#include <string>
#include <primitives/block.h>

class CBlockIndex;

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height at which BIP16 becomes active */
    int BIP16Height;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nTargetTimespan / nTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];

    /** PoW/PoSt parameters */
    bool fAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    uint256 powLimit; 
    uint256 posLimit;
    int64_t nTargetSpacing;
    int64_t nTargetTimespan; // SolarCoin: PoST
    int64_t nTargetTimespan_Version1;
    int64_t nTargetTimespan_Version2;
    int64_t nInterval_Version2;
    int64_t nHeight_Version2;
    int64_t DifficultyAdjustmentInterval_V1() const { return nTargetTimespan_Version1 / nTargetSpacing; }
    int64_t DifficultyAdjustmentInterval_V2() const { return 15; }
    unsigned int nStakeMinAge;
    unsigned int nModifierInterval;

    // Fork params
    static const int FORK_HEIGHT_1 = 1177000;
    static const int FORK_HEIGHT_2 = 1440000;
    static const int LAST_POW_BLOCK = 835213;

    // PoS params
    static const int TWO_PERCENT_INT_HEIGHT = LAST_POW_BLOCK + 1000;
    static const int64_t INITIAL_COIN_SUPPLY = 34145512; // Used in calculating interest rate (97.990085882B are out of circulation)
    static constexpr double COIN_SUPPLY_GROWTH_RATE = 1.35;
    static constexpr int TWO_PERCENT_INT = 2.0;

    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    constexpr static const double PI = 3.1415926535;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
