#include <ctype.h>

#include <qp.h>

#include <stdio.h>

const char hex_alphabet[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

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
