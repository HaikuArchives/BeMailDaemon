#include <size_t.h>
#include <sys/types.h>

#define BASE64_LINELENGTH 76

#ifdef __cplusplus
extern "C" {
#endif 

ssize_t	encode_base64(char *out, char *in, off_t length);
ssize_t	decode_base64(char *out, char *in, off_t length);

#ifdef __cplusplus
}
#endif