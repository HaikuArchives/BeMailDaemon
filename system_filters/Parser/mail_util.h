#ifndef GARGOYLE_MAIL_UTIL_H
#define GARGOYLE_MAIL_UTIL_H
#include <stdio.h>

class BString;

size_t unqp(char *data, size_t dataLen);
// convert text inplace

void StripGook(BString* header);
// Given a header field with gobbledygook in it, find the
// longest human-readable phrase.

ssize_t rfc2047_to_utf8(char **buffer, size_t *bufLen, size_t strLen = 0);
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
// return in *buffer a \n-terminated line from *header or file,
// after folding \r?\n(\s)->$1.  return the length of the folded
// string directly, or -1 if there was a memory allocation error.
// *header and file are left pointing to the first character
// after the line read.
//
// if buffer is not NULL return a pointer to the buffer in *buffer
// if *buffer is not NULL, use the preallocated buffer
// if buflen is not NULL, return the final size of the buffer in buflen
// if buffer is not NULL, buflen is not NULL, and *buffer is not NULL
//   *buffer is a buffer of size *buflen
// if buffer is NULL or *buffer is NULL, and buflen is not NULL
//   start with a buffer of size *buflen
// if the end of *header or EOF is encountered,
//   the last byte read ((*buffer)[retval-1]) might not be a newline.


#endif
