#ifndef GARGOYLE_MAIL_PARSER_H
#define GARGOYLE_MAIL_PARSER_H
#include <stdio.h>
#include <E-mail.h>
#include <TypeConstants.h>

class BMessage;
class BFile;
class BEntry;
class BPositionIO;

// for telling the parser which fields to recognize
struct mail_header_field
{
	const char *rfc_name;
	size_t rfc_size; // strlen(rfc_name)
	
	const char *attr_name;
	type_code attr_type;
	// currently either B_STRING_TYPE and B_TIME_TYPE
};
static const mail_header_field gDefaultFields[] =
{
	{ "To: ", 4,            B_MAIL_ATTR_TO,       B_STRING_TYPE },
	{ "From: ", 6,          B_MAIL_ATTR_FROM,     B_STRING_TYPE },
	{ "Date: ", 6,          B_MAIL_ATTR_WHEN,     B_TIME_TYPE },
	{ "Reply-To: ", 10,     B_MAIL_ATTR_REPLY,    B_STRING_TYPE },
	{ "Subject: ", 9,       B_MAIL_ATTR_SUBJECT,  B_STRING_TYPE },
	{ "Priority: ", 10,     B_MAIL_ATTR_PRIORITY, B_STRING_TYPE },
	{ "Mime-Version: ", 14, B_MAIL_ATTR_MIME,     B_STRING_TYPE },
	{ "Status: ", 14,       B_MAIL_ATTR_STATUS,   B_STRING_TYPE },
	{ NULL, 0,              NULL,                 0 }
};
#define MAIL_HEADERS const mail_header_field *fields = gDefaultFields


void ParseRFC2822String(BMessage *out_fields, const char *msg, MAIL_HEADERS);
void ParseRFC2822File(BMessage *out_fields, FILE*  in, MAIL_HEADERS);
void ParseRFC2822File(BMessage *out_fields, BFile& in, MAIL_HEADERS);
void ParseRFC2822File(BMessage *out, BPositionIO& in, MAIL_HEADERS);
// Parse the header of the RFC2822 message passed in (either as
// a char* or a file reference).  Store an entry named f[].attr_name
// of type f[].attr_type in *out_fields for each f[].rfc_name that
// matches a header field in the message.  If there is more than
// one header that matches, more than one entry will be stored.
// B_STRING_TYPE header fields are converted to UTF-8 as per RFC2047.
//
// If the FILE version is used, the FILE* is assumed to point to the
// beginning of the header in question.  It will be left at the end
// of the header, following the empty-line terminator.

status_t SetMailAttrs(BFile*, const char* name = NULL, MAIL_HEADERS);
// Given a BEntry* and/or BFile* for a file containing an RFC2822 mail
// message, set attributes as returned by ParseRFC2822File() above, set
// the attributres B_MAIL_ATTR_HEADER and B_MAIL_ATTR_CONTENT to the
// length of the header and content (respectively), set the file's
// MIME type to B_MAIL_TYPE, and set B_MAIL_ATTR_STATUS to "New"
// if no Status: header was found.  In addition, if name!=NULL set
// the attribute B_MAIL_ATTR_NAME from the attribute whose name was
// passed as 'name' (phew!).

#undef MAIL_HEADERS
#endif
