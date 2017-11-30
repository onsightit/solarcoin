// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/**
 * network protocol versioning
 */

//static const int PROTOCOL_VERSION = 70015; // Bitcoin
static const int PROTOCOL_VERSION = 70005; // SolarCoin

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! In this version, 'getheaders' was introduced.
//static const int GETHEADERS_VERSION = 70002; // Bitcoin
static const int GETHEADERS_VERSION = 70005; // SolarCoin
//static const int GETHEADERS_VERSION = 70006; // SolarCoin

//! disconnect from peers older than this proto version
static const int MIN_PEER_PROTO_VERSION = 70005;

//! nTime field added to CAddress, starting with this version;
//! if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 31402;

//! BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

//! "filter*" commands are disabled without NODE_BLOOM after and including this version
//static const int NO_BLOOM_VERSION = 70011; // Bitcoin
static const int NO_BLOOM_VERSION = 70005; // SolarCoin

//! "sendheaders" command and announcing blocks with headers starts with this version
//static const int SENDHEADERS_VERSION = 70012; // Bitcoin
static const int SENDHEADERS_VERSION = GETHEADERS_VERSION; // SolarCoin: Allow sendheaders with lagacy 2.1.8 nodes.

//! "feefilter" tells peers to filter invs to you by fee starts with this version
//static const int FEEFILTER_VERSION = 70013; // Bitcoin
static const int FEEFILTER_VERSION = 70013; // SolarCoin: TODO: Introduce feefilter in future protocol.

//! short-id-based block download starts with this version
//static const int SHORT_IDS_BLOCKS_VERSION = 70014; // Bitcoin
static const int SHORT_IDS_BLOCKS_VERSION = 70014; // SolarCoin: TODO: Introduce short_ids in future protocol.

//! not banning for invalid compact blocks starts with this version
//static const int INVALID_CB_NO_BAN_VERSION = 70015; // Bitcoin
static const int INVALID_CB_NO_BAN_VERSION = 70015; // SolarCoin

#endif // BITCOIN_VERSION_H
