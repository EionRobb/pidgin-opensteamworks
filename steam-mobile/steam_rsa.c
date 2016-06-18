/*
 *  Steam Mobile Plugin for Pidgin
 *  Copyright (C) 2012-2016 Eion Robb
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
https://steamcommunity.com/mobilelogin/getrsakey?username=<steamusername>
{"success":true,"publickey_mod":"pubkeyhex","publickey_exp":"pubkeyhex","timestamp":"165685150000"}


https://steamcommunity.com/mobilelogin/dologin/

password=<base64rsaencryptedpwd>&username=<steamusername>&emailauth=&captchagid=-1&captcha_text=&emailsteamid=&rsatimestamp=165685150000&remember_login=true&donotcache=1368831657863

*/

#if defined USE_OPENSSL_CRYPTO && !(defined __APPLE__ || defined __OpenBSD__)
#	undef USE_OPENSSL_CRYPTO
#endif

#if !defined USE_MBEDTLS_CRYPTO && !defined USE_OPENSSL_CRYPTO && !defined USE_NSS_CRYPTO && !defined USE_GCRYPT_CRYPTO
// #	ifdef _WIN32
// #		define USE_WIN32_CRYPTO
// #	else
#		define USE_NSS_CRYPTO
// #	endif
#endif

#ifdef USE_NSS_CRYPTO

#include <nss.h>
#include <keyhi.h>
#include <keythi.h>
#include <pk11pub.h>
#include <secdert.h>

// Coverts a hex string, eg "ABCD0123" into "\xAB\xCD\x01\x23"
// The length of the returned char* will always be half of that of the input string
guchar *
hexstring_to_binary(const gchar *in_string) {
	guint in_len = strlen(in_string);
	unsigned char *output;
	guint pos, count;
	guint output_len;
	
	output_len = in_len / 2;
	output = g_new0(unsigned char, output_len + 10);
	
	pos = 0;
	for(count = 0; count < output_len; count++) {
		sscanf(&in_string[pos], "%2hhx", &output[count]);
		pos += 2;
	}
	
	return output;
}

guchar *
pkcs1pad2(const char *data, int keysize)
{
	guchar *buffer = g_new0(guchar, keysize);
	
	int len = strlen(data) - 1;
	
	while(len >=0 && keysize > 0)
		buffer[--keysize] = (unsigned char)data[len--];
	buffer[--keysize] = 0;
	srand( time(NULL) );
	while(keysize > 2)
		buffer[--keysize] = (rand() % 254) + 1;
	buffer[--keysize] = 2;
	buffer[--keysize] = 0;
	
	return buffer;
}


gchar *
steam_encrypt_password(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
{
	SECItem derPubKey;
	SECKEYPublicKey *pubKey;
	PRArenaPool *arena;
	guint modlen = strlen(modulus_str) / 2;
	guint explen = strlen(exponent_str) / 2;
	guchar *temp;
	gchar *output;
	guchar *encrypted;
	//gchar *tmpstr;
	struct MyRSAPublicKey {
		SECItem m_modulus;
		SECItem m_exponent;
	} inPubKey;
	SECStatus rv;

	const SEC_ASN1Template MyRSAPublicKeyTemplate[] = {
		{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(struct MyRSAPublicKey) },
		{ SEC_ASN1_INTEGER, offsetof(struct MyRSAPublicKey, m_modulus), },
		{ SEC_ASN1_INTEGER, offsetof(struct MyRSAPublicKey, m_exponent), },
		{ 0, }
	};
	
	temp = hexstring_to_binary(modulus_str);
	inPubKey.m_modulus.data = (unsigned char *) PORT_Alloc(modlen + 10); 
	memcpy(inPubKey.m_modulus.data, temp, modlen); 
	inPubKey.m_modulus.len = modlen; 
	inPubKey.m_modulus.type = siUnsignedInteger; 
	g_free(temp);
	
	temp = hexstring_to_binary(exponent_str);
	inPubKey.m_exponent.data = (unsigned char *) PORT_Alloc(explen + 10); 
	memcpy(inPubKey.m_exponent.data, temp, explen); 
	inPubKey.m_exponent.len = explen;
	inPubKey.m_exponent.type = siUnsignedInteger;
	g_free(temp);
	
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	SEC_ASN1EncodeItem(arena, &derPubKey, &inPubKey, MyRSAPublicKeyTemplate);
	pubKey = SECKEY_ImportDERPublicKey(&derPubKey, CKK_RSA);
	PORT_FreeArena(arena, PR_FALSE);
	
	encrypted = g_new0(guchar, modlen);
	temp = pkcs1pad2(password, modlen);
	
	/* encrypt password, result will be in encrypted */
	rv = PK11_PubEncryptRaw(pubKey, encrypted, temp, modlen, 0);
	g_free(temp);
	
	if (rv != SECSuccess)
	{
		purple_debug_error("steam", "password encrypt failed\n");
		if (pubKey) SECKEY_DestroyPublicKey(pubKey);
		g_free(encrypted);
		return NULL;
	}
	
	output = purple_base64_encode(encrypted, modlen);
	
	g_free(encrypted);
	
	if (pubKey) SECKEY_DestroyPublicKey(pubKey);
	
	return output;
}

#elif defined USE_GCRYPT_CRYPTO

#include <gcrypt.h>
#include <string.h>

// The following functions steam_util_str_hex2bytes, steam_crypt_rsa_enc and steam_encrypt_password
// (originally steam_crypt_rsa_enc_str) have been taken directly from steam-util.c and steam-crypt.c
// from the bitlbee-steam source code. The original files are released under the GNU General Public
// License version 2 and can be found at https://github.com/jgeboski/bitlbee-steam.
// All credit goes to the original author of bitlbee-steam, James Geboski <jgeboski@gmail.com>.

GByteArray *
steam_util_str_hex2bytes(const gchar *str)
{
  GByteArray *ret;
  gboolean    hax;
  gsize       size;
  gchar       val;
  guint       i;
  guint       d;

  g_return_val_if_fail(str != NULL, NULL);

  size = strlen(str);
  hax  = (size % 2) != 0;

  ret = g_byte_array_new();
  g_byte_array_set_size(ret, (size + 1) / 2);
  memset(ret->data, 0, ret->len);

  for (d = i = 0; i < size; i++, hax = !hax) {
    val = g_ascii_xdigit_value(str[i]);

    if (val < 0) {
      g_byte_array_free(ret, TRUE);
      return NULL;
    }

    if (hax)
      ret->data[d++] |= val & 0x0F;
    else
      ret->data[d] |= (val << 4) & 0xF0;
  }

  return ret;
}

GByteArray *
steam_crypt_rsa_enc(const GByteArray *mod, const GByteArray *exp, const GByteArray *bytes)
{
  GByteArray   *ret;
  gcry_mpi_t    mmpi;
  gcry_mpi_t    empi;
  gcry_mpi_t    dmpi;
  gcry_sexp_t   kata;
  gcry_sexp_t   data;
  gcry_sexp_t   cata;
  gcry_error_t  res;
  gsize         size;

  g_return_val_if_fail(mod   != NULL, NULL);
  g_return_val_if_fail(exp   != NULL, NULL);
  g_return_val_if_fail(bytes != NULL, NULL);

  mmpi = empi = dmpi = NULL;
  kata = data = cata = NULL;
  ret  = NULL;

  res  = gcry_mpi_scan(&mmpi, GCRYMPI_FMT_USG, mod->data, mod->len, NULL);
  res |= gcry_mpi_scan(&empi, GCRYMPI_FMT_USG, exp->data, exp->len, NULL);
  res |= gcry_mpi_scan(&dmpi, GCRYMPI_FMT_USG, bytes->data, bytes->len, NULL);

  if (G_LIKELY(res == 0)) {
    res  = gcry_sexp_build(&kata, NULL, "(public-key(rsa(n %m)(e %m)))", mmpi, empi);
    res |= gcry_sexp_build(&data, NULL, "(data(flags pkcs1)(value %m))", dmpi);

    if (G_LIKELY(res == 0)) {
      res = gcry_pk_encrypt(&cata, data, kata);

      if (G_LIKELY(res == 0)) {
        gcry_sexp_release(data);
        data = gcry_sexp_find_token(cata, "a", 0);

        if (G_LIKELY(data != NULL)) {
          gcry_mpi_release(dmpi);
          dmpi = gcry_sexp_nth_mpi(data, 1, GCRYMPI_FMT_USG);

          if (G_LIKELY(dmpi != NULL)) {
            ret = g_byte_array_new();
            g_byte_array_set_size(ret, mod->len);

            gcry_mpi_print(GCRYMPI_FMT_USG, ret->data, ret->len, &size, dmpi);

            g_warn_if_fail(size <= mod->len);
            g_byte_array_set_size(ret, size);
          } else {
            g_warn_if_reached();
          }
        } else {
          g_warn_if_reached();
        }
      }
    }
  }

  gcry_sexp_release(cata);
  gcry_sexp_release(data);
  gcry_sexp_release(kata);

  gcry_mpi_release(dmpi);
  gcry_mpi_release(empi);
  gcry_mpi_release(mmpi);

  return ret;
}

gchar *
steam_encrypt_password(const gchar *mod, const gchar *exp, const gchar *str)
{
  GByteArray *bytes;
  GByteArray *mytes;
  GByteArray *eytes;
  GByteArray *enc;
  gchar      *ret;

  g_return_val_if_fail(mod != NULL, NULL);
  g_return_val_if_fail(exp != NULL, NULL);
  g_return_val_if_fail(str != NULL, NULL);

  mytes = steam_util_str_hex2bytes(mod);

  if (G_UNLIKELY(mytes == NULL))
    return NULL;

  eytes = steam_util_str_hex2bytes(exp);

  if (G_UNLIKELY(eytes == NULL)) {
    g_byte_array_free(mytes, TRUE);
    return NULL;
  }

  bytes = g_byte_array_new();
  g_byte_array_append(bytes, (guint8*) str, strlen(str));
  enc = steam_crypt_rsa_enc(mytes, eytes, bytes);

  g_byte_array_free(bytes, TRUE);
  g_byte_array_free(eytes, TRUE);
  g_byte_array_free(mytes, TRUE);

  if (G_UNLIKELY(enc == NULL))
    return NULL;

  ret = g_base64_encode(enc->data, enc->len);
  g_byte_array_free(enc, TRUE);

  return ret;
}

#elif defined USE_MBEDTLS_CRYPTO

#include "mbedtls/config.h"
#include "mbedtls/rsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

gchar *
steam_encrypt_password(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
{
	mbedtls_rsa_context rsa;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	int ret;
	guchar *encrypted_password;
	gchar *output;

	// Init entropy context
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
	if (ret != 0) {
		purple_debug_error("steam", "failed to init entropy context, error=%d\n", ret);
		return NULL;
	}

	// Init mbedtls rsa
	mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);

	// Read modulus
	ret = mbedtls_mpi_read_string(&rsa.N, 16, modulus_str);
	if (ret != 0) {
		purple_debug_error("steam", "modulus parsing failed, error=%d\n", ret);
		return NULL;
	}

	// Read exponent
	ret = mbedtls_mpi_read_string(&rsa.E, 16, exponent_str);
	if (ret != 0) {
		purple_debug_error("steam", "exponent parsing failed, error=%d\n", ret);
		return NULL;
	}

	// Set RSA key length
	rsa.len = (mbedtls_mpi_bitlen(&rsa.N) + 7) >> 3;

	// Allocate space for encrypted password
	encrypted_password = g_new0(guchar, rsa.len);

	ret = mbedtls_rsa_pkcs1_encrypt(&rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC, strlen(password), (unsigned char*)password, encrypted_password);

	if (ret != 0) {
		purple_debug_error("steam", "password encryption failed, error=%d\n", ret);
		g_free(encrypted_password);
		return NULL;
	}

	output = purple_base64_encode(encrypted_password, (int)rsa.len);
	g_free(encrypted_password);

	return output;
}

#elif defined USE_WIN32_CRYPTO

#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS
#include <wincrypt.h>
#include <tchar.h>
#define SECURITY_WIN32
#include <security.h>

gchar *
steam_encrypt_password(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
{
	DWORD cchModulus = (DWORD)strlen(modulus_str);
	int i;
	BYTE *pbBuffer = 0;
	BYTE *pKeyBlob = 0;
	HCRYPTKEY phKey = 0;
	HCRYPTPROV hCSP = 0;
	
	// convert hex string to byte array
	DWORD cbLen = 0, dwSkip = 0, dwFlags = 0;
	if (!CryptStringToBinaryA(modulus_str, cchModulus, CRYPT_STRING_HEX, NULL, &cbLen, &dwSkip, &dwFlags))
	{
		purple_debug_error("steam", "password encryption failed, cant get length of modulus, error=%d\n", GetLastError());
		return NULL;
	}
	
	// allocate a new buffer.
	pbBuffer = (BYTE*)malloc(cbLen);
	if (!CryptStringToBinaryA(modulus_str, cchModulus, CRYPT_STRING_HEX, pbBuffer, &cbLen, &dwSkip, &dwFlags))
	{
		purple_debug_error("steam", "password encryption failed, cant get modulus, error=%d\n", GetLastError());
		free(pbBuffer);
		return NULL;
	}
	
	// reverse byte array
	for (i = 0; i < (int)(cbLen / 2); ++i)
	{
		BYTE temp = pbBuffer[cbLen - i - 1];
		pbBuffer[cbLen - i - 1] = pbBuffer[i];
		pbBuffer[i] = temp;
	}
	
	if (!CryptAcquireContext(&hCSP, NULL, NULL, PROV_RSA_AES, CRYPT_SILENT) &&
			!CryptAcquireContext(&hCSP, NULL, NULL, PROV_RSA_AES, CRYPT_SILENT | CRYPT_NEWKEYSET))
	{
		purple_debug_error("steam", "password encryption failed, cant get a crypt context, error=%d\n", GetLastError());
		free(pbBuffer);
		return NULL;
	}
	
	// Move the key into the key container.
	DWORD cbKeyBlob = sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + cbLen;
	pKeyBlob = (BYTE*)malloc(cbKeyBlob);
	
	// Fill in the data.
	PUBLICKEYSTRUC *pPublicKey = (PUBLICKEYSTRUC*)pKeyBlob;
	pPublicKey->bType = PUBLICKEYBLOB;
	pPublicKey->bVersion = CUR_BLOB_VERSION;  // Always use this value.
	pPublicKey->reserved = 0;                 // Must be zero.
	pPublicKey->aiKeyAlg = CALG_RSA_KEYX;     // RSA public-key key exchange.
	
	// The next block of data is the RSAPUBKEY structure.
	RSAPUBKEY *pRsaPubKey = (RSAPUBKEY*)(pKeyBlob + sizeof(PUBLICKEYSTRUC));
	pRsaPubKey->magic = 0x31415352; // RSA1 // Use public key
	pRsaPubKey->bitlen = cbLen * 8;  // Number of bits in the modulus.
	//pRsaPubKey->pubexp = 0x10001; // "010001" // Exponent.
	pRsaPubKey->pubexp = strtol(exponent_str, NULL, 16);
	
	// Copy the modulus into the blob. Put the modulus directly after the
	// RSAPUBKEY structure in the blob.
	BYTE *pKey = (BYTE*)(((BYTE *)pRsaPubKey) + sizeof(RSAPUBKEY));
	memcpy(pKey, pbBuffer, cbLen);
	
	// Now import public key       
	if (!CryptImportKey(hCSP, pKeyBlob, cbKeyBlob, 0, 0, &phKey))
	{
		purple_debug_error("steam", "password encryption failed, couldnt create key, error=%d\n", GetLastError());
		
		free(pKeyBlob);
		free(pbBuffer);
		CryptReleaseContext(hCSP, 0);
		
		return NULL;
	}
	
	DWORD dataSize = strlen(password);
	DWORD encryptedSize = dataSize;
	
	// get length of encrypted data
	if (!CryptEncrypt(phKey, 0, TRUE, 0, NULL, &encryptedSize, 0))
	{
		gint errorno = GetLastError();
		purple_debug_error("steam", "password encryption failed, couldnt get length of RSA, error=%d %s\n", errorno, g_win32_error_message(errorno));
		
		free(pKeyBlob);
		free(pbBuffer);
		CryptDestroyKey(phKey);
		CryptReleaseContext(hCSP, 0);
		
		return NULL;
	}
	
	BYTE *encryptedData = g_new0(BYTE, encryptedSize);
	
	// encrypt password
	memcpy(encryptedData, password, dataSize);
	if (!CryptEncrypt(phKey, 0, TRUE, 0, encryptedData, &dataSize, encryptedSize))
	{
		purple_debug_error("steam", "password encryption failed, couldnt RSA the thing, error=%d\n", GetLastError());
		
		free(pKeyBlob);
		free(pbBuffer);
		CryptDestroyKey(phKey);
		CryptReleaseContext(hCSP, 0);
		
		return NULL;
	}
	
	// reverse byte array again
	for (i = 0; i < (int)(encryptedSize / 2); ++i)
	{
		BYTE temp = encryptedData[encryptedSize - i - 1];
		encryptedData[encryptedSize - i - 1] = encryptedData[i];
		encryptedData[i] = temp;
	}
	
	free(pKeyBlob);
	CryptDestroyKey(phKey);
	free(pbBuffer);
	CryptReleaseContext(hCSP, 0);
	
	gchar *ret = g_base64_encode(encryptedData, encryptedSize/2);
	g_free(encryptedData);
	return ret;
}

#elif defined USE_OPENSSL_CRYPTO

#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/engine.h>

gchar *
steam_encrypt_password(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
{
	BIGNUM *bn_modulus;
	BIGNUM *bn_exponent;
	RSA *rsa;
	gchar *output = NULL;
	guchar *encrypted;
	int rv;
	
	ERR_load_crypto_strings();
	
	bn_modulus = BN_new();
	rv = BN_hex2bn(&bn_modulus, modulus_str);
	if (rv == 0)
	{
		purple_debug_error("steam", "modulus hext to bignum parse failed\n");
		BN_free(bn_modulus);
		return NULL;
	}
	
	bn_exponent = BN_new();
	rv = BN_hex2bn(&bn_exponent, exponent_str);
	if (rv == 0)
	{
		purple_debug_error("steam", "exponent hex to bignum parse failed\n");
		BN_clear_free(bn_modulus);
		BN_clear_free(bn_exponent);
		return NULL;
	}
	
	rsa = RSA_new();
	if (rsa == NULL)
	{
		purple_debug_error("steam", "RSA structure allocation failed\n");
		BN_free(bn_modulus);
		BN_free(bn_exponent);
		return NULL;
	}
	BN_free(rsa->n);
	rsa->n = bn_modulus;
	BN_free(rsa->e);
	rsa->e = bn_exponent;
	
	encrypted = g_new0(guchar, RSA_size(rsa));
	rv = RSA_public_encrypt((int)(strlen(password)),
                          (const unsigned char *)password,
                          encrypted,
                          rsa,
                          RSA_PKCS1_PADDING);
	if (rv < 0)
	{
		unsigned long error_num = ERR_get_error();
		char *error_str = ERR_error_string(error_num, NULL);
		purple_debug_error("steam", "%s", error_str);
		RSA_free(rsa);
		g_free(encrypted);
		return NULL;
	}
	
	output = purple_base64_encode(encrypted, RSA_size(rsa));
	
	// Cleanup
	RSA_free(rsa);
	ERR_free_strings();
	g_free(encrypted);
	
	return output;
}

#endif
