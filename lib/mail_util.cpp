/* mail util - header parsing
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <UTF8.h>
#include <String.h>
#include <Locker.h>
#include <DataIO.h>
#include <List.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <ctype.h>
#include <errno.h>
#include <parsedate.h>

#include <mail_encoding.h>

#include <mail_util.h>


namespace Zoidberg {
namespace Mail {

struct CharsetConversionEntry
{
	const char *charset;
	uint32 flavor;
};

extern const CharsetConversionEntry charsets [] =
{
	// In order of authority, so when searching for the name for a particular
	// numbered conversion, start at the beginning of the array.
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
	{"iso-8859-13", B_ISO13_CONVERSION},
	{"iso-8859-14", B_ISO14_CONVERSION},
	{"iso-8859-15", B_ISO15_CONVERSION},
	{"shift-jis",	B_SJIS_CONVERSION},
	{"iso-2022-jp", B_JIS_CONVERSION},
	{"euc-jp",		B_EUC_CONVERSION},
	{"euc-kr",      B_EUC_KR_CONVERSION}, // Shift encoding 7 bit and KSC-5601 if bit 8 is on.
	{"ksc5601",		B_EUC_KR_CONVERSION}, // Not sure if 7 or 8 bit.
	{"ks_c_5601-1987", B_EUC_KR_CONVERSION}, // Not sure if 7 or 8 bit.
	{"koi8-r",      B_KOI8R_CONVERSION},
	{"windows-1251",B_MS_WINDOWS_1251_CONVERSION},
	{"windows-1252",B_MS_WINDOWS_CONVERSION},
	{"dos-437",     B_MS_DOS_CONVERSION},
	{"dos-866",     B_MS_DOS_866_CONVERSION},
	{"x-mac-roman", B_MAC_ROMAN_CONVERSION},
	/* {"utf-16",		B_UNICODE_CONVERSION}, Doesn't work due to NULs in text */
	{"utf-8",		MDR_UTF8_CONVERSION /* Special code for no conversion */},
	{NULL, (uint32) -1} /* End of list marker, NULL string pointer is the key. */
};


// The next couple of functions are our wrapper around convert_to_utf8 and
// convert_from_utf8 so that they can also convert from UTF-8 to UTF-8 by
// specifying the MDR_UTF8_CONVERSION constant as the conversion operation.

status_t MDR_convert_to_utf8 (
	uint32 srcEncoding,
	const char *src,
	int32 *srcLen,
	char *dst,
	int32 *dstLen,
	int32 *state,
	char substitute)
{
	int32 copyAmount;

	if (srcEncoding == MDR_UTF8_CONVERSION)
	{
		copyAmount = *srcLen;
		if (*dstLen < copyAmount)
			copyAmount = *dstLen;
		memcpy (dst, src, copyAmount);
		*srcLen = copyAmount;
		*dstLen = copyAmount;
		return B_OK;
	}

	return convert_to_utf8 (srcEncoding, src, srcLen, dst, dstLen, state,
		substitute);
}


status_t MDR_convert_from_utf8 (
	uint32 dstEncoding,
	const char *src,
	int32 *srcLen,
	char *dst,
	int32 *dstLen,
	int32 *state,
	char substitute)
{
	int32		copyAmount;
	status_t	errorCode;
	int32		originalDstLen = *dstLen;
	int32		tempDstLen;
	int32		tempSrcLen;

	if (dstEncoding == MDR_UTF8_CONVERSION)
	{
		copyAmount = *srcLen;
		if (*dstLen < copyAmount)
			copyAmount = *dstLen;
		memcpy (dst, src, copyAmount);
		*srcLen = copyAmount;
		*dstLen = copyAmount;
		return B_OK;
	}

	errorCode = convert_from_utf8 (dstEncoding, src, srcLen, dst, dstLen, state, substitute);
	if (errorCode != B_OK)
		return errorCode;

	if (dstEncoding != B_JIS_CONVERSION)
		return B_OK;

	// B_JIS_CONVERSION (ISO-2022-JP) works by shifting between different
	// character subsets.  For E-mail headers (and other uses), it needs to be
	// switched back to ASCII at the end (otherwise the last character gets
	// lost or other weird things happen in the headers).  Note that we can't
	// just append the escape code since the convert_from_utf8 "state" will be
	// wrong.  So we append an ASCII letter and throw it away, leaving just the
	// escape code.  Well, it actually switches to the Roman character set, not
	// ASCII, but that should be OK.

	tempDstLen = originalDstLen - *dstLen;
	if (tempDstLen < 3) // Not enough space remaining in the output.
		return B_OK; // Sort of an error, but we did convert the rest OK.
	tempSrcLen = 1;
	errorCode = convert_from_utf8 (dstEncoding, "a", &tempSrcLen,
		dst + *dstLen, &tempDstLen, state, substitute);
	if (errorCode != B_OK)
		return errorCode;
	*dstLen += tempDstLen - 1 /* don't include the ASCII letter */;
	return B_OK;
}



static int handle_non_rfc2047_encoding(char **buffer,size_t *bufferLength,size_t *sourceLength)
{
	char *string = *buffer;
	int32 length = *sourceLength;
	int32 i;

	// check for 8-bit characters
	for (i = 0;i < length;i++)
		if (string[i] & 0x80)
			break;
	if (i == length)
		return false;

	// check for groups of 8-bit characters - this code is not very smart;
	// it just can detect some sort of single-byte encoded stuff, the rest
	// is regarded as UTF-8

	int32 singletons = 0,doubles = 0;

	for (i = 0;i < length;i++)
	{
		if (string[i] & 0x80)
		{
			if ((string[i + 1] & 0x80) == 0)
				singletons++;
			else doubles++;
			i++;
		}
	}

	if (singletons != 0)	// can't be valid UTF-8 anymore, so we assume ISO-Latin-1
	{
		int32 state = 0;
		// just to be sure
		int32 destLength = length * 4 + 1;
		int32 destBufferLength = destLength;
		char *dest = (char *)malloc(destLength);
		if (dest == NULL)
			return 0;

		if (convert_to_utf8(B_ISO1_CONVERSION,string,&length,dest,&destLength,&state) == B_OK)
		{
			free(*buffer);
			*buffer = dest;
			*bufferLength = destBufferLength;
			*sourceLength = destLength;
			return true;
		}
		free(dest);
		return false;
	}

	// we assume a valid UTF-8 string here, but yes, we don't check it
	return true;
}


_EXPORT ssize_t rfc2047_to_utf8(char **bufp, size_t *bufLen, size_t strLen)
{
	char *string = *bufp;
	char *head, *tail;
	char *charset, *encoding, *end;
	ssize_t ret = B_OK;

	if (bufp == NULL || *bufp == NULL)
		return -1;

	//---------Handle *&&^%*&^ non-RFC compliant, 8bit mail
	if (handle_non_rfc2047_encoding(bufp,bufLen,&strLen))
		return strLen;

	// set up string length
	if (strLen == 0)
		strLen = strlen(*bufp);
	char lastChar = (*bufp)[strLen];
	(*bufp)[strLen] = '\0';

	//---------Whew! Now for RFC compliant mail
	for (head = tail = string;
		((charset = strstr(tail, "=?")) != NULL)
		&& (((encoding = strchr(charset + 2, '?')) != NULL)
			&& encoding[1] && (encoding[2] == '?') && encoding[3])
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
		for (i = 0; charsets[i].charset != NULL; i++)
		{
			if (strncasecmp(charset, charsets[i].charset, cLen) == 0 &&
			strlen(charsets[i].charset) == cLen)
				break;
		}

		if (charsets[i].charset == NULL)
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
		srcLen = !base64encoded ? decode_qp(src, src, srcLen, 1)
				: decode_base64(src, src, srcLen);

		// allocate space for the converted text
		int32 dstLen = end-string + *bufLen-strLen;
		char *dst = (char*)malloc(dstLen);
		int32 cvLen = srcLen;
		int32 convState = 0;

		//
		// do the conversion
		//
		ret = MDR_convert_to_utf8(charsets[i].flavor, src, &cvLen, dst, &dstLen, &convState);
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

	return ret < B_OK ? ret : string-head;
}

_EXPORT ssize_t utf8_to_rfc2047 (char **bufp, ssize_t length,uint32 charset, char encoding) {
	struct word {
		const char *begin;
		size_t length;

		bool has_8bit;
	};

	{
	int32 state = 0;
	int32 len = length*5; // Some character sets bloat up quite a bit, even 5 times.
	char *result = (char *)malloc(len);
	//-----Just try
	MDR_convert_from_utf8(charset,*bufp,&length,result,&len,&state);
	length = len;

	result[length] = 0;

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
			// A control character (0 to 31 decimal), like the escape codes
			// used by ISO-2022-JP, or NULs in UTF-16 probably should be
			// transfered in encoded mode too.  Though ISO-2022-JP is supposed
			// to have the encoded words ending in ASCII mode (it switches
			// between 4 different character sets using escape codes), but that
			// would mean mixing in the character set conversion with the
			// encoding code, which we can't do (or do character set conversion
			// for each word separately?).
			if (start[current->length] >= 0 && start[current->length] < 32)
				current->has_8bit = true;
		}

		start += current->length;

		words.AddItem(current);
	}

	BString rfc2047;

	const char *charset_dec = NULL;
	for (int32 i = 0; charsets[i].charset != NULL; i++) {
		if (charsets[i].flavor == charset) {
			charset_dec = charsets[i].charset;
			break;
		}
	}

	// Combine adjacent words which need encoding into a bigger word.
	struct word *run;
	for (int32 i = 0; (current = (struct word *)words.ItemAt(i)) != NULL; i++) {
		for (int32 g = i+1; (run = (struct word *)words.ItemAt(g)) != NULL; g++) {
			if (run->has_8bit && current->has_8bit) {
				current->length += run->length+1;
				delete (struct word *)words.RemoveItem(g);
				g--;
			} else {
				//i = g;
				break;
			}
		}
	}

	// Encode words which aren't plain ASCII.  Only quoted printable and base64
	// are allowed for header encoding, according to the standards.
	while ((current = (struct word *)words.RemoveItem(0L)) != NULL) {
		if ((encoding != quoted_printable && encoding != base64) ||
		!current->has_8bit) {
			rfc2047.Append(current->begin, current->length + 1);
			delete current;
		} else {
			char *encoded = NULL;
			ssize_t encoded_len = 0;

			switch (encoding) {
				case quoted_printable:
					encoded = (char *)malloc(current->length * 3);
					encoded_len = encode_qp(encoded,current->begin,current->length,true);
					break;
				case base64:
					encoded = (char *)malloc(current->length * 2);
					encoded_len = encode_base64(encoded,current->begin,current->length);
					break;
				default: // Unknown encoding type, shouldn't happen.
					encoded = (char *)current->begin;
					encoded_len = current->length;
					break;
			}

#ifdef DEBUG
			printf("String: %s, len: %ld\n",current->begin,current->length);
#endif

			rfc2047 << "=?" << charset_dec << '?' << encoding << '?';
			rfc2047.Append(encoded,encoded_len);
			rfc2047 << "?=" << current->begin[current->length];

			if (encoding == quoted_printable || encoding == base64)
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
	ssize_t len = buflen && *buflen ? *buflen : 0;
	char * buf = buffer && *buffer ? *buffer : NULL;
	ssize_t cnt = 0; // Number of characters currently in the buffer.
	int c;

	while (true)
	{
		// Make sure there is space in the buffer for two more characters (one
		// for the next character, and one for the end of string NUL byte).
		if (buf == NULL || cnt + 2 >= len)
		{
			char *temp = (char *)realloc(buf, len + 64);
			if (temp == NULL) {
				// Out of memory, however existing buffer remains allocated.
				cnt = ENOMEM;
				break;
			}
			len += 64;
			buf = temp;
		}

		// Read the next character, or end of file, or IO error.
		if ((c = fgetc(file)) == EOF) {
			if (ferror (file)) {
				cnt = errno;
				if (cnt >= 0)
					cnt = -1; // Error codes must be negative.
			} else {
				// Really is end of file.  Also make it end of line if there is
				// some text already read in.  If the first thing read was EOF,
				// just return an empty string.
				if (cnt > 0) {
					buf[cnt++] = '\n';
					if (buf[cnt-2] == '\r') {
						buf[cnt-2] = '\n';
						--cnt;
					}
				}
			}
			break;
		}

		buf[cnt++] = c;

		if (c == '\n') {
			// Convert CRLF end of line to just a LF.  Do it before folding, in
			// case we don't need to fold.
			if (cnt >= 2 && buf[cnt-2] == '\r') {
				buf[cnt-2] = '\n';
				--cnt;
			}
			// If the current line is empty then return it (so that empty lines
			// don't disappear if the next line starts with a space).
			if (cnt <= 1)
				break;
			// Fold if first character on the next line is whitespace.
			c = fgetc(file); // Note it's OK to read EOF and ungetc it too.
			if (c == ' ' || c == '\t')
				buf[cnt-1] = c; // Replace \n with the white space character.
			else {
				// Not folding, we finished reading a line; break out of the loop
				ungetc(c,file);
				break;
			}
		}
	}


	if (buf != NULL && cnt >= 0)
		buf[cnt] = '\0';

	if (buffer)
		*buffer = buf;
	else if (buf)
		free(buf);

	if (buflen)
		*buflen = len;

	return cnt;
}


//====================================================================

_EXPORT ssize_t readfoldedline(BPositionIO &in, char **buffer, size_t *buflen)
{
	ssize_t len = buflen && *buflen ? *buflen : 0;
	char * buf = buffer && *buffer ? *buffer : NULL;
	ssize_t cnt = 0; // Number of characters currently in the buffer.
	char c;
	status_t errorCode;

	while (true)
	{
		// Make sure there is space in the buffer for two more characters (one
		// for the next character, and one for the end of string NUL byte).
		if (buf == NULL || cnt + 2 >= len)
		{
			char *temp = (char *)realloc(buf, len + 64);
			if (temp == NULL) {
				// Out of memory, however existing buffer remains allocated.
				cnt = ENOMEM;
				break;
			}
			len += 64;
			buf = temp;
		}

		errorCode = in.Read (&c,1); // A really slow way of reading - unbuffered.
		if (errorCode != 1) {
			if (errorCode < 0) {
				cnt = errorCode; // IO error encountered, just return the code.
			} else {
				// Really is end of file.  Also make it end of line if there is
				// some text already read in.  If the first thing read was EOF,
				// just return an empty string.
				if (cnt > 0) {
					buf[cnt++] = '\n';
					if (buf[cnt-2] == '\r') {
						buf[cnt-2] = '\n';
						--cnt;
					}
				}
			}
			break;
		}

		buf[cnt++] = c;

		if (c == '\n') {
			// Convert CRLF end of line to just a LF.  Do it before folding, in
			// case we don't need to fold.
			if (cnt >= 2 && buf[cnt-2] == '\r') {
				buf[cnt-2] = '\n';
				--cnt;
			}
			// If the current line is empty then return it (so that empty lines
			// don't disappear if the next line starts with a space).
			if (cnt <= 1)
				break;
			// if first character on the next line is whitespace, fold lines
			errorCode = in.Read(&c,1);
			if (errorCode == 1) {
				if (c == ' ' || c == '\t')
					buf[cnt-1] = c; // Replace \n with the white space character.
				else {
					// Not folding, we finished reading a whole line.
					in.Seek(-1,SEEK_CUR); // Undo the look-ahead character read.
					break;
				}
			} else if (errorCode < 0) {
				cnt = errorCode;
				break;
			} else // No next line; at the end of the file.  Return the line.
				break;
		}
	}

	if (buf != NULL && cnt >= 0)
		buf[cnt] = '\0';

	if (buffer)
		*buffer = buf;
	else if (buf)
		free(buf);

	if (buflen)
		*buflen = len;

	return cnt;
}


_EXPORT ssize_t nextfoldedline(const char** header, char **buffer, size_t *buflen)
{
	ssize_t len = buflen && *buflen ? *buflen : 0;
	char * buf = buffer && *buffer ? *buffer : NULL;
	ssize_t cnt = 0; // Number of characters currently in the buffer.
	char c;

	while (true)
	{
		// Make sure there is space in the buffer for two more characters (one
		// for the next character, and one for the end of string NUL byte).
		if (buf == NULL || cnt + 2 >= len)
		{
			char *temp = (char *)realloc(buf, len + 64);
			if (temp == NULL) {
				// Out of memory, however existing buffer remains allocated.
				cnt = ENOMEM;
				break;
			}
			len += 64;
			buf = temp;
		}

		// Read the next character, or end of file.
		if ((c = *(*header)++) == 0) {
			// End of file.  Also make it end of line if there is some text
			// already read in.  If the first thing read was EOF, just return
			// an empty string.
			if (cnt > 0) {
				buf[cnt++] = '\n';
				if (buf[cnt-2] == '\r') {
					buf[cnt-2] = '\n';
					--cnt;
				}
			}
			break;
		}

		buf[cnt++] = c;

		if (c == '\n') {
			// Convert CRLF end of line to just a LF.  Do it before folding, in
			// case we don't need to fold.
			if (cnt >= 2 && buf[cnt-2] == '\r') {
				buf[cnt-2] = '\n';
				--cnt;
			}
			// If the current line is empty then return it (so that empty lines
			// don't disappear if the next line starts with a space).
			if (cnt <= 1)
				break;
			// if first character on the next line is whitespace, fold lines
			c = *(*header)++;
			if (c == ' ' || c == '\t')
				buf[cnt-1] = c; // Replace \n with the white space character.
			else {
				// Not folding, we finished reading a line; break out of the loop
				(*header)--; // Undo read of the non-whitespace.
				break;
			}
		}
	}


	if (buf != NULL && cnt >= 0)
		buf[cnt] = '\0';

	if (buffer)
		*buffer = buf;
	else if (buf)
		free(buf);

	if (buflen)
		*buflen = len;

	return cnt;
}


_EXPORT void StripGook(BString* header)
{
	//
	// return human-readable name from From: or To: header
	//
	BString name;
	const char *start = header->String();
	const char *stop = start + strlen (start);

	// Find a string S in the header (email foo) that matches:
	//   Old style name in brackets: foo@bar.com (S)
	//   New style quotes: "S" <foo@bar.com>
	//   New style no quotes if nothing else found: S <foo@bar.com>
	//   If nothing else found then use the whole thing: S

	for (int i=0; i<=3; ++i)
	{
		// Set p1 to the first letter in the name and p2 to just past the last
		// letter in the name.  p2 stays NULL if a name wasn't found in this
		// pass.
		const char *p1 = NULL, *p2 = NULL;

		switch (i) {
			case 0: // foo@bar.com (S)
				if ((p1 = strchr(start,'(')) != NULL) {
					p1++; // Advance to first letter in the name.
					size_t nest = 1; // Handle nested brackets.
					for (p2=p1; p2<stop; ++p2)
					{
						if (*p2 == ')')
							--nest;
						else if (*p2 == '(')
							++nest;
						if (nest <= 0)
							break;
					}
					if (nest != 0)
						p2 = NULL; // False alarm, no terminating bracket.
				}
				break;
			case 1: // "S" <foo@bar.com>
				if ((p1 = strchr(start,'\"')) != NULL)
					p2 = strchr (++ /* skip past leading quote */ p1,'\"');
				break;
			case 2: // S <foo@bar.com>
				p1 = start;
				if (name.Length() == 0)
					p2 = strchr (start,'<');
				break;
			case 3: // S
				p1 = start;
				if (name.Length() == 0)
					p2 = stop;
				break;
		}

		// Remove leading and trailing space-like characters and save the
		// result if it is longer than any other likely names found.
		if (p2 != NULL) {
			while (p1 < p2 && (isspace (*p1)))
				++p1;

			while (p1 < p2 && (isspace (p2[-1])))
				--p2;

			int newLength = p2 - p1;
			if (name.Length() < newLength)
				name.SetTo (p1, newLength);
		}
	} // rof

	// Yahoo stupidly inserts the e-mail address into the name string, so this
	// bit of code fixes: "Joe <joe@yahoo.com>" <joe@yahoo.com>

	int32	lessIndex;
	int32	greaterIndex;

	lessIndex = name.FindFirst ('<');
	greaterIndex = name.FindLast ('>');
	if (lessIndex >= 0 && lessIndex < greaterIndex)
		name.Remove (lessIndex, greaterIndex - lessIndex + 1);
	while (name.Length() > 0 && isspace (name[name.Length() - 1]))
		name.Remove (name.Length() - 1, 1); // Remove more trailing spaces.

	*header = name;
}



// Given a subject in a BString, remove the extraneous RE: re: and other stuff
// to get down to the core subject string, which should be identical for all
// messages posted about a topic.  The input string is modified in place to
// become the output core subject string.

static int32				gLocker = 0;
static size_t				gNsub = 1;
static re_pattern_buffer	gRe;
static re_pattern_buffer   *gRebuf = NULL;
static char					gTranslation[256];

_EXPORT void SubjectToThread (BString &string)
{
// a regex that matches a non-ASCII UTF8 character:
#define U8C \
	"[\302-\337][\200-\277]" \
	"|\340[\302-\337][\200-\277]" \
	"|[\341-\357][\200-\277][\200-\277]" \
	"|\360[\220-\277][\200-\277][\200-\277]" \
	"|[\361-\367][\200-\277][\200-\277][\200-\277]" \
	"|\370[\210-\277][\200-\277][\200-\277][\200-\277]" \
	"|[\371-\373][\200-\277][\200-\277][\200-\277][\200-\277]" \
	"|\374[\204-\277][\200-\277][\200-\277][\200-\277][\200-\277]" \
	"|\375[\200-\277][\200-\277][\200-\277][\200-\277][\200-\277]"

#define PATTERN \
	"^ +" \
	"|^(\\[[^]]*\\])(\\<|  +| *(\\<(\\w|" U8C "){2,3} *(\\[[^\\]]*\\])? *:)+ *)" \
	"|^(  +| *(\\<(\\w|" U8C "){2,3} *(\\[[^\\]]*\\])? *:)+ *)" \
	"| *\\(fwd\\) *$"

	if (gRebuf == NULL && atomic_add(&gLocker,1) == 0)
	{
		// the idea is to compile the regexp once to speed up testing

		for (int i=0; i<256; ++i) gTranslation[i]=i;
		for (int i='a'; i<='z'; ++i) gTranslation[i]=toupper(i);

		gRe.translate = gTranslation;
		gRe.regs_allocated = REGS_FIXED;
		re_syntax_options = RE_SYNTAX_POSIX_EXTENDED;

		const char *pattern = PATTERN;
		// count subexpressions in PATTERN
		for (unsigned int i=0; pattern[i] != 0; ++i)
		{
			if (pattern[i] == '\\')
				++i;
			else if (pattern[i] == '(')
				++gNsub;
		}

		const char *err = re_compile_pattern(pattern,strlen(pattern),&gRe);
		if (err == NULL)
			gRebuf = &gRe;
		else
			fprintf(stderr, "Failed to compile the regex: %s\n", err);
	}
	else
	{
		int32 tries = 200;
		while (gRebuf == NULL && tries-- > 0)
			snooze(10000);
	}

	if (gRebuf)
	{
		struct re_registers regs;
		// can't be static if this function is to be thread-safe

		regs.num_regs = gNsub;
		regs.start = (regoff_t*)malloc(gNsub*sizeof(regoff_t));
		regs.end = (regoff_t*)malloc(gNsub*sizeof(regoff_t));

		for (int start=0;
		    (start=re_search(gRebuf, string.String(), string.Length(),
							0, string.Length(), &regs)) >= 0;
			)
		{
			//
			// we found something
			//

			// don't delete [bemaildaemon]...
			if (start == regs.start[1])
				start = regs.start[2];

			string.Remove(start,regs.end[0]-start);
			if (start) string.Insert(' ',1,start);
		}

		free(regs.start);
		free(regs.end);
	}
}



// Converts a date to a time.  Handles numeric time zones too, unlike
// parsedate.  Returns -1 if it fails.

_EXPORT time_t ParseDateWithTimeZone (const char *DateString)
{
	time_t	currentTime;
	time_t	dateAsTime;
	char	tempDateString [80];
	char	tempZoneString [6];
	time_t	zoneDeltaTime;
	int		zoneIndex;
	char   *zonePntr;

	// See if we can remove the time zone portion.  parsedate understands time
	// zone 3 letter names, but doesn't understand the numeric +9999 time zone
	// format.  To do: see if a newer parsedate exists.
	strncpy (tempDateString, DateString, sizeof (tempDateString));
	tempDateString[sizeof (tempDateString) - 1] = 0;

	for (zoneIndex = strlen (tempDateString); zoneIndex >= 0; zoneIndex--)
	{
		zonePntr = tempDateString + zoneIndex;
		if (zonePntr[0] == '+' || zonePntr[0] == '-')
		{
			if (zonePntr[1] >= '0' && zonePntr[1] <= '9' &&
				zonePntr[2] >= '0' && zonePntr[2] <= '9' &&
				zonePntr[3] >= '0' && zonePntr[3] <= '9' &&
				zonePntr[4] >= '0' && zonePntr[4] <= '9')
				break;
		}
	}
	if (zoneIndex >= 0)
	{
		// Remove the zone from the date string and any following time zone
		// letter codes.  Also put in GMT so that the date gets parsed as GMT.
		memcpy (tempZoneString, zonePntr, 5);
		tempZoneString [5] = 0;
		strcpy (zonePntr, "GMT");
	}
	else // No numeric time zone found.
		strcpy (tempZoneString, "+0000");

	time (&currentTime);
	dateAsTime = parsedate (tempDateString, currentTime);
	if (dateAsTime == (time_t) -1)
		return -1; // Failure.

	zoneDeltaTime = 60 * atol (tempZoneString + 3); // Get the last two digits - minutes.
	tempZoneString[3] = 0;
	zoneDeltaTime += atol (tempZoneString + 1) * 60 * 60; // Get the first two digits - hours.
	if (tempZoneString[0] == '+')
		zoneDeltaTime = 0 - zoneDeltaTime;
	dateAsTime += zoneDeltaTime;

	return dateAsTime;
}

}	// namespace Mail
}	// namespace Zoidberg
