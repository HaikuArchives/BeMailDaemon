#include <size_t.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif 

ssize_t	encode_qp(char *out, const char *in, off_t length, int mask_spaces);
ssize_t	decode_qp(char *out, const char *in, off_t length);

#ifdef __cplusplus
}
#endif