**Fantom (FNX) Release Candidate for Version 1.0.3.0**

Fantom Integration/Staging Tree
================================

**Copyright (c) 2015 The Fantom Developers**

What is Fantom?
----------------

Fantom is an EXPERIMENTAL Digital Project Launched in 2015 for working on Bleeding Edge Programming....

Fantom requires LibSECP256k1 and LibGMP for functioning

* Algorithm: Velvet
* Coin Suffix: FNX
* PoW Period: Inifinite
* PoW Target Spacing: 28 Seconds
* PoW Difficulty Retarget: 28 Seconds
* PoW Reward per Block: [CURRENTLY] 5 FNX
* Full Confirmation: 10 Blocks
* Maturity: 42 Blocks
* PoS Target Spacing: 28 Seconds
* PoS Difficulty Retarget: Every Block
* PoS Reward: [CURRENTLY] 2 FNX Dynamic PoS Reward
* PoS Max: Unlimited
* Blanknode Collateral Amount: 512 FNX
* Blanknode Min Confirmation: 10 Blocks
* Total Coins: 8 Million FNX

Fantom is a bleeding edge digital currency that enables instant payments to anyone, anywhere in the world. Fantom uses peer-to-peer technology over I2P/Tor/ClearNet to operate securly with no central authority (centralisation): managing transactions and issuing currency (FNX) are carried out collectively by the Fantom network. Fantom is the name of open source software which enables the use of the currency FNX.

Fantom utilises Blanknodes, Zerosend and InstantX to provide anonymous and near instant transaction confirmations.

Fantom implements Gavin Andresens signature cache optimisation from Bitcoin for significantly faster transaction validation.

Fantom uses ShadowChat from Shadow for encrypted, anonymous and secure messaging via the Fantom wallet network.

Fantom includes a completely decentralised marketplace, providing anonymity and escrow services for safe and fast trades.

Fantom includes an Address Index feature, based on the address index API (searchrawtransactions RPC command) implemented in Bitcoin Core but modified implementation to work with the Fantom codebase (PoS coins maintain a txindex by default for instance). Initialize the Address Index by running the -reindexaddr command line argument, it may take 10-15 minutes to build the initial index.


**Zerosend Network Information**
* Ported Masternodes from Darkcoin and rebranded as Blanknodes and modified to use stealth addresses.
* Darksend ported and rebranded as Zerosend.
* Utilisation of InstantX for instant transaction confirmation.
* Multiple Blanknodes can be ran from the same IP address.


**MainNet Parameters**
* P2P Port/Blanknodes = 31000
* RPC Port = 31500
* Magic Bytes: 0x42 0x04 0x20 0x24


**TestNet Parameters**
* P2P Port/Blanknodes = 31750
* RPC Port = 31800
* Magic Bytes: 0x24 0x20 0x04 0x42



Fantom is dependent upon libsecp256k1 by sipa and libgmp, please follow this guide to install to Linux (Ubuntu)
======================================================================================================
```
cd ~
sudo apt-get install make automake autoconf libtool unzip git libgmime-2.6-0 libgmp-dev libgmp10 libgmpxx4ldbl
git clone https://github.com/LordOfTheCoin/fantom.git
cd fantom/libs/
sudo unzip secp256k1-master.zip
cd secp256k1-master
sudo libtoolize
sudo aclocal
sudo autoheader
sudo ./autogen.sh
sudo ./configure
sudo make
./tests
sudo make install
```

Build Instructions for Qt5 Linux Wallet (Ubuntu)
================================================
```
sudo apt-get install make libqt5webkit5-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools qtcreator libprotobuf-dev protobuf-compiler build-essential libboost-dev libboost-all-dev libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev libdb++-dev libstdc++6 libminiupnpc-dev libevent-dev libcurl4-openssl-dev git libpng-dev qrencode libqrencode-dev libgmime-2.6-0 libgmp-dev libgmp10 libgmpxx4ldbl
```
In terminal navigate to the Fantom folder (assuming you have followed the libsecp256k1 guide).

```
cd /home/Fantom
qmake -qt=qt5 "USE_QRCODE=1" "USE_NATIVE_I2P=1"
make
```

Build Instructions for Terminal Based Linux Wallet (Ubuntu)
===========================================================

```
sudo apt-get install build-essential libboost-all-dev libssl-dev libcurl4-openssl-dev libminiupnpc-dev libdb++-dev libstdc++6 make libgmime-2.6-0 libgmp-dev libgmp10 libgmpxx4ldbl
```

In terminal navigate to the Fantom folder (assuming you have followed the libsecp256k1 guide).

```
cd /home/fantom/src/
cp crypto obj/crypto -rR
mkdir obj/crypto/hash
mkdir obj/crypto/hash/deps
make -f makefile.unix USE_NATIVE_I2P=1
strip fantomd
./fantomd 
cd ..
nano fantom.conf
```

Edit the Username and Password fields to anything you choose (but remember them) then save the file

```
mv fantom.conf /home/.fantom/
cd src/
./fantomd
```
Example Blanknode .conf Configuration
===================================================

```
rpcallowip=127.0.0.1
rpcuser=MAKEUPYOUROWNUSERNAME
rpcpassword=MAKEUPYOUROWNPASSWORD
rpcport=31500
server=1
daemon=1
listen=1
staking=0
port=31000
blanknodeaddr=YOUR_IP:31000
blanknode=1
blanknodeprivkey=KEY GENERATED BY COMMAND
snconflock=1 // Blanknodes can be locked via the configuration file 
```

Set the blanknodeprivkey to the result of running:
```
blanknode genkey
```

From the RPC console in the local wallet.

If you are running your blanknode as a hidden service, replace YOUR_IP with your onion hostname (typically found in /var/lib/tor/YOURHIDDENSERVICENAME/hostname).

Port 31000 must be open.

If you are using the hot/cold system where you are remotely activating your blanknode from your local wallet, you will also put these entries in your local (cold) wallet .conf:

> blanknodeaddr=YOUR_IP:31000
 blanknode=1
 blanknodeprivkey=KEY GENERATED BY COMMAND

Then, start the daemon on your remote blanknode, start your local wallet, and then you can start your blanknode from the RPC console:

> blanknode start
