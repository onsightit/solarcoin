// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"
#include "chain.h"
#include "primitives/transaction.h"
#include "key.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "stdlib.h"
#include "timedata.h"

typedef std::vector<unsigned char> valtype;

void CBlockHeader::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = std::max(GetBlockTime(), GetAdjustedTime());
}

std::string CBlockHeader::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlockHeader(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce);
    return s.str();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u, vchBlockSig=%s)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size(),
        HexStr(vchBlockSig.begin(), vchBlockSig.end()).c_str());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

int64_t GetBlockWeight(const CBlock& block)
{
    // This implements the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}

/* TODO: Staking 

// solarcoin: attempt to generate suitable proof-of-stake
bool CBlock::SignBlock(CWallet& wallet, int64_t nFees)

{
    // if we are trying to sign
    //    something except proof-of-stake block template
    const CTransaction& tx = (const CTransaction&)vtx[0];
    if (!tx.vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // startup timestamp

    CKey key;
    CTransaction txCoinStake;

    int64_t nSearchTime = txCoinStake.nTime; // search to current time

    if (nSearchTime > nLastCoinStakeSearchTime)
    {
        if (wallet.CreateCoinStakeTime(wallet, nBits, nSearchTime - nLastCoinStakeSearchTime, nFees, txCoinStake, key))
        {
            if (txCoinStake.nTime >= max(pindexBest->GetMedianTimePast()+1, PastDrift(pindexBest->GetBlockTime())))
            {
                // make sure coinstake would meet timestamp protocol
                //    as it would be the same as the block timestamp
                vtx[0].nTime = nTime = txCoinStake.nTime;
                nTime = max(pindexBest->GetMedianTimePast()+1, GetMaxTransactionTime());
                nTime = max(GetBlockTime(), PastDrift(pindexBest->GetBlockTime()));

                // we have to make sure that we have no future timestamps in
                //    our transactions set
                for (vector<CTransaction>::iterator it = vtx.begin(); it != vtx.end();)
                    if (it->nTime > nTime) { it = vtx.erase(it); } else { ++it; }

                vtx.insert(vtx.begin() + 1, txCoinStake);
                // TODO: hashMerkleRoot = BuildMerkleTree();

                // append a signature to our block
                return key.Sign(GetHash(), vchBlockSig);
            }
        }
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime = nSearchTime;
    }

    return false;
}

// ppcoin: check block signature
bool CBlock::CheckBlockSignature(bool fProofOfStake) const
{
    if (GetHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
        return vchBlockSig.empty();

    vector<valtype> vSolutions;
    txnouttype whichType;

    if(fProofOfStake)
    {
        const CTxOut& txout = vtx[1].vout[1];

        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            return false;
        if (whichType == TX_PUBKEY)
        {
            valtype& vchPubKey = vSolutions[0];
            CKey key;
            if (!key.SetPubKey(vchPubKey))
                return false;
            if (vchBlockSig.empty())
                return false;
            return key.Verify(GetHash(), vchBlockSig);
        }
    }
    else
    {
        for(unsigned int i = 0; i < vtx[0].vout.size(); i++)
        {
            const CTxOut& txout = vtx[0].vout[i];

            if (!Solver(txout.scriptPubKey, whichType, vSolutions))
                return false;

            if (whichType == TX_PUBKEY)
            {
                // Verify
                valtype& vchPubKey = vSolutions[0];
                CKey key;
                if (!key.SetPubKey(vchPubKey))
                    continue;
                if (vchBlockSig.empty())
                    continue;
                if(!key.Verify(GetHash(), vchBlockSig))
                    continue;

                return true;
            }
        }
    }
    return false;
}
*/
