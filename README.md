Solarcoin Core integration/staging tree
=====================================

https://solarcoin.org

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

Solarcoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/solarcoin-project/solarcoin/tags) are created
regularly to indicate new official, stable release versions of Solarcoin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [mailing list](https://groups.google.com/forum/#!forum/solarcoin-dev)
should be used to discuss complicated or controversial changes before working
on a patch set.

Developer IRC can be found on Freenode at #solarcoin-dev.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

We only accept translation fixes that are submitted through [Bitcoin Core's Transifex page](https://www.transifex.com/projects/p/bitcoin/).
Translations are converted to Solarcoin periodically.

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.
