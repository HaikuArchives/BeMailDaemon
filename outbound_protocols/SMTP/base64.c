#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Debug.h>

static char index_64[128] = {
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
	52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6,  7, 8, 9,10,  11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
	-1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

#define CHAR64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

ssize_t encode64(char *out,char *_in, unsigned inlen)
{
	unsigned char oval;
	int len;
	unsigned char *in;
	char *buf;
	static char basis64[] =	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????";
 	
 	buf = malloc(inlen+1);
 	in = (unsigned char*)buf;
	
 	memcpy(buf,_in,inlen);
 	buf[inlen] = '\0';
 	
 	len = 0;
 	
 	for (; inlen >= 3; inlen -= 3){
        *out++ = basis64[in[0] >> 2];
        *out++ = basis64[((in[0] << 4) & 0x30) | (in[1] >> 4)];
        *out++ = basis64[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
        *out++ = basis64[in[2] & 0x3f];
     	len+=4;
        in += 3;
    }
    if (inlen > 0) {
        *out++ = basis64[in[0] >> 2];
        oval = (in[0] << 4) & 0x30;
        if (inlen > 1) oval |= in[1] >> 4;
        *out++ = basis64[oval];
        *out++ = (inlen < 2) ? '=' : basis64[(in[1] << 2) & 0x3c];
        *out++ = '=';
        len += 4;
    }
    *out = '\0';
    free(buf);
    return len;
}


ssize_t decode64(char *out,const char *_in, unsigned inlen)
{
	unsigned lup;
	ssize_t len;
	int c1, c2, c3, c4;
	char *buf;
	unsigned char *in;
 	unsigned i,k;
 	
 	buf = malloc(inlen+1);
 	in = (unsigned char*)buf;
	
 	k=0;
 	// Eliminate invalid charactors.
	for(i = 0;i < inlen;i++)
 	{
 		c1 = _in[i];
 		if(CHAR64(c1) == -1)
 			continue;
 		buf[k++] = c1;
 	}
 	buf[k] = '\0';
 	len = 0;
 	inlen = k;
    
    if (in[0] == '+' && in[1] == ' ') in+=2;
	
	for (lup=0;lup<inlen/4;lup++)
    {
    	if(*in == '\r'|| *in == '\n') 
        { 
          	in++;
          	continue; 
        }
     
        c1 = *in++;
        if(CHAR64(c1) == -1 ) {PRINT(("Invalid1:%d\n",c1));}
        c2 = *in++;
        if(CHAR64(c2) == -1 ) {PRINT(("Invalid2:%d\n",c2));}
        c3 = *in++;
        if(CHAR64(c3) == -1 ) {PRINT(("Invalid3:%d\n",c3));}
        c4 = *in++;
        if(CHAR64(c4) == -1 ) {PRINT(("Invalid4:%d\n",c4));}
        *out++ = (CHAR64(c1) << 2) | (CHAR64(c2) >> 4);
        len++;
        if (c3 != '=')
        {
            *out++ = ((CHAR64(c2) << 4) & 0xf0) | (CHAR64(c3) >> 2);
            len++;
            if (c4 != '=')
            {
                *out++ = ((CHAR64(c3) << 6) & 0xc0) | CHAR64(c4);
                len++;
            }
        }
    }
    *out='\0'; /* terminate string */
 	free(buf);
 	return len;
}
