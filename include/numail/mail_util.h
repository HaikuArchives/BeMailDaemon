#ifndef ZOIDBERG_GARGOYLE_MAIL_UTIL_H
#define ZOIDBERG_GARGOYLE_MAIL_UTIL_H
/* mail util - header parsing
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <stdio.h>

class BString;


namespace Zoidberg {
namespace Mail {

size_t unqp(char *data, size_t dataLen);
// convert text inplace

void StripGook(BString* header);
// Given a header field with gobbledygook in it, find the
// longest human-readable phrase.

ssize_t rfc2047_to_utf8(char **buffer, size_t *bufLen, size_t strLen = 0);
ssize_t utf8_to_rfc2047 (char **bufp, ssize_t length,uint32 charset, char encoding);
// convert (in place) RFC 2047-style escape sequences ("=?...?.?...?=")
// in the first strLen characters of *buffer into UTF-8, and return the
// length of the converted string or an error code less than 0 on error.
//
// This may cause the string to grow.  If it grows bigger than *bufLen,
// *buffer will be reallocated using realloc(), and its new length stored
// in *bufLen.
//
// Unidentified charsets and conversion errors cause
// the offending text to be skipped.

ssize_t nextfoldedline(const char** header, char **buffer, size_t *buflen);
ssize_t readfoldedline(FILE *file, char **buffer, size_t *buflen);
ssize_t readfoldedline(BPositionIO &in, char **buffer, size_t *buflen);
// Return in *buffer a \n-terminated line (even if the original is \r\n
// terminated or not terminated at all (last line in file situation)) from a
// memory buffer, FILE* or BPositionIO, after folding \r?\n(\s)->$1.  Return
// the length of the folded string directly, or a negative error code if there
// was a memory allocation error or file read error.  It will return zero only
// when trying to read at end of file.  *header, *file and &in are left
// pointing to the first character after the line read.
//
// if buffer is not NULL return a pointer to the buffer in *buffer
// if *buffer is not NULL, use the preallocated buffer, though it may get
// realloc'd (so use malloc to allocate it and expect to have your *buffer
// pointer modified to point to the new buffer if a realloc happens).
// if buflen is not NULL, return the final size of the buffer in buflen
// if buffer is not NULL, buflen is not NULL, and *buffer is not NULL
//   *buffer is a buffer of size *buflen
// if buffer is NULL or *buffer is NULL, and buflen is not NULL then
//   start with a buffer of size *buflen

}	// namespace Mail
}	// namespace Zoidberg

#endif	/* ZOIDBERG_GARGOYLE_MAIL_UTIL_H */
