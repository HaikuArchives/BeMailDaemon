#include <mail_encoding.h>

#if __POWERPC__
#define encode_base64__local_abi encode_base64__FPcPcx
#define decode_base64__local_abi decode_base64__FPcPcxb
#else
//--------Note, this is for GCC 2 only
#define encode_base64__local_abi encode_base64__FPcT0x
#define decode_base64__local_abi decode_base64__FPcT0xb
#endif

ssize_t encode_base64__local_abi(char *out, char *in, off_t length);
ssize_t decode_base64__local_abi(char *out, char *in, off_t length, char replace_cr);

_EXPORT ssize_t encode_base64__local_abi(char *out, char *in, off_t length) {
	return encode_base64(out,in,length);
}

_EXPORT ssize_t decode_base64__local_abi(char *out, char *in, off_t length, char replace_cr) {
	replace_cr= '\0';
	return decode_base64(out,in,length);
}

