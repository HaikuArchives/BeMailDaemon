#include <UTF8.h>
#include <String.h>
#include <DataIO.h>
#include <List.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <base64.h>
#include <qp.h>

#include <mail_util.h>

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

static const CharsetConversionEntry charsets[] =
{
	{"iso-8859-1",  B_ISO1_CONVERSION},
	{"iso-8859-2",  B_ISO2_CONVERSION},
	{"iso-8859-3",  B_ISO3_CONVERSION},
	{"iso-8859-4",  B_ISO4_CONVERSION},
	{"iso-8859-5",  B_ISO5_CONVERSION},
	{"iso-8859-6",  B_ISO6_CONVERSION},
	{"iso-8859-7",  B_ISO7_CONVERSION},
	{"iso-8859-8",  B_ISO8_CONVERSION},
	{"iso-8859-9",  B_ISO9_CONVERSION},
	{"iso-8859-10", B_ISO10_CONVERSION},
	{"iso-2022-jp", B_JIS_CONVERSION},
	{"koi8-r",      B_KOI8R_CONVERSION},
	{"iso-8859-13", B_ISO13_CONVERSION},
	{"iso-8859-14", B_ISO14_CONVERSION},
	{"iso-8859-15", B_ISO15_CONVERSION},
	{"Windows-1251",B_MS_WINDOWS_1251_CONVERSION},
	{"Windows-1252",B_MS_WINDOWS_CONVERSION},
	{"dos-866",     B_MS_DOS_866_CONVERSION},
	{"dos-437",     B_MS_DOS_CONVERSION},
	{"euc-kr",      B_EUC_KR_CONVERSION},
	{"x-mac-roman", B_MAC_ROMAN_CONVERSION}
};

_EXPORT ssize_t rfc2047_to_utf8(char **bufp, size_t *bufLen, size_t strLen)
{
	char *string = *bufp;
	char *head, *tail;
	char *charset, *encoding, *end;
	ssize_t ret = B_OK;
	
	if (bufp==NULL || *bufp==NULL)
		return -1;
	
	// set up string length
	if (strLen==0)
		strLen = strlen(*bufp);
	char lastChar = (*bufp)[strLen];
	(*bufp)[strLen] = '\0';
	
	//---------Handle *&&^%*&^ non-RFC compliant, 8bit mail
	{
	int32 state = 0;
	int32 len = strLen;
	//-----Just try
	convert_to_utf8(B_ISO1_CONVERSION,*bufp,&len,*bufp,&len,&state);
	strLen = len;
	}
	
	//---------Whew! Now for RFC compliant mail
	for (head = tail = string;
		(charset = strstr(tail, "=?"))
		&& ((encoding = strchr(charset + 2, '?')) != NULL
			&& encoding[1] && encoding[2] == '?' && encoding[3])
		&& (end = strstr(encoding + 3, "?=")) != NULL;
		// found "=?...charset...?e?...text...?=   (e == encoding)
		//        ^charset       ^encoding    ^end
		tail = end)
	{
		// copy non-encoded text to the output
		if (string != tail && tail != charset)
			memmove(string, tail, charset-tail);
		string += charset-tail;
		tail = charset;
		
		// move things to point at what they should:
		//   =?...charset...?e?...text...?=   (e == encoding)
		//     ^charset      ^encoding     ^end
		charset += 2;
		encoding += 1;
		end += 2;
		
		// find the charset this text is in now
		size_t		cLen = encoding - 1 - charset;
		bool		base64encoded = toupper(*encoding) == 'B';
		
		int i;
		for(i = sizeof(charsets)/sizeof(CharsetConversionEntry) - 1;
			i>=0
			&& (strncasecmp(charset, charsets[i].charset, cLen)
			|| strlen(charsets[i].charset) != cLen);
			--i)
		{	}
		
		if (i < 0)
		{
			// unidentified charset
			// what to do? doing nothing skips the encoded text;
			// but we should keep it: we copy it to the output.
			if (string != tail && tail != end)
				memmove(string, tail, end-tail);
			string += end-tail;
			continue;
		}
		// else we've successfully identified the charset
		
		
		char *src = encoding+2;
		int32 srcLen = end - 2 - src;
		// encoded text: src..src+srcLen
		
		// decode text, get decoded length (reducing xforms)
		srcLen = !base64encoded ? decode_qp(src, src, srcLen)
				: decode_base64(src, src, srcLen);
		
		// allocate space for the converted text
		int32 dstLen = end-string + *bufLen-strLen;
		char *dst = (char*)malloc(dstLen);
		int32 cvLen = srcLen;
		int32 convState = 0;
		
		//
		// do the conversion
		//
		ret = convert_to_utf8(charsets[i].flavor, src, &cvLen, dst, &dstLen, &convState);
		if (ret != B_OK)
		{
			// what to do? doing nothing skips the encoded text
			// but we should keep it: we copy it to the output.
			
			free(dst);
			
			if (string != tail && tail != end)
				memmove(string, tail, end-tail);
			string += end-tail;
			continue;
		}
		/* convert_to_ is either returning something wrong or my
		   test data is screwed up.  Whatever it is, Not Enough
		   Space is not the only cause of the below, so we just
		   assume it succeeds if it converts anything at all.
		else if (cvLen < srcLen)
		{
			// not enough room to convert the data;
			// grow *buf and retry
			
			free(dst);
			
			char *temp = (char*)realloc(*bufp, 2*(*bufLen + 1));
			if (temp == NULL)
			{
				ret = B_NO_MEMORY;
				break;
			}
			
			*bufp = temp;
			*bufLen = 2*(*bufLen + 1);
			
			string = *bufp + (string-head);
			tail = *bufp + (tail-head);
			charset = *bufp + (charset-head);
			encoding = *bufp + (encoding-head);
			end = *bufp + (end-head);
			src = *bufp + (src-head);
			head = *bufp;
			continue;
		}
		*/
		else
		{
			if (dstLen > end-string)
			{
				// copy the string forward...
				memmove(string+dstLen, end, strLen - (end-head) + 1);
				strLen += string+dstLen - end;
				end = string + dstLen;
			}
			
			memcpy(string, dst, dstLen);
			string += dstLen;
			free(dst);
			continue;
		}
	}
	
	// copy everything that's left
	size_t tailLen = strLen - (tail - head);
	memmove(string, tail, tailLen+1);
	string += tailLen;
	
	// replace the last char
	(*bufp)[strLen] = lastChar;
	
	return ret<B_OK? ret:string-head;
}

_EXPORT ssize_t utf8_to_rfc2047 (char **bufp, ssize_t length,uint32 charset, char encoding) {
	struct word {
		const char *begin;
		size_t length;
	
		bool has_8bit;
	};
	
	{
	int32 state = 0;
	int32 len = length*2;
	char *result = (char *)malloc(len);
	//-----Just try
	convert_from_utf8(charset,*bufp,&length,result,&len,&state);
	length = len;
	free(*bufp);
	*bufp = result;
	}
	
	BList words;
	struct word *current;
	
	for (const char *start = *bufp; start < (*bufp+length); start++) {
		current = new struct word;
		current->begin = start;
		current->has_8bit = false;
		
		for (current->length = 0; start[current->length] != 0; current->length++) {
			if (isspace(start[current->length]))
				break;
			if (start[current->length] & (1 << 7))
				current->has_8bit = true;
		}
		
		start += current->length;
		
		words.AddItem(current);
	}
	
	BString rfc2047;
	
	const char *charset_dec = NULL;
	for (int32 i = 0; i < 21; i++) {
		if (charsets[i].flavor == charset) {
			charset_dec = charsets[i].charset;
			break;
		}
	}
	
	struct word *run;
	for (int32 i = 0; (current = (struct word *)words.ItemAt(i)) != NULL; i++) {
		for (int32 g = i+1; (run = (struct word *)words.ItemAt(g)) != NULL; g++) {
			if (run->has_8bit && current->has_8bit) {
				current->length += run->length+1;
				delete words.RemoveItem(g);
				g--;
			} else {
				//i = g;
				break;
			}
		}
	}
	
	while ((current = (struct word *)words.RemoveItem(0L)) != NULL) {
		if (!current->has_8bit) {
			rfc2047.Append(current->begin, current->length + 1);
			delete current;
		} else {
			char *encoded = NULL;
			ssize_t encoded_len = 0;

			switch (encoding) {
				case -1:
					encoded = (char *)current->begin;
					encoded_len = current->length;
					break;
				case 'q':
					encoded = (char *)malloc(current->length * 3);
					encoded_len = encode_qp(encoded,(char *)current->begin,current->length);
					break;
				case 'b':
					encoded = (char *)malloc(current->length * 2);
					encoded_len = encode_base64(encoded,(char *)current->begin,current->length);
					break;
			}
			
			printf("String: %s, len: %ld\n",current->begin,current->length);
			
			rfc2047 << "=?" << charset_dec << '?' << encoding << '?';
			rfc2047.Append(encoded,encoded_len);
			rfc2047 << "?=" << current->begin[current->length];
			
			if (encoding > 0)
				free(encoded);
		}
	}
	
	free(*bufp);
	
	*bufp = (char *)(malloc(rfc2047.Length() + 1));
	strcpy(*bufp,rfc2047.String());
	
	return rfc2047.Length();
}
	

//====================================================================

_EXPORT ssize_t readfoldedline(FILE *file, char **buffer, size_t *buflen)
{
	ssize_t len = buflen && *buflen? *buflen-1:0; // space for \0
	char * buf = buffer && *buffer? *buffer:NULL;
	ssize_t cnt = 0;
	int c;
	
	while ((c=fgetc(file)) != EOF)
	{
		if (buf == NULL || cnt>=len)
		{
			char* temp = (char*)realloc(buf, 2*(len+1));
			if (temp == NULL) break;
			
			len = 2*(len+1) - 1;
			buf = temp;
		}
		
		switch (c){
		case '\n':
			buf[cnt++]=c;
			c = fgetc(file);
			// if next is linear whitespace, fold lines
			if (c == ' ' || c == '\t')
			{
				if (cnt>=2 && buf[cnt-2]=='\r')
					cnt -= 2;
				else --cnt;
				// then fall through
			}
			else
			{
				// we read a line; break out of the loop
				ungetc(c,file);
				c = '\n';
				break;
			}
		
		default:
			buf[cnt++] = c;
		}
		
		if (c=='\n') break;
	}
	if (c==EOF || c=='\n')
		if (buf) buf[cnt]='\0';
	else cnt = -1;
	
	if (buffer) *buffer = buf;
	else if (buf) free(buf);
	
	if (buflen) *buflen = len+1;
	
	return cnt;
}

//====================================================================

_EXPORT ssize_t readfoldedline(BPositionIO &in, char **buffer, size_t *buflen)
{
	ssize_t len = buflen && *buflen? *buflen-1:0; // space for \0
	char * buf = buffer && *buffer? *buffer:NULL;
	ssize_t cnt = 0;
	int8 c;
	
	while (in.Read(&c,1) == 1)
	{
		if (buf == NULL || cnt>=len)
		{
			char* temp = (char*)realloc(buf, 2*(len+1));
			if (temp == NULL) break;
			
			len = 2*(len+1) - 1;
			buf = temp;
		}
		
		switch (c){
		case '\n':
			buf[cnt++]=c;
			in.Read(&c,1);
			// if next is linear whitespace, fold lines
			if (c == ' ' || c == '\t')
			{
				if (cnt>=2 && buf[cnt-2]=='\r')
					cnt -= 2;
				else --cnt;
				// then fall through
			}
			else
			{
				// we read a line; break out of the loop
				//ATT-ungetc(c,file); translates to this, right?
				in.Seek(-1,SEEK_CUR);
				c = '\n';
				break;
			}
		
		default:
			buf[cnt++] = c;
		}
		
		if (c=='\n') break;
	}
	if (c==EOF || c=='\n')
		if (buf) buf[cnt]='\0';
	else cnt = -1;
	
	if (buffer) *buffer = buf;
	else if (buf) free(buf);
	
	if (buflen) *buflen = len+1;
	
	return cnt;
}

_EXPORT ssize_t nextfoldedline(const char** header, char **buffer, size_t *buflen)
{
	ssize_t len = buflen && *buflen? *buflen-1:0; // space for \0
	char * buf = buffer && *buffer? *buffer:NULL;
	ssize_t cnt = 0;
	int c;
	
	while ((c=*(*header)++))
	{
		if (buf == NULL || cnt>=len)
		{
			char* temp = (char*)realloc(buf, 2*(len+1));
			if (temp == NULL) break;
			
			len = 2*(len+1) - 1;
			buf = temp;
		}
		
		switch (c){
		case '\n':
			buf[cnt++]=c;
			c = *(*header)++;
			// if next is linear whitespace, fold lines
			if (c == ' ' || c == '\t')
			{
				if (cnt>=2 && buf[cnt-2]=='\r')
					cnt -= 2;
				else --cnt;
				// then fall through
			}
			else
			{
				// we read a line; break out of the loop
				(*header)--;
				c = '\n';
				break;
			}
		
		default:
			buf[cnt++] = c;
		}
		
		if (c=='\n') break;
	}
	if (c==EOF || c=='\n')
		if (buf) buf[cnt]='\0';
	else cnt = -1;
	
	if (buffer) *buffer = buf;
	else if (buf) free(buf);
	
	if (buflen) *buflen = len+1;
	
	return cnt;
}



_EXPORT void StripGook(BString* header)
{
	//
	// return human-readable name from From: or To: header
	//
	BString name;
	const char *start = header->String();
	const char *stop = start+header->Length()-1;
	
	// Find a string S in the header that matches:
	//   (S)
	//   S <foo>
	//   S <foo> (S)  XXX
	//   "S"
	// or the longest [-\w\.\@]+ string (XXX todo, maybe)
	
	for (int i=0; i<=3; ++i)
	{
		const char *p1 = NULL, *p2 = NULL;
		
		switch (i){
		case 0: if ((p1 = strchr(start,'(')))
				{
					size_t nest = 1;
					for (p2=p1+1; p2<stop && nest; ++p2)
					{
						if (*p2 == ')') --nest;
						else if (*p2 == '(') ++nest;
					}
					++p1; p2 -= 2; // XXX corrects for what?
				}
				break;
		case 1: p1 = start;
				p2 = strchr(start,'<');
				if (p2) --p2;
				break;
		case 2: p1 = strchr(start,'"');
				if (p1 && (p2 = strchr(p1+1,'"'))) { ++p1; --p2; }
				break;
		case 3: p1 = start;
				if (name.Length()==0) p2=stop;
				break;
		}
		
		if (p2)
		{
			while (*p1 && p1<p2 &&
					(isspace(*p1) || *p1 == '"' || *p1 == '\''
					|| *p1 == '(' || *p1 == '<'))
				++p1;
			
			while (p1<p2 &&
					(isspace(*p2) || *p2 == '"' || *p2 == '\''
					|| *p2 == ')' || *p2 == '>'))
				--p2;
			
			if (name.Length() < p2-p1+1) name.SetTo(p1,p2-p1+1);
		}
	} // rof
	
	*header = name;
}

