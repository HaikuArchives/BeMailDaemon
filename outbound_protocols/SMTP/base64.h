#ifndef __BASE64_H__
#define __BASE64_H__
#ifdef __cplusplus
extern "C"{
#endif

ssize_t decode64(char *out,char *in, unsigned inlen);
ssize_t encode64(char *_out,char *_in, unsigned inlen);

#ifdef __cplusplus
};
#endif
#endif /* __BASE64_H__ */
