#ifndef ZOIDBERG_MDR_LANGUAGE_H
#define ZOIDBERG_MDR_LANGUAGE_H
/*
** Mail Daemon Replacement interim International Language Macros.
**
** Copyright 2003 Dr. Zoidberg Enterprises.  All rights reserved.
**
** The input for the language macro system is the MDR_DIALECT define which is
** set by the makefile (you compile a version for the desired language, it
** can't change at run-time using this temporary internationalization system).
** MDR_DIALECT was set to 0 for English-UK, 1 for Japanese, and other numbers
** for other language dialects.  If it's not present, we use English-UK.
**
** $Log$
** Revision 1.1  2003/01/30 23:26:07  agmsmith
** Starting to add a simplistic internationalization system for "Koki",
** who wants to use BeMail in Japanese.  It should later be replaced by
** a more comprehensive system, but at least this one will mark out the
** spots where translated text is needed.
*/

#define MDR_DIALECT_ENGLISH_UK 0
#define MDR_DIALECT_JAPANESE 1

#ifndef MDR_DIALECT
	#define MDR_DIALECT MDR_DIALECT_ENGLISH_UK
#endif

#if (MDR_DIALECT == MDR_DIALECT_ENGLISH_UK)
	#define MDR_DIALECT_CHOICE(EnglishUK,Japanese) EnglishUK
	#define MDR_COUNTRY "United Kingdom"
	#define MDR_LANGUAGE "English"
#elif (MDR_DIALECT == MDR_DIALECT_JAPANESE)
	#define MDR_DIALECT_CHOICE(EnglishUK,Japanese) Japanese
	#define MDR_COUNTRY "Japan"
	#define MDR_LANGUAGE "Japanese"
#else
	#error "Unrecognized value specified for MDR_DIALECT macro constant."
#endif

#endif	/* ZOIDBERG_MDR_LANGUAGE_H */
