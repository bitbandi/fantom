// Copyright 2015 Ahmad A Kazi
// Velvet Hashing Algorithm for Proof of Work Based Currencies and for Hybrids
// Intention is to stress CPUS and Design the Algorithm to make sure CPUMiners are hard to make
// Thus allowing Decentralzation At Once!

#ifndef VELVETHASH_H
#define VELVETHASH_H

#include <gmpxx.h>

#include "uint256.h"
#include "velvetmath.h"
#include "deps/sph_bmw.h"
#include "deps/sph_groestl.h"
#include "deps/sph_jh.h"
#include "deps/sph_keccak.h"
#include "deps/sph_skein.h"
#include "deps/sph_luffa.h"
#include "deps/sph_cubehash.h"
#include "deps/sph_shavite.h"
#include "deps/sph_simd.h"
#include "deps/sph_echo.h"
#include "deps/sph_hamsi.h"
#include "deps/sph_fugue.h"
#include "deps/sph_shabal.h"
#include "deps/sph_whirlpool.h"
#include "deps/sph_sha2.h"
#include "deps/sph_haval.h"

#ifndef QT_NO_DEBUG
#include <string>
#endif

#ifdef GLOBALDEFINED
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL sph_bmw512_context       z_bmw;
GLOBAL sph_groestl512_context   z_groestl;
GLOBAL sph_jh512_context        z_jh;
GLOBAL sph_keccak512_context    z_keccak;
GLOBAL sph_skein512_context     z_skein;
GLOBAL sph_luffa512_context     z_luffa;
GLOBAL sph_cubehash512_context  z_cubehash;
GLOBAL sph_shavite512_context   z_shavite;
GLOBAL sph_simd512_context      z_simd;
GLOBAL sph_echo512_context      z_echo;
GLOBAL sph_hamsi512_context     z_hamsi;
GLOBAL sph_fugue512_context     z_fugue;
GLOBAL sph_shabal512_context    z_shabal;
GLOBAL sph_whirlpool_context    z_whirlpool;
GLOBAL sph_sha512_context       z_sha2;
GLOBAL sph_haval256_5_context   z_haval;
GLOBAL sph_shabal256_context	z_shabal256;

#define fillz() do { \
    sph_bmw512_init(&z_bmw); \
    sph_groestl512_init(&z_groestl); \
    sph_jh512_init(&z_jh); \
    sph_keccak512_init(&z_keccak); \
    sph_skein512_init(&z_skein); \
    sph_luffa512_init(&z_luffa); \
    sph_cubehash512_init(&z_cubehash); \
    sph_shavite512_init(&z_shavite); \
    sph_simd512_init(&z_simd); \
    sph_echo512_init(&z_echo); \
    sph_hamsi512_init(&z_hamsi); \
    sph_fugue512_init(&z_fugue); \
    sph_shabal512_init(&z_shabal); \
    sph_whirlpool_init(&z_whirlpool); \
    sph_sha512_init(&z_sha2); \
    sph_haval256_5_init(&z_haval); \
    sph_shabal256_init(&z_shabal256); \
} while (0) 


#define ZBMW (memcpy(&ctx_bmw, &z_bmw, sizeof(z_bmw)))
#define ZGROESTL (memcpy(&ctx_groestl, &z_groestl, sizeof(z_groestl)))
#define ZJH (memcpy(&ctx_jh, &z_jh, sizeof(z_jh)))
#define ZKECCAK (memcpy(&ctx_keccak, &z_keccak, sizeof(z_keccak)))
#define ZSKEIN (memcpy(&ctx_skein, &z_skein, sizeof(z_skein)))
#define ZHAMSI (memcpy(&ctx_hamsi, &z_hamsi, sizeof(z_hamsi)))
#define ZFUGUE (memcpy(&ctx_fugue, &z_fugue, sizeof(z_fugue)))
#define ZSHABAL (memcpy(&ctx_shabal, &z_shabal, sizeof(z_shabal)))
#define ZWHIRLPOOL (memcpy(&ctx_whirlpool, &z_whirlpool, sizeof(z_whirlpool)))
#define ZSHA2 (memcpy(&ctx_sha2, &z_sha2, sizeof(z_sha2)))
#define ZHAVAL (memcpy(&ctx_haval, &z_haval, sizeof(z_haval)))
#define ZSHABAL256 (memcpy(&ctx_shabal256, &z_shabal256, sizeof(z_shabal256)))

#define BITS_PER_DIGIT 3.32192809488736234787
#define EPS (std::numeric_limits<double>::epsilon())

#define NM25M 23
#define SW_DIVS 23

template<typename T1>
inline uint256 VelvetHash(const T1 pbegin, const T1 pend, const unsigned int nnNonce)
{
    sph_bmw512_context        ctx_bmw;
    sph_groestl512_context    ctx_groestl;
    sph_jh512_context         ctx_jh;
    sph_keccak512_context     ctx_keccak;
    sph_skein512_context      ctx_skein;
    sph_luffa512_context      ctx_luffa;
    sph_cubehash512_context   ctx_cubehash;
    sph_shavite512_context    ctx_shavite;
    sph_simd512_context       ctx_simd;
    sph_echo512_context       ctx_echo;
    sph_hamsi512_context      ctx_hamsi;
    sph_fugue512_context      ctx_fugue;
    sph_shabal512_context     ctx_shabal;
    sph_whirlpool_context     ctx_whirlpool;
    sph_sha512_context        ctx_sha2;
    sph_haval256_5_context    ctx_haval;
	sph_shabal256_context	  ctx_shabal256;
	
static unsigned char pblank[1];

#ifndef QT_NO_DEBUG
    //std::string strhash;
    //strhash = "";
#endif

	int bytes, nnNonce2 = (int)(nnNonce/2);
	
    uint512 mask = 25;
    uint512 zero = 0;
    
    uint512 hash[25];

	uint256 finalhash;
	for(int i=0; i < 25; i++)
	hash[i] = 0;

    const void* ptr = (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0]));
    size_t sz = (pend - pbegin) * sizeof(pbegin[0]);
    
    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[0]));
    
    if ((hash[0] & mask) != zero)
    {
		sph_bmw512_init(&ctx_bmw);
		sph_bmw512 (&ctx_bmw, ptr, sz);
		sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[1]));    
	}
    else
    {
		sph_groestl512_init(&ctx_groestl);
		sph_groestl512 (&ctx_groestl, ptr, sz);
		sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[1]));
    }
    
    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[2]));

	sph_groestl512_init(&ctx_groestl);
	sph_groestl512 (&ctx_groestl, ptr, sz);
	sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[3]));

    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[4]));

    if ((hash[4] & mask) != zero)
    {
		sph_skein512_init(&ctx_skein);
		sph_skein512 (&ctx_skein, ptr, sz);
		sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[5]));
	}
    else
    {
		sph_jh512_init(&ctx_jh);
		sph_jh512 (&ctx_jh, ptr, sz);
		sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[5]));
    }
    
    if ((hash[5] & mask) != zero)
    {
		sph_jh512_init(&ctx_jh);
		sph_jh512 (&ctx_jh, ptr, sz);
		sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[6]));
	}
    else
    {
		sph_skein512_init(&ctx_skein);
		sph_skein512 (&ctx_skein, ptr, sz);
		sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[6]));
    }
    
    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512 (&ctx_cubehash, ptr, sz);
    sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[7]));
    
    sph_shavite512_init(&ctx_shavite);
    sph_shavite512(&ctx_shavite, ptr, sz);
    sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[8]));
        
    if ((hash[8] & mask) != zero)
    {
		sph_keccak512_init(&ctx_keccak);
		sph_keccak512 (&ctx_keccak, ptr, sz);
		sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[9]));
	}
    else
    {

		sph_luffa512_init(&ctx_luffa);
		sph_luffa512 (&ctx_luffa, static_cast<void*>(&hash[8]), 64);
		sph_luffa512_close(&ctx_luffa, static_cast<void*>(&hash[9]));
    }

    if ((hash[9] & mask) != zero)
    {
		sph_simd512_init(&ctx_simd);
		sph_simd512 (&ctx_simd, ptr, sz);
		sph_simd512_close(&ctx_simd, static_cast<void*>(&hash[10]));
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[10]));
    }
       
    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[11]));

    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[12]));

    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[13]));

    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[14]));

    sph_shabal512_init(&ctx_shabal);
    sph_shabal512 (&ctx_shabal, ptr, sz);
    sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[15]));
    
    if ((hash[15] & mask) != zero)
    {
		sph_hamsi512_init(&ctx_hamsi);
		sph_hamsi512 (&ctx_hamsi, ptr, sz);
		sph_hamsi512_close(&ctx_hamsi, static_cast<void*>(&hash[16]));	
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[16]));
    }
       
    sph_fugue512_init(&ctx_fugue);
    sph_fugue512 (&ctx_fugue, ptr, sz);
    sph_fugue512_close(&ctx_fugue, static_cast<void*>(&hash[17]));

    if ((hash[17] & mask) != zero)
    {
		sph_shabal512_init(&ctx_shabal);
		sph_shabal512 (&ctx_shabal, ptr, sz);
		sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[18]));
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[18]));
    }

    sph_whirlpool_init(&ctx_whirlpool);
    sph_whirlpool (&ctx_whirlpool, ptr, sz);
    sph_whirlpool_close(&ctx_whirlpool, static_cast<void*>(&hash[19]));

    sph_sha512_init(&ctx_sha2);
    sph_sha512 (&ctx_sha2, ptr, sz);
    sph_sha512_close(&ctx_sha2, static_cast<void*>(&hash[20]));

    sph_haval256_5_init(&ctx_haval);
    sph_haval256_5 (&ctx_haval, ptr, sz);
    sph_haval256_5_close(&ctx_haval, static_cast<void*>(&hash[21]));

    if ((hash[21] & mask) != zero)
    {
		sph_shabal512_init(&ctx_shabal);
		sph_shabal512 (&ctx_shabal, ptr, sz);
		sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[22]));
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[22]));
    }
    
    if ((hash[22] & mask) != zero)
    {
		sph_shabal512_init(&ctx_shabal);
		sph_shabal512 (&ctx_shabal, ptr, sz);
		sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[23]));
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[23]));
    }
    
    if ((hash[23] & mask) != zero)
    {
		sph_shabal512_init(&ctx_shabal);
		sph_shabal512 (&ctx_shabal, ptr, sz);
		sph_shabal512_close(&ctx_shabal, static_cast<void*>(&hash[24]));
	}
    else
    {
		sph_echo512_init(&ctx_echo);
		sph_echo512 (&ctx_echo, ptr, sz);
		sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[24]));
    }
    mpz_t bns[26];
    //Take care of zeros and load gmp
    for(int i=0; i < 25; i++){
	if(hash[i]==0)
	    hash[i] = 1;
	mpz_init(bns[i]);
	mpz_set_uint512(bns[i],hash[i]);
    }

    mpz_init(bns[25]);
    mpz_set_ui(bns[25],0);
    for(int i=0; i < 25; i++)
	mpz_add(bns[25], bns[25], bns[i]);

    mpz_t product;
    mpz_init(product);
    mpz_set_ui(product,1);
//    mpz_pow_ui(bns[25], bns[25], 2);
    for(int i=0; i < 26; i++){
	mpz_mul(product,product,bns[i]);
    }
    mpz_pow_ui(product, product, 2);

    bytes = mpz_sizeinbase(product, 256);
//    printf("M25M data space: %iB\n", bytes);
    char *data = (char*)malloc(bytes);
    mpz_export(data, NULL, -1, 1, 0, 0, product);

    sph_shabal256_init(&ctx_shabal256);
    // ZSHA256;
    sph_shabal256 (&ctx_shabal256, data, bytes);
    sph_shabal256_close(&ctx_shabal256, static_cast<void*>(&finalhash));
//   printf("finalhash = %s\n", hash[6].GetHex().c_str());
    free(data);

    int digits=(int)((sqrt((double)(nnNonce2))*(1.+EPS))/9000+255);
//    int iterations=(int)((sqrt((double)(nnNonce2))+EPS)/500+350); // <= 500
//    int digits=100;
    int iterations=20; // <= 500
    mpf_set_default_prec((long int)(digits*BITS_PER_DIGIT+16));

    mpz_t magipi;
    mpz_t magisw;
    mpf_t magifpi;
    mpf_t mpa1, mpb1, mpt1, mpp1;
    mpf_t mpa2, mpb2, mpt2, mpp2;
    mpf_t mpsft;

    mpz_init(magipi);
    mpz_init(magisw);
    mpf_init(magifpi);
    mpf_init(mpsft);
    mpf_init(mpa1);
    mpf_init(mpb1);
    mpf_init(mpt1);
    mpf_init(mpp1);

    mpf_init(mpa2);
    mpf_init(mpb2);
    mpf_init(mpt2);
    mpf_init(mpp2);

    uint32_t usw_;
    usw_ = sw_(nnNonce2, SW_DIVS);
    if (usw_ < 1) usw_ = 1;
//    if(fDebugMagi) printf("usw_: %d\n", usw_);
    mpz_set_ui(magisw, usw_);
    uint32_t mpzscale=mpz_size(magisw);
for(int i=0; i < NM25M; i++)
{
    if (mpzscale > 1000) {
      mpzscale = 1000;
    }
    else if (mpzscale < 1) {
      mpzscale = 1;
    }
//    if(fDebugMagi) printf("mpzscale: %d\n", mpzscale);

    mpf_set_ui(mpa1, 1);
    mpf_set_ui(mpb1, 2);
    mpf_set_d(mpt1, 0.25*mpzscale);
    mpf_set_ui(mpp1, 1);
    mpf_sqrt(mpb1, mpb1);
    mpf_ui_div(mpb1, 1, mpb1);
    mpf_set_ui(mpsft, 10);

    for(int i=0; i <= iterations; i++)
    {
	mpf_add(mpa2, mpa1,  mpb1);
	mpf_div_ui(mpa2, mpa2, 2);
	mpf_mul(mpb2, mpa1, mpb1);
	mpf_abs(mpb2, mpb2);
	mpf_sqrt(mpb2, mpb2);
	mpf_sub(mpt2, mpa1, mpa2);
	mpf_abs(mpt2, mpt2);
	mpf_sqrt(mpt2, mpt2);
	mpf_mul(mpt2, mpt2, mpp1);
	mpf_sub(mpt2, mpt1, mpt2);
	mpf_mul_ui(mpp2, mpp1, 2);
	mpf_swap(mpa1, mpa2);
	mpf_swap(mpb1, mpb2);
	mpf_swap(mpt1, mpt2);
	mpf_swap(mpp1, mpp2);
    }
    mpf_add(magifpi, mpa1, mpb1);
    mpf_pow_ui(magifpi, magifpi, 2);
    mpf_div_ui(magifpi, magifpi, 4);
    mpf_abs(mpt1, mpt1);
    mpf_div(magifpi, magifpi, mpt1);

//    mpf_out_str(stdout, 10, digits+2, magifpi);

    mpf_pow_ui(mpsft, mpsft, digits/2);
    mpf_mul(magifpi, magifpi, mpsft);

    mpz_set_f(magipi, magifpi);

//mpz_set_ui(magipi,1);

    mpz_add(product,product,magipi);
    mpz_add(product,product,magisw);
    
    if(finalhash==0) finalhash = 1;
    mpz_set_uint256(bns[0],finalhash);
    mpz_add(bns[25], bns[25], bns[0]);

    mpz_mul(product,product,bns[25]);
    mpz_cdiv_q (product, product, bns[0]);
    if (mpz_sgn(product) <= 0) mpz_set_ui(product,1);

    bytes = mpz_sizeinbase(product, 256);
    mpzscale=bytes;
//    printf("M25M data space: %iB\n", bytes);
    char *bdata = (char*)malloc(bytes);
    mpz_export(bdata, NULL, -1, 1, 0, 0, product);

    sph_shabal256_init(&ctx_shabal256);
    // ZSHA256;
    sph_shabal256 (&ctx_shabal256, bdata, bytes);
    sph_shabal256_close(&ctx_shabal256, static_cast<void*>(&finalhash));
    free(bdata);
//    printf("finalhash = %s\n", finalhash.GetHex().c_str());
}
    //Free the memory
    for(int i=0; i < 26; i++){
	mpz_clear(bns[i]);
    }
//    mpz_clear(dSpectralWeight);
    mpz_clear(product);

    mpz_clear(magipi);
    mpz_clear(magisw);
    mpf_clear(magifpi);
    mpf_clear(mpsft);
    mpf_clear(mpa1);
    mpf_clear(mpb1);
    mpf_clear(mpt1);
    mpf_clear(mpp1);

    mpf_clear(mpa2);
    mpf_clear(mpb2);
    mpf_clear(mpt2);
    mpf_clear(mpp2);
    
    return finalhash;
}

#endif // VELVETHASH_H
