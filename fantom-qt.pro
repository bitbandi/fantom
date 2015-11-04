TEMPLATE = app
TARGET = Fantom
VERSION = 1.2.0

greaterThan(QT_MAJOR_VERSION, 5) {
    INCLUDEPATH += src src/json src/qt src/qt/plugins/mrichtexteditor
    QT += network widgets
    DEFINES += ENABLE_WALLET
    DEFINES += BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE 
    DEFINES += USE_NATIVE_I2P
    CONFIG += static
    CONFIG += no_include_pwd
    CONFIG += thread
    QMAKE_CXXFLAGS = -fpermissive
}

greaterThan(QT_MAJOR_VERSION, 4) {
    INCLUDEPATH += src src/json src/qt src/qt/plugins/mrichtexteditor
    QT += network widgets
    DEFINES += ENABLE_WALLET
    DEFINES += BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE 
    DEFINES += USE_NATIVE_I2P
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
    CONFIG += static
    CONFIG += no_include_pwd
    CONFIG += thread
    QMAKE_CXXFLAGS = -fpermissive
}

linux {
    SECP256K1_LIB_PATH = /usr/local/lib
    SECP256K1_INCLUDE_PATH = /usr/local/include
}

# for boost 1.37, add -mt to the boost libraries
# use: qmake BOOST_LIB_SUFFIX=-mt
# for boost thread win32 with _win32 sufix
# use: BOOST_THREAD_LIB_SUFFIX=_win32-...
# or when linking against a specific BerkelyDB version: BDB_LIB_SUFFIX=-4.8

# Dependency library locations can be customized with:
#    BOOST_INCLUDE_PATH, BOOST_LIB_PATH, BDB_INCLUDE_PATH,
#    BDB_LIB_PATH, OPENSSL_INCLUDE_PATH and OPENSSL_LIB_PATH respectively

win32 {
	BOOST_LIB_SUFFIX=-mgw49-mt-s-1_57
	BOOST_INCLUDE_PATH=C:/deps/boost_1_57_0
	BOOST_LIB_PATH=C:/deps/boost_1_57_0/stage/lib
	BDB_INCLUDE_PATH=C:/deps/db-4.8.30.NC/build_unix
	BDB_LIB_PATH=C:/deps/db-4.8.30.NC/build_unix
	OPENSSL_INCLUDE_PATH=C:/deps/openssl-1.0.1j/include
	OPENSSL_LIB_PATH=C:/deps/openssl-1.0.1j
	MINIUPNPC_INCLUDE_PATH=C:/deps/
	MINIUPNPC_LIB_PATH=C:/deps/miniupnpc
	QRENCODE_INCLUDE_PATH=C:/deps/qrencode-3.4.4
	QRENCODE_LIB_PATH=C:/deps/qrencode-3.4.4/.libs
	SECP256K1_LIB_PATH=C:/deps/secp256k1/.libs
	SECP256K1_INCLUDE_PATH=C:/deps/secp256k1/include
	GMP_INCLUDE_PATH=C:/deps/gmp-6.0.0
	GMP_LIB_PATH=C:/deps/gmp-6.0.0/.libs
}

# workaround for boost 1.58
DEFINES += BOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

# use: qmake "RELEASE=1"
contains(RELEASE, 1) {
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk
    macx:QMAKE_CFLAGS += -mmacosx-version-min=10.7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk
    macx:QMAKE_LFLAGS += -mmacosx-version-min=10.7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk
    macx:QMAKE_OBJECTIVE_CFLAGS += -mmacosx-version-min=10.7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk

    !windows:!macx {
        # Linux: static link
        # LIBS += -Wl,-Bstatic
    }
}

!win32 {
# for extra security against potential buffer overflows: enable GCCs Stack Smashing Protection
QMAKE_CXXFLAGS *= -fstack-protector-all --param ssp-buffer-size=1
QMAKE_LFLAGS   *= -fstack-protector-all --param ssp-buffer-size=1
# We need to exclude this for Windows cross compile with MinGW 4.2.x, as it will result in a non-working executable!
# This can be enabled for Windows, when we switch to MinGW >= 4.4.x.
}
# for extra security on Windows: enable ASLR and DEP via GCC linker flags
win32:QMAKE_LFLAGS *= -Wl,--dynamicbase -Wl,--nxcompat -static
win32:QMAKE_LFLAGS *= -static-libgcc -static-libstdc++

# use: qmake "USE_QRCODE=1"
# libqrencode (http://fukuchi.org/works/qrencode/index.en.html) must be installed for support
contains(USE_QRCODE, 1) {
    message(Building with QRCode support)
    DEFINES += USE_QRCODE
    LIBS += -lqrencode
}

# use: qmake "USE_UPNP=1" ( enabled by default; default)
#  or: qmake "USE_UPNP=0" (disabled by default)
#  or: qmake "USE_UPNP=-" (not supported)
# miniupnpc (http://miniupnp.free.fr/files/) must be installed for support
contains(USE_UPNP, -) {
    message(Building without UPNP support)
} else {
    message(Building with UPNP support)
    count(USE_UPNP, 0) {
        USE_UPNP=1
    }
    DEFINES += USE_UPNP=$$USE_UPNP MINIUPNP_STATICLIB STATICLIB
    INCLUDEPATH += $$MINIUPNPC_INCLUDE_PATH
    LIBS += $$join(MINIUPNPC_LIB_PATH,,-L,) -lminiupnpc
    win32:LIBS += -liphlpapi
}

# use: qmake "USE_DBUS=1" or qmake "USE_DBUS=0"
linux:count(USE_DBUS, 0) {
    USE_DBUS=1
}
contains(USE_DBUS, 1) {
    message(Building with DBUS (Freedesktop notifications) support)
    DEFINES += USE_DBUS
    QT += dbus
}

contains(FANTOM_NEED_QT_PLUGINS, 1) {
    DEFINES += FANTOM_NEED_QT_PLUGINS
    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
}

# LIBSEC256K1 SUPPORT
QMAKE_CXXFLAGS *= -DUSE_SECP256K1

INCLUDEPATH += src/leveldb/include src/leveldb/helpers
LIBS += $$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a
SOURCES += src/txdb-leveldb.cpp
!win32 {
    # we use QMAKE_CXXFLAGS_RELEASE even without RELEASE=1 because we use RELEASE to indicate linking preferences not -O preferences
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a
} else {
    # make an educated guess about what the ranlib command is called
    isEmpty(QMAKE_RANLIB) {
        QMAKE_RANLIB = $$replace(QMAKE_STRIP, strip, ranlib)
    }
    LIBS += -lshlwapi
    genleveldb.commands = @echo off
}
genleveldb.target = $$PWD/src/leveldb/libleveldb.a
genleveldb.depends = FORCE
PRE_TARGETDEPS += $$PWD/src/leveldb/libleveldb.a
QMAKE_EXTRA_TARGETS += genleveldb
# Gross ugly hack that depends on qmake internals, unfortunately there is no other way to do it.
QMAKE_CLEAN += $$PWD/src/leveldb/libleveldb.a; cd $$PWD/src/leveldb ; $(MAKE) clean

# regenerate src/build.h
!windows|contains(USE_BUILD_INFO, 1) {
    genbuild.depends = FORCE
    genbuild.commands = cd $$PWD; /bin/sh share/genbuild.sh $$OUT_PWD/build/build.h
    genbuild.target = $$OUT_PWD/build/build.h
    PRE_TARGETDEPS += $$OUT_PWD/build/build.h
    QMAKE_EXTRA_TARGETS += genbuild
    DEFINES += HAVE_BUILD_INFO
}

#contains(DEFINES, USE_NATIVE_I2P) {
#    geni2pbuild.depends = FORCE
#    geni2pbuild.commands = cd $$PWD; /bin/sh share/inc_build_number.sh src/i2pbuild.h bitcoin-qt-build-number
#    geni2pbuild.target = src/i2pbuild.h
#    PRE_TARGETDEPS += src/i2pbuild.h
#    QMAKE_EXTRA_TARGETS += geni2pbuild
#}

contains(USE_O3, 1) {
    message(Building O3 optimization flag)
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS += -O3
    QMAKE_CFLAGS += -O3
}

*-g++-32 {
    message("32 platform, adding -msse2 flag")

    QMAKE_CXXFLAGS += -msse2
    QMAKE_CFLAGS += -msse2
}

QMAKE_CXXFLAGS_WARN_ON = -fdiagnostics-show-option -Wall -Wextra -Wno-ignored-qualifiers -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector

# Input
DEPENDPATH += src src/json src/qt
HEADERS += src/qt/fantomgui.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/optionsdialog.h \
    src/qt/coincontroldialog.h \
    src/qt/coincontroltreewidget.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/signverifymessagedialog.h \
    src/qt/aboutdialog.h \
    src/qt/editaddressdialog.h \
    src/qt/fantomaddressvalidator.h \
    src/alert.h \
    src/addrman.h \
    src/base58.h \
    src/bignum.h \
    src/chainparams.h \
    src/chainparamsseeds.h \
    src/checkpoints.h \
    src/cleanse.h \
    src/compat.h \
    src/coincontrol.h \
    src/sync.h \
    src/util.h \
    src/hash.h \
    src/uint256.h \
    src/kernel.h \
    src/scrypt.h \
    src/pbkdf2.h \
    src/serialize.h \
    src/core.h \
    src/main.h \
    src/miner.h \
    src/net.h \
    src/key.h \
    src/eckey.h \
    src/db.h \
    src/txdb.h \
    src/txmempool.h \
    src/walletdb.h \
    src/script.h \
    src/init.h \
    src/mruset.h \
    src/json/json_spirit_writer_template.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/optionsmodel.h \
    src/qt/monitoreddatamapper.h \
    src/qt/trafficgraphwidget.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/fantomamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/rpcclient.h \
    src/rpcprotocol.h \
    src/rpcserver.h \
    src/timedata.h \
    src/qt/overviewpage.h \
    src/qt/blockbrowser.h \
    src/qt/statisticspage.h \
    src/qt/csvmodelwriter.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/fantomunits.h \
    src/qt/qvaluecombobox.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/notificator.h \
    src/qt/paymentserver.h \
    src/allocators.h \
    src/ui_interface.h \
    src/qt/debugconsole.h \
    src/version.h \
    src/netbase.h \
    src/clientversion.h \
    src/threadsafety.h \
    src/tinyformat.h \
    src/stealth.h \
    src/qt/flowlayout.h \
    src/qt/zerosendconfig.h \
    src/blanknode.h \ 
    src/blanknode-pos.h \
	src/blanknode-payments.h \
    src/zerosend.h \    
    src/zerosend-relay.h \
    src/instantx.h \
    src/activeblanknode.h \
    src/blanknodeman.h \
    src/spork.h \
    src/crypto/common.h \
    src/crypto/hmac_sha256.h \
    src/crypto/hmac_sha512.h \
    src/crypto/rfc6979_hmac_sha256.h \
    src/crypto/ripemd160.h \
    src/crypto/sha1.h \
    src/crypto/sha256.h \
    src/crypto/sha512.h \
    src/eccryptoverify.h \
    src/qt/blanknodemanager.h \
    src/qt/addeditblanknode.h \
    src/qt/blanknodeconfigdialog.h \
    src/qt/winshutdownmonitor.h \
    src/smessage.h \
    src/qt/messagepage.h \
    src/qt/messagemodel.h \
    src/qt/sendmessagesdialog.h \
    src/qt/sendmessagesentry.h \
    src/qt/plugins/mrichtexteditor/mrichtextedit.h \
    src/qt/qvalidatedtextedit.h \
    src/rpcmarket.h \
    src/qt/fantommarket.h \
    src/qt/buyspage.h \
    src/qt/sellspage.h \
    src/qt/createmarketlistingdialog.h \
    src/qt/marketlistingdetailsdialog.h \
    src/qt/deliverydetailsdialog.h \
    src/market.h \
    src/crypto/hash/velvetmath.h \
    src/crypto/hash/velvet.h \
	src/crypto/hash/deps/sph_blake.h \
    src/crypto/hash/deps/sph_skein.h \
    src/crypto/hash/deps/sph_keccak.h \
    src/crypto/hash/deps/sph_jh.h \
    src/crypto/hash/deps/sph_groestl.h \
    src/crypto/hash/deps/sph_bmw.h \
    src/crypto/hash/deps/sph_types.h \
    src/crypto/hash/deps/sph_luffa.h \
    src/crypto/hash/deps/sph_cubehash.h \
    src/crypto/hash/deps/sph_echo.h \
    src/crypto/hash/deps/sph_shavite.h \
    src/crypto/hash/deps/sph_simd.h \
    src/crypto/hash/deps/sph_hamsi.h \
    src/crypto/hash/deps/sph_fugue.h \
    src/crypto/hash/deps/sph_shabal.h \
    src/crypto/hash/deps/sph_whirlpool.h \
    src/crypto/hash/deps/sph_haval.h \
    src/crypto/hash/deps/sph_sha2.h


SOURCES += src/qt/fantom.cpp src/qt/fantomgui.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/optionsdialog.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/coincontroldialog.cpp \
    src/qt/coincontroltreewidget.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/signverifymessagedialog.cpp \
    src/qt/aboutdialog.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/fantomaddressvalidator.cpp \
    src/qt/statisticspage.cpp \
    src/alert.cpp \
    src/chainparams.cpp \
    src/cleanse.cpp \
    src/version.cpp \
    src/sync.cpp \
    src/txmempool.cpp \
    src/util.cpp \
    src/hash.cpp \
    src/netbase.cpp \
    src/key.cpp \
    src/eckey.cpp \
    src/script.cpp \
    src/core.cpp \
    src/main.cpp \
    src/miner.cpp \
    src/init.cpp \
    src/net.cpp \
    src/checkpoints.cpp \
    src/addrman.cpp \
    src/base58.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/optionsmodel.cpp \
    src/qt/monitoreddatamapper.cpp \
    src/qt/trafficgraphwidget.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/fantomstrings.cpp \
    src/qt/fantomamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/rpcclient.cpp \
    src/rpcprotocol.cpp \
    src/rpcserver.cpp \
    src/rpcdump.cpp \
    src/rpcmisc.cpp \
    src/rpcnet.cpp \
    src/rpcmining.cpp \
    src/rpcwallet.cpp \
    src/rpcblockchain.cpp \
    src/rpcrawtransaction.cpp \
    src/timedata.cpp \
    src/qt/overviewpage.cpp \
    src/qt/blockbrowser.cpp \
    src/qt/csvmodelwriter.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/fantomunits.cpp \
    src/qt/qvaluecombobox.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/notificator.cpp \
    src/qt/paymentserver.cpp \
    src/qt/debugconsole.cpp \
    src/noui.cpp \
    src/kernel.cpp \
    src/scrypt-arm.S \
    src/scrypt-x86.S \
    src/scrypt-x86_64.S \
    src/scrypt.cpp \
    src/pbkdf2.cpp \
    src/stealth.cpp \
    src/qt/flowlayout.cpp \
    src/qt/zerosendconfig.cpp \
    src/blanknode.cpp \
    src/blanknode-pos.cpp \
    src/zerosend.cpp \
    src/zerosend-relay.cpp \
    src/rpczerosend.cpp \
    src/blanknode-payments.cpp \
    src/instantx.cpp \
    src/activeblanknode.cpp \
    src/spork.cpp \
    src/blanknodeconfig.cpp \
    src/blanknodeman.cpp \
    src/crypto/hmac_sha256.cpp \
    src/crypto/hmac_sha512.cpp \
    src/crypto/rfc6979_hmac_sha256.cpp \
    src/crypto/ripemd160.cpp \
    src/crypto/sha1.cpp \
    src/crypto/sha256.cpp \
    src/crypto/sha512.cpp \
    src/eccryptoverify.cpp \
    src/qt/blanknodemanager.cpp \
    src/qt/addeditblanknode.cpp \
    src/qt/blanknodeconfigdialog.cpp \
    src/qt/winshutdownmonitor.cpp \
    src/smessage.cpp \
    src/qt/messagepage.cpp \
    src/qt/messagemodel.cpp \
    src/qt/sendmessagesdialog.cpp \
    src/qt/sendmessagesentry.cpp \
    src/qt/qvalidatedtextedit.cpp \
    src/qt/plugins/mrichtexteditor/mrichtextedit.cpp \
    src/rpcsmessage.cpp \
    src/rpcmarket.cpp \
    src/qt/fantommarket.cpp \
    src/qt/buyspage.cpp \
    src/qt/sellspage.cpp \
    src/qt/createmarketlistingdialog.cpp \
    src/qt/marketlistingdetailsdialog.cpp \
    src/qt/deliverydetailsdialog.cpp \
    src/market.cpp \
    src/crypto/hash/velvetmath.cpp \
    src/crypto/hash/deps/blake.c \
    src/crypto/hash/deps/bmw.c \
    src/crypto/hash/deps/groestl.c \
    src/crypto/hash/deps/jh.c \
    src/crypto/hash/deps/keccak.c \
    src/crypto/hash/deps/skein.c \
    src/crypto/hash/deps/luffa.c \
    src/crypto/hash/deps/cubehash.c \
    src/crypto/hash/deps/shavite.c \
    src/crypto/hash/deps/echo.c \
    src/crypto/hash/deps/simd.c \
    src/crypto/hash/deps/hamsi.c \
    src/crypto/hash/deps/fugue.c \
    src/crypto/hash/deps/shabal.c \
    src/crypto/hash/deps/whirlpool.c \
    src/crypto/hash/deps/haval.c \
    src/crypto/hash/deps/sha2big.c

RESOURCES += \
    src/qt/fantom.qrc

FORMS += \
    src/qt/forms/coincontroldialog.ui \
    src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/signverifymessagedialog.ui \
    src/qt/forms/aboutdialog.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/blockbrowser.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui \
    src/qt/forms/debugconsole.ui \
    src/qt/forms/optionsdialog.ui \
    src/qt/forms/zerosendconfig.ui \
    src/qt/forms/blanknodemanager.ui \
    src/qt/forms/addeditblanknode.ui \
    src/qt/forms/statisticspage.ui \
    src/qt/forms/blanknodeconfigdialog.ui \
    src/qt/forms/messagepage.ui \
    src/qt/forms/sendmessagesentry.ui \
    src/qt/forms/sendmessagesdialog.ui \
    src/qt/plugins/mrichtexteditor/mrichtextedit.ui \
    src/qt/forms/fantommarket.ui \
    src/qt/forms/buyspage.ui \
    src/qt/forms/sellspage.ui \
    src/qt/forms/createmarketlistingdialog.ui \
    src/qt/forms/marketlistingdetailsdialog.ui \
    src/qt/forms/deliverydetailsdialog.ui \

contains(DEFINES, USE_NATIVE_I2P) {
HEADERS += src/i2p.h \
    src/i2psam.h \
    src/qt/showi2paddresses.h \
    src/qt/i2poptionswidget.h

SOURCES += src/i2p.cpp \
    src/i2psam.cpp \
    src/qt/showi2paddresses.cpp \
    src/qt/i2poptionswidget.cpp

FORMS += src/qt/forms/showi2paddresses.ui \
    src/qt/forms/i2poptionswidget.ui
}

contains(USE_QRCODE, 1) {
HEADERS += src/qt/qrcodedialog.h
SOURCES += src/qt/qrcodedialog.cpp
FORMS += src/qt/forms/qrcodedialog.ui
}

CODECFORTR = UTF-8

# for lrelease/lupdate
# also add new translations to src/qt/fantom.qrc under translations/
TRANSLATIONS = $$files(src/qt/locale/fantom_*.ts)

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
isEmpty(QM_DIR):QM_DIR = $$PWD/src/qt/locale
# automatically build translations, so they can be included in resource file
TSQM.name = lrelease ${QMAKE_FILE_IN}
TSQM.input = TRANSLATIONS
TSQM.output = $$QM_DIR/${QMAKE_FILE_BASE}.qm
TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
TSQM.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += TSQM

# "Other files" to show in Qt Creator
OTHER_FILES += \
    doc/*.rst doc/*.txt doc/README README.md res/fantom-qt.rc

# platform specific defaults, if not overridden on command line
isEmpty(BOOST_LIB_SUFFIX) {
    macx:BOOST_LIB_SUFFIX = -mt
    win32:BOOST_LIB_SUFFIX = -mgw49-mt-s-1_59
}

isEmpty(BOOST_THREAD_LIB_SUFFIX) {
    BOOST_THREAD_LIB_SUFFIX = $$BOOST_LIB_SUFFIX
}

isEmpty(BDB_LIB_PATH) {
    macx:BDB_LIB_PATH = /usr/local/Cellar/berkeley-db4/4.8.30/lib
}

isEmpty(BDB_LIB_SUFFIX) {
    macx:BDB_LIB_SUFFIX = -4.8
}

isEmpty(BDB_INCLUDE_PATH) {
    macx:BDB_INCLUDE_PATH = /usr/local/Cellar/berkeley-db4/4.8.30/include
}

isEmpty(BOOST_LIB_PATH) {
    macx:BOOST_LIB_PATH = /usr/local/Cellar/boost/1.58.0/lib
}

isEmpty(BOOST_INCLUDE_PATH) {
    macx:BOOST_INCLUDE_PATH = /usr/local/Cellar/boost/1.58.0/include
}

isEmpty(QRENCODE_LIB_PATH) {
    macx:QRENCODE_LIB_PATH = /usr/local/lib
}

isEmpty(QRENCODE_INCLUDE_PATH) {
    macx:QRENCODE_INCLUDE_PATH = /usr/local/include
}

isEmpty(SECP256K1_LIB_PATH) {
    macx:SECP256K1_LIB_PATH = /usr/local/lib
}

isEmpty(SECP256K1_INCLUDE_PATH) {
    macx:SECP256K1_INCLUDE_PATH = /usr/local/include
}
windows:DEFINES += WIN32
windows:RC_FILE = src/qt/res/fantom-qt.rc

windows:!contains(MINGW_THREAD_BUGFIX, 0) {
    # At least qmake's win32-g++-cross profile is missing the -lmingwthrd
    # thread-safety flag. GCC has -mthreads to enable this, but it doesn't
    # work with static linking. -lmingwthrd must come BEFORE -lmingw, so
    # it is prepended to QMAKE_LIBS_QT_ENTRY.
    # It can be turned off with MINGW_THREAD_BUGFIX=0, just in case it causes
    # any problems on some untested qmake profile now or in the future.
    DEFINES += _MT BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
    QMAKE_LIBS_QT_ENTRY = -lmingwthrd $$QMAKE_LIBS_QT_ENTRY
}

macx:HEADERS += src/qt/macdockiconhandler.h src/qt/macnotificationhandler.h
macx:OBJECTIVE_SOURCES += src/qt/macdockiconhandler.mm src/qt/macnotificationhandler.mm
macx:LIBS += -framework Foundation -framework ApplicationServices -framework AppKit -framework CoreServices
macx:DEFINES += MAC_OSX MSG_NOSIGNAL=0
macx:ICON = src/qt/res/icons/fantom.icns
macx:TARGET = "Fantom-Qt"
macx:QMAKE_CFLAGS_THREAD += -pthread
macx:QMAKE_LFLAGS_THREAD += -pthread
macx:QMAKE_CXXFLAGS_THREAD += -pthread
macx:QMAKE_INFO_PLIST = share/qt/Info.plist

# Set libraries and includes at end, to use platform-defined defaults if not overridden
INCLUDEPATH += $$SECP256K1_INCLUDE_PATH $$BOOST_INCLUDE_PATH $$BDB_INCLUDE_PATH $$OPENSSL_INCLUDE_PATH $$QRENCODE_INCLUDE_PATH $$GMP_INCLUDE_PATH
LIBS += $$join(SECP256K1_LIB_PATH,,-L,) $$join(BOOST_LIB_PATH,,-L,) $$join(BDB_LIB_PATH,,-L,) $$join(OPENSSL_LIB_PATH,,-L,) $$join(QRENCODE_LIB_PATH,,-L,) $$join(GMP_LIB_PATH,,-L,)
LIBS += -lssl -lcrypto -ldb_cxx$$BDB_LIB_SUFFIX -lgmp
# -lgdi32 has to happen after -lcrypto (see  #681)
windows:LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
LIBS += -lsecp256k1
LIBS += -lboost_system$$BOOST_LIB_SUFFIX -lboost_filesystem$$BOOST_LIB_SUFFIX -lboost_program_options$$BOOST_LIB_SUFFIX -lboost_thread$$BOOST_THREAD_LIB_SUFFIX
windows:LIBS += -lboost_chrono$$BOOST_LIB_SUFFIX

contains(RELEASE, 1) {
    !windows:!macx {
        # Linux: turn dynamic linking back on for c/c++ runtime libraries
        LIBS += -Wl,-Bdynamic
    }
}

!windows:!macx {
    DEFINES += LINUX
    LIBS += -lrt -ldl
}

system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)
