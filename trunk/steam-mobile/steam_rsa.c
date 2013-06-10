/*
https://steamcommunity.com/mobilelogin/getrsakey?username=<steamusername>
{"success":true,"publickey_mod":"pubkeyhex","publickey_exp":"pubkeyhex","timestamp":"165685150000"}


https://steamcommunity.com/mobilelogin/dologin/

password=<base64rsaencryptedpwd>&username=<steamusername>&emailauth=&captchagid=-1&captcha_text=&emailsteamid=&rsatimestamp=165685150000&remember_login=true&donotcache=1368831657863

*/


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
