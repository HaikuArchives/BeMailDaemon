#include <base64.h>

#if __POWERPC__
#define encode_base64__local_abi encode_base64__FPcPcx
#define decode_base64__local_abi decode_base64__FPcPcxb
#else
//--------Note, this is for GCC 2 only
#define encode_base64__local_abi encode_base64__FPcT0x
#define decode_base64__local_abi decode_base64__FPcT0xb
#endif

extern "C" _EXPORT ssize_t encode_base64__local_abi(char *out, char *in, off_t length);
extern "C" _EXPORT ssize_t decode_base64__local_abi(char *out, char *in, off_t length, bool replace_cr);

ssize_t encode_base64__local_abi(char *out, char *in, off_t length) {
	return encode_base64(out,in,length);
}

ssize_t decode_base64__local_abi(char *out, char *in, off_t length, bool replace_cr) {
	return decode_base64(out,in,length);
}