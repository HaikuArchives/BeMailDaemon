#include <ctype.h>
#include <string.h>

#include <mail_encoding.h>

#define	DEC(Char) (((Char) - ' ') & 077)

typedef unsigned char uchar;

char base64_alphabet[64] = { //----Fast lookup table
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '+',
  '/'
 };
 
const char hex_alphabet[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

_EXPORT ssize_t encode(mail_encoding encoding, char *out, const char *in, off_t length, int encode_spaces) {
	switch (encoding) {
		case base64:
			return encode_base64(out,in,length);
		case quoted_printable:
			return encode_qp(out,in,length,encode_spaces);
		case no_encoding:
			memcpy(out,in,length);
			return length;
		case uuencode:
		default:
			return -1;
	}
	
	return -1;
}
			
_EXPORT ssize_t decode(mail_encoding encoding, char *out, const char *in, off_t length) {
	switch (encoding) {
		case base64:
			return decode_base64(out,in,length);
		case quoted_printable:
			return decode_qp(out,in,length);
		case uuencode:
			return uu_decode(out,in,length);
		case no_encoding:
			memcpy(out,in,length);
			return length;
	}
	
	return -1;
}

_EXPORT ssize_t max_encoded_length(mail_encoding encoding, off_t cur_length) {
	
	switch (encoding) {
		case base64:
		{
			double result;
			result = cur_length;
			result *= 1.33333333333333;
			result += (result / BASE64_LINELENGTH)*2 + 20;
			return (ssize_t)(result);
		}
		case quoted_printable:
			return cur_length*3;
		case no_encoding:
			return cur_length;
		case uuencode:
		default:
			return -1;
	}
	
	return -1;
}

_EXPORT ssize_t	encode_base64(char *out, const char *in, register off_t length) {
	register unsigned long concat;
	register int i = 0;
	register int k = 0;
	register int curr_linelength = 4; //--4 is a safety extension, designed to cause retirement *before* it actually gets too long
	
	while ( i < length ) {
		concat = ((in[i] & 0xff) << 16);
		
		if ((i+1) < length)
			concat |= ((in[i+1] & 0xff) << 8);
		if ((i+2) < length)
			concat |= (in[i+2] & 0xff);
			
		i += 3;
				
		out[k++] = base64_alphabet[(concat >> 18) & 63];
		out[k++] = base64_alphabet[(concat >> 12) & 63];
		out[k++] = base64_alphabet[(concat >> 6) & 63];
		out[k++] = base64_alphabet[concat & 63];

		if (i >= length) {
			int v;
			for (v = 0; v <= (i - length); v++)
				out[k-v] = '=';
		}

		curr_linelength += 4;
		
		if (curr_linelength > BASE64_LINELENGTH) {
			out[k++] = '\r';
			out[k++] = '\n';
			
			curr_linelength = 4;
		}
	}
	
	return k;
}

_EXPORT  ssize_t	decode_base64(char *out, const char *in, register off_t length) {
		
		register unsigned long concat, value;
		register int i,j;
		register int k = 0;
		
		for (i = 0; i < length; i += 4) {
			concat = 0;
			
			for (j = 0; (j < 4) && ((i + j) < length); j++) {
				value = in[i+j];
				
				if (( value >= 'A' ) && ( value <= 'Z'))
					value -= 'A';
				else if (( value >= 'a' ) && ( value <= 'z'))
					value = value - 'a' + 26;
				else if (( value >= '0' ) && ( value <= '9'))
					value = value - '0' + 52;
				else if ( value == '+' )
					value = 62;
				else if ( value == '/' )
					value = 63;
				else if ( value == '=' )
					break;
				else {
					i += 2;
					j--;
					continue;
				}
				
				value = value << ((3-j)*6);
				
				concat |= value;
			}
			
			if (j > 1)
				out[k++] = (concat & 0x00ff0000) >> 16;
			if (j > 2)
				out[k++] = (concat & 0x0000ff00) >> 8;
			if (j > 3)
				out[k++] = (concat & 0x000000ff);
		}
		
		return k;
}


_EXPORT ssize_t	decode_qp(char *out, const char *in, off_t length)
{
	// decode Quoted Printable
	char *dataout = out;
	const char *datain = in, *dataend = in+length;
	
	while ( datain < dataend )
	{
		if (*datain == '=' && dataend-datain>2)
		{
			int a,b;
			
			a = toupper(datain[1]);
			a -= a>='0' && a<='9'? '0' : (a>='A' && a<='F'? 'A'-10 : a+1);
			
			b = toupper(datain[2]);
			b -= b>='0' && b<='9'? '0' : (b>='A' && b<='F'? 'A'-10 : b+1);
			
			if (a>=0 && b>=0)
			{
				*dataout++ = (a<<4) + b;
				datain += 3;
				continue;
			} else if (datain[1]=='\r' && datain[2]=='\n') {
				// strip =<CR><NL>
				datain += 3;
				continue;
			}
		}
		else if (*datain == '_')
		{
			*dataout++ = ' ';
			++datain;
			continue;
		}
		
		*dataout++ = *datain++;
	}
	
	*dataout = '\0';
	return dataout-out;	
}

_EXPORT ssize_t	encode_qp(register char *out, register const char *in, register off_t length, register int encode_spaces) {
	register int g = 0, i = 0;
	
	for (; i < length; i++) {
		if ((in[i] & (1 << 7)) || (in[i] == '?') || (in[i] == '=') || (in[i] == '_')) {
			out[g++] = '=';
			out[g++] = hex_alphabet[(in[i] >> 4) & 0x0f];
			out[g++] = hex_alphabet[in[i] & 0x0f];
		}
		else if (((in[i] == ' ')  || (in[i] == '\t')) && (encode_spaces))
			out[g++] = '_';
		else
			out[g++] = in[i];
	}

	return g;
}

_EXPORT ssize_t	uu_decode(char *out, const char *in, off_t length) {
	long n;
	uchar *p,*inBuffer = (uchar*)in;
	uchar *outBuffer = (uchar*)out;
	
	inBuffer = (uchar*)strstr((char*)inBuffer,"begin");
	goto enterLoop;
	while (((inBuffer - (uchar *)in) <= length) && strncmp((char*)inBuffer,"end",3)) {
		p = inBuffer;
		n = DEC(inBuffer[0]);
		for (++inBuffer; n>0; inBuffer+=4, n-=3) {
			if (n >= 3) {
				*outBuffer++ = DEC(inBuffer[0]) << 2 | DEC (inBuffer[1]) >> 4;
				*outBuffer++ = DEC(inBuffer[1]) << 4 | DEC (inBuffer[2]) >> 2;
				*outBuffer++ = DEC(inBuffer[2]) << 6 | DEC (inBuffer[3]);
			} else {
				if (n >= 1) *outBuffer++ = DEC(inBuffer[0]) << 2
					| DEC (inBuffer[1]) >> 4;
				if (n >= 2) *outBuffer++ = DEC(inBuffer[1]) << 4
					| DEC (inBuffer[2]) >> 2;
			}
		};
		inBuffer = p;
		enterLoop:
		while ((inBuffer[0] != '\n') && (inBuffer[0] != '\r')
			&& (inBuffer[0] != 0)) inBuffer++;
		while ((inBuffer[0] == '\n') || (inBuffer[0] == '\r')) inBuffer++;
	}
	
	return (ssize_t)(outBuffer - ((uchar*)in));
}