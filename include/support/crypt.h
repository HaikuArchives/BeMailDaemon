#ifndef CRYPT_H
#define CRYPT_H
/* crypt - simple encryption algorithm used for passwords
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#define PASSWORD_LENGTH 32


char *get_passwd(BMessage *msg,const char *name);
bool set_passwd(BMessage *msg,const char *name,const char *password);

void passwd_crypt(char *in,char *out,int length);

#endif	/* CRYPT_H */
