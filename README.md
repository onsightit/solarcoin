SolarCoin integration/staging tree
================================
![solarcoin](http://i.imgur.com/BS9hSS8.png)

http://www.solarcoin.org

Copyright (c) 2009-2014 Bitcoin Developers
Copyright (c) 2011-2014 Litecoin Developers
Copyright (c) 2014-2015 VeriCoin Developers
Copyright (c) 2014-2015 SolarCoin Developers

What is SolarCoin?
----------------

SolarCoin [SLR] is a lite version of Bitcoin using scrypt as a proof-of-work algorithm.

- 140 Character Transaction Messaging
- 100 coins per block initial rewards, halving every 525,600 (once a year) blocks.
- 1 Minute Block Targets
- 1440 Block Difficulty Adjustments
- RPC PORT = 18181
- P2P PORT = 18188
- Public Key: 18
- Addresses begin with '8'
- nChainStartTime: 1384473600
- Genesis Block Hash: edcf32dbfd327fe7f546d3a175d91b05e955ec1224e087961acc9a2aa8f592ee
- Merkle Root: 33ecdb1985425f576c65e2c85d7983edc6207038a2910fefaf86cfb4e53185a3

Harforks: 
Block 310,000 (Implemented block reduction & Patched KGW retargeting algorithm)

QR Code Support

For more information, as well as an immediately useable, binary version of
the SolarCoin client sofware, see http://www.solarcoin.org.

License
-------

SolarCoin is released under the terms of the MIT license. See `COPYING` for more
information or see http://opensource.org/licenses/MIT.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the SolarCoin
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already) on the
[mailing list](http://ADDE-DEVELOPER-MAILING-LIST.solarcoin.cc)

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see `doc/coding.txt`) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/bitcoin/bitcoin/tags) are created
regularly to indicate new official, stable release versions of SolarCoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test. Please be patient and help out, and
remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write unit tests for new code, and to
submit new unit tests for old code.

Unit tests for the core code are in `src/test/`. To compile and run them:

    cd src; make -f makefile.unix test

Unit tests for the GUI code are in `src/qt/test/`. To compile and run them:

    qmake BITCOIN_QT_TEST=1 -o Makefile.test bitcoin-qt.pro
    make -f Makefile.test
    ./solarcoin-qt_test

