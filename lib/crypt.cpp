/* crypt - simple encryption algorithm used for passwords
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <Message.h>

#include <string.h>
#include <bsd_mem.h>
#include <crypt.h>


static const char key[PASSWORD_LENGTH + 1] = "Dr. Zoidberg Enterprises, BeMail";


char *get_passwd(BMessage *msg,const char *name)
{
	char *encryptedPassword;
	ssize_t length;
	if (msg->FindData(name,B_RAW_TYPE,(const void **)&encryptedPassword,&length) < B_OK || !encryptedPassword || length == 0)
		return NULL;

	char *buffer = new char[length];
	passwd_crypt(encryptedPassword,buffer,length);
	
	return buffer;
}


bool set_passwd(BMessage *msg,const char *name,const char *password)
{
	if (!password)
		return false;

	char buffer[password ? strlen(password) + 1 : 0];
	passwd_crypt((char *)password,buffer,sizeof(buffer));

	if (msg->ReplaceData(name,B_RAW_TYPE,buffer,sizeof(buffer)) < B_OK)
		return msg->AddData(name,B_RAW_TYPE,buffer,sizeof(buffer)) >= B_OK; 

	return true;
}


void passwd_crypt(char *in,char *out,int length)
{
	int i;

	bzero(out,length);
	strncpy(out,in,length);
	if (length > PASSWORD_LENGTH)
		length = PASSWORD_LENGTH;

	for (i = 0;i < length;i++)
		out[i] ^= key[i];
}

