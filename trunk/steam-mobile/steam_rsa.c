/*
https://steamcommunity.com/mobilelogin/getrsakey?username=<steamusername>
{"success":true,"publickey_mod":"pubkeyhex","publickey_exp":"pubkeyhex","timestamp":"165685150000"}


https://steamcommunity.com/mobilelogin/dologin/

password=<base64rsaencryptedpwd>&username=<steamusername>&emailauth=&captchagid=-1&captcha_text=&emailsteamid=&rsatimestamp=165685150000&remember_login=true&donotcache=1368831657863

*/

#if defined USE_OPENSSL_CRYPTO && !defined __APPLE__
#undef USE_OPENSSL_CRYPTO
#endif

#if !defined USE_POLARSSL_CRYPTO && !defined USE_OPENSSL_CRYPTO && !defined USE_NSS_CRYPTO
#define USE_NSS_CRYPTO
#endif

#ifdef USE_NSS_CRYPTO

#include <nss.h>
#include <base64.h>
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
	int abs_len = keysize;
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
	
	/*tmpstr = BTOA_DataToAscii(encrypted, modlen);
	output = g_strdup(tmpstr);
	PORT_Free(tmpstr);*/
	output = purple_base64_encode(encrypted, modlen);
	
	g_free(encrypted);
	
	if (pubKey) SECKEY_DestroyPublicKey(pubKey);
	
	return output;
}

#elif defined USE_POLARSSL_CRYPTO

#include "polarssl/config.h"
#include "polarssl/rsa.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"

gchar *
steam_encrypt_password(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
{
	rsa_context rsa;
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	int ret;
	guchar *encrypted_password;
	gchar *output;

	// Init entropy context
	entropy_init(&entropy);
	ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, NULL, 0);

	if (ret != 0) {
		purple_debug_error("steam", "RSA init failed, error=%d\n", ret);
		return NULL;
	}

	// Init polarssl rsa
	rsa_init(&rsa, RSA_PKCS_V15, 0);

	// Read modulus
	ret = mpi_read_string(&rsa.N, 16, modulus_str);
	if (ret != 0) {
		purple_debug_error("steam", "modulus parsing failed, error=%d\n", ret);
		return NULL;
	}

	// Read exponent
	ret = mpi_read_string(&rsa.E, 16, exponent_str);
	if (ret != 0) {
		purple_debug_error("steam", "exponent parsing failed, error=%d\n", ret);
		return NULL;
	}

	// Set RSA key length
	rsa.len = ( mpi_msb( &rsa.N ) + 7 ) >> 3;

	// Allocate space for encrypted password
	encrypted_password = g_new0(guchar, rsa.len);

	ret = rsa_pkcs1_encrypt(&rsa, ctr_drbg_random, &ctr_drbg, RSA_PUBLIC, strlen(password), (unsigned char*)password, encrypted_password);

	if (ret != 0) {
		purple_debug_error("steam", "password encryption failed, error=%d\n", ret);
		g_free(encrypted_password);
		return NULL;
	}

	output = purple_base64_encode(encrypted_password, (int)rsa.len);
	g_free(encrypted_password);

	return output;
}

#elif defined USE_OPENSSL_CRYPTO

#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/engine.h>

gchar *
steam_encrypt_password_openssl(const gchar *modulus_str, const gchar *exponent_str, const gchar *password)
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
		purple_debug_error("steam", error_str);
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