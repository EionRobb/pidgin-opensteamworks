/*
https://steamcommunity.com/login/getrsakey?username=<steamusername>
{"success":true,"publickey_mod":"pubkeyhex","publickey_exp":"pubkeyhex","timestamp":"165685150000"}


https://steamcommunity.com/login/dologin/

password=<base64rsaencryptedpwd>&username=<steamusername>&emailauth=&captchagid=-1&captcha_text=&emailsteamid=&rsatimestamp=165685150000&remember_login=true&donotcache=1368831657863

*/


#include "nss.h"

// Coverts a hex string, eg "ABCD0123" in "\xAB\xCD\x01\x23"
// The length of the returned char* will always be half of that of the input string
unsigned char *
hexstring_to_binary(const char *in_string) {
	guint in_len = strlen(in_string);
	unsigned char *output;
	const char *pos;
	guint count;
	guint output_len;
	
	output_len = in_len / 2;
	output = g_new0(unsigned char, output_len);
	
	pos = in_string;
	for(count = 0; count < output_len; count++) {
		sscanf(pos, "%2hhx", &output[count]);
		pos += 2;
	}
	
	return output;
}


char *
steam_encrypt_password(const char *modulus_str, const char *exponent_str, const char *password)
{
	SECItem derPubKey;
	SECKEYPublicKey *pubKey;
	PRArenaPool *arena;
	guint modlen = strlen(modulus_str) / 2;
	guint explen = strlen(exponent_str) / 2;
	unsigned char *temp;
	gchar *output;
	guchar *encrypted;
	char *tmpstr;
	struct MyRSAPublicKey {
		SECItem m_modulus;
		SECItem m_exponent;
	} inPubKey;

	const SEC_ASN1Template MyRSAPublicKeyTemplate[] = {
		{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(MyRSAPublicKey) },
		{ SEC_ASN1_INTEGER, offsetof(MyRSAPublicKey, m_modulus), },
		{ SEC_ASN1_INTEGER, offsetof(MyRSAPublicKey, m_exponent), },
		{ 0, }
	};
	
	temp = hexstring_to_binary(modulus_str);
	inPubKey.m_modulus.data = (unsigned char *) PORT_Alloc(modlen); 
	memcpy(inPubKey.m_modulus.data, temp, modlen); 
	inPubKey.m_modulus.len = modlen; 
	inPubKey.m_modulus.type = siUnsignedInteger; 
	g_free(temp);
	
	temp = hexstring_to_binary(exponent_str);
	inPubKey.m_exponent.data = (unsigned char *) PORT_Alloc(explen); 
	memcpy(inPubKey.m_exponent.data, temp, explen); 
	inPubKey.m_exponent.len = explen;
	inPubKey.m_exponent.type = siUnsignedInteger;
	g_free(temp);
	
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	SEC_ASN1EncodeItem(arena, &derPubKey, &inPubKey, MyRSAPublicKeyTemplate);
	pubKey = SECKEY_ImportDERPublicKey(&derPubKey, CKK_RSA);
	PORT_FreeArena(arena, PR_FALSE);
	
	encrypted = g_new0(guchar, modlen);
	
	/* encrypt password, result will be in encrypted */
	rv = PK11_PubEncryptRaw(pubKey, encrypted, password, strlen(password), 0);
	
	tmpstr = NSSBase64_EncodeItem(0, 0, 0, &encrypted);
	output = g_strdup(tmpstr);
	PORT_Free(tmpstr);
	
	g_free(encrypted);
	
	if (pubKey) SECKEY_DestroyPublicKey(pubKey);
	
	return output;
}