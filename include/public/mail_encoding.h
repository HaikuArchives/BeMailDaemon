#ifndef ZOIDBERG_MAIL_ENCODING_H
#define ZOIDBERG_MAIL_ENCODING_H

#include <size_t.h>
#include <sys/types.h>

#define BASE64_LINELENGTH 76

typedef enum {
	base64 				= 'b',
	quoted_printable 	= 'q',
	uuencode			= 'u',
	 // it is invalid to encode something using uuencode
	
	no_encoding			= -1
} mail_encoding;

#ifdef __cplusplus
extern "C" {
#endif 

ssize_t encode(mail_encoding encoding, char *out, const char *in, off_t length, int mask_spaces);
 // the value of mask_spaces only matters for quoted_printable
ssize_t decode(mail_encoding encoding, char *out, const char *in, off_t length);

ssize_t max_encoded_length(mail_encoding encoding, off_t cur_length);

ssize_t	encode_base64(char *out, const char *in, off_t length);
ssize_t	decode_base64(char *out, const char *in, off_t length);

ssize_t	encode_qp(char *out, const char *in, off_t length, int mask_spaces);
ssize_t	decode_qp(char *out, const char *in, off_t length);

ssize_t	uu_decode(char *out, const char *in, off_t length);

#ifdef __cplusplus
}
#endif

#endif // ZOIDBERG_MAIL_ENCODING_H
