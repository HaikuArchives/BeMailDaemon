#include <ctype.h>

#include <qp.h>

_EXPORT ssize_t	decode_qp(char *out, char *in, off_t length)
{
	// decode Quoted Printable
	char *dataout = out, *dataend = in+length;
	char *datain = in;
	
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
			} else if (datain[1]==12 && datain[2]==10) {
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