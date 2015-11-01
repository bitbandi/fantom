**Fantom (FNX) Release Candidate for Version 1.0.0.0**

(FOR TESTING PURPOSES ONLY)

Fantom Integration/Staging Tree
================================

**Copyright (c) 2015 The Fantom Developers**

What is Fantom?
----------------
Algorithm: Scrypt
Coin Suffix: FNX
PoW Period: 42,002 Blocks
PoW Target Spacing: 60 Seconds
PoW Difficulty Retarget: 10 Blocks
PoW Reward per Block: 42 FNX
Full Confirmation: 10 Blocks
Maturity: 42 Blocks
PoS Target Spacing: 64 Seconds
PoS Difficulty Retarget: 10 Blocks
PoS Reward: 1 FNX Static PoS Reward
Minimum Confirmations for Stake: 420 Blocks
PoS Min: 4 Hours
PoS Max: Unlimited
Blanknode Collateral Amount: 512 FNX
Blanknode Min Confirmation: 10 Blocks
Total Coins: 90,000,000 FNX
Block Size: 50MB (50X Bitcoin Core)

Fantom is a new digital currency that enables instant payments to anyone, anywhere in the world. Fantom uses peer-to-peer technology over I2P/Tor/ClearNet to operate securly with no central authority (centralisation): managing transactions and issuing currency (FNX) are carried out collectively by the Fantom network. Fantom is the name of open source software which enables the use of the currency FNX.

Fantom utilises Blanknodes, Zerosend and InstantX to provide anonymous and near instant transaction confirmations.

Fantom only broadcasts transactions once unlike Bitcoin Core which rebroadcasts transactions which can be used to unmask and link IP addresses and transactions. However if your transaction does not successfully broadcast you can use the -resendtx console command to broadcast the transaction again.

Fantom implements Gavin Andresens signature cache optimisation from Bitcoin for significantly faster transaction validation.

Fantom uses ShadowChat from Shadow for encrypted, anonymous and secure messaging via the Fantom wallet network.

Fantom includes a completely decentralised marketplace, providing anonymity and escrow services for safe and fast trades.

Fantom includes an Address Index feature, based on the address index API (searchrawtransactions RPC command) implemented in Bitcoin Core but modified implementation to work with the Fantom codebase (PoS coins maintain a txindex by default for instance). Initialize the Address Index by running the -reindexaddr command line argument, it may take 10-15 minutes to build the initial index.


**Zerosend Network Information**
Ported Masternodes from Darkcoin and rebranded as Blanknodes and modified to use stealth addresses.
Darksend ported and rebranded as Zerosend.
Utilisation of InstantX for instant transaction confirmation.
Multiple Blanknodes can be ran from the same IP address.


**MainNet Parameters**
P2P Port/Blanknodes = 31000
RPC Port = 31500
Magic Bytes: 0x42 0x04 0x20 0x24


**TestNet Parameters**
P2P Port/Blanknodes = 31750
RPC Port = 31800
Magic Bytes: 0x24 0x20 0x04 0x42



Fantom is dependent upon libsecp256k1 by sipa, please follow this guide to install to Linux (Ubuntu)
======================================================================================================
//Open a terminal

//Type:
$ cd ~

//Then:
$ sudo apt-get install make automake autoconf libtool unzip git

//Then:
$ git clone https://github.com/FNX/Fantom.git

//Then:
$ cd Fantom/libs/

//Then:
$ sudo unzip secp256k1-master.zip

//Then:
$ cd secp256k1-master

//Then:
$ sudo libtoolize

//Then:
$ sudo aclocal

//Then:
$ sudo autoheader

//Then:
$ sudo ./autogen.sh

//Then:
$ sudo ./configure

//Then:
$ sudo make

//Then:
$ ./tests

//Then:
$ sudo make install

//end of guide


Build Instructions for Qt5 Linux Wallet (Ubuntu)
================================================
//Install dependencies via Terminal:

$ sudo apt-get install make libqt5webkit5-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools qtcreator libprotobuf-dev protobuf-compiler build-essential libboost-dev libboost-all-dev libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev libdb++-dev libstdc++6 libminiupnpc-dev libevent-dev libcurl4-openssl-dev git libpng-dev qrencode libqrencode-dev

//In terminal navigate to the Fantom folder (assuming you have followed the libsecp256k1 guide).

$ cd /home/Fantom

//Then:

$ qmake -qt=qt5 "USE_QRCODE=1" "USE_NATIVE_I2P=1"

//Then:

$ make

//This will compile and build the Qt Wallet which takes a little while, please be patient.

//When finished you will have a file called Fantom - Simply Double Click

//end of guide


Build Instructions for Terminal Based Linux Wallet (Ubuntu)
===========================================================
//Install dependencies via Terminal:

$ sudo apt-get install build-essential libboost-all-dev libssl-dev libcurl4-openssl-dev libminiupnpc-dev libdb++-dev libstdc++6 make 

//In terminal navigate to the Fantom folder (assuming you have followed the libsecp256k1 guide).

$ cd /home/Fantom/src/

//Then:

$ cp crypto obj/crypto -rR

//Enter into the terminal:

$ make -f makefile.unix USE_NATIVE_I2P=1

//This will produce a file named fantomd which is the command line instance of Fantom

//Now type:

$ strip fantomd

//When finished you will have a file called fantomd

//To run Fantom

$ ./fantomd & 

//It will complain about having no fantom.conf file, we'll edit the one provided and move it into place

$ cd ..
$ nano fantom.conf

//Edit the Username and Password fields to anything you choose (but remember them) then save the file

$ mv fantom.conf /home/.fantom/
$ cd src/
$ ./fantomd &

//The server will start. Here are a few commands, google for more.

$ ./fantomd getinfo
$ ./fantomd getmininginfo
$ ./fantomd getnewaddresss

//end of guide


Example Blanknode .conf Configuration
===================================================

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


Set the blanknodeprivkey to the result of running:

blanknode genkey
//From the RPC console in the local wallet.

//If you are running your blanknode as a hidden service, replace YOUR_IP with your onion hostname (typically found in /var/lib/tor/YOURHIDDENSERVICENAME/hostname).

Port 31000 must be open.

//If you are using the hot/cold system where you are remotely activating your blanknode from your local wallet, you will also put these entries in your local (cold) wallet .conf:

blanknodeaddr=YOUR_IP:31000
blanknode=1
blanknodeprivkey=KEY GENERATED BY COMMAND

//Then, start the daemon on your remote blanknode, start your local wallet, and then you can start your blanknode from the RPC console:

blanknode start


//end of guide
