#include "mail_parser.h"
#include "mail_util.h"
#include "NodeMessage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <Application.h>
#include <Directory.h>
#include <File.h>
#include <Entry.h>
#include <Path.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <parsedate.h>
#include <String.h>

//------------All this mail parser stuff is c/o Gargoyle


//--------------------------------------------------------------------

status_t SetMailAttrs(BFile *file, const char* nameheader, const mail_header_field* fields)
{
	file->Seek(0,SEEK_SET);
	status_t	result = file->InitCheck();
	if (result != B_NO_ERROR) return result;
	
	FILE *		f = fdopen(file->Dup(), "r");
	if (f==NULL) return errno;
	
	if (nameheader==NULL) nameheader = B_MAIL_ATTR_FROM;
	
	//
	// Read in the header
	//
	BMessage	header;
	//
	// Parse the header
	//
	ParseRFC2822File(&header, f, fields);
	
	//
	// end of the header; save derivative fields:
	//
	char name[B_FILE_NAME_LENGTH];
	{
		// human-readable name
		BString h;
		if (header.FindString(nameheader,0,&h)==B_OK)
		{
			StripGook(&h);
			header.AddString(B_MAIL_ATTR_NAME,h);
		}
		strncpy(name,h.String(),sizeof(name));
	}
	 
	{
		// header and content sizes
		off_t offset;
		off_t offset2;
		
		fgetpos(f, &offset);
		fseek(f,0,SEEK_END);
		fgetpos(f, &offset2);
		fclose(f);
		
		// XXX these should be INT64s
		header.AddInt32(B_MAIL_ATTR_HEADER, (int32)offset);
		header.AddInt32(B_MAIL_ATTR_CONTENT, (int32)(offset2-offset));
	}
	
	// old code had Status starting as "New"
	// how should this work?
	if (!header.HasString(B_MAIL_ATTR_STATUS))
		header.AddString(B_MAIL_ATTR_STATUS, "New");
	
	// write attributes
	BNode& node = *file;

	BNodeInfo info(file);
	info.SetType(B_MAIL_TYPE);
	
	node << header;
	
	return B_OK;
}

void ParseRFC2822String(BMessage *out, const char *header, const mail_header_field* fields)
{
	char *		buf = NULL;
	size_t		buflen = 0;
	int32		len;
	time_t		when;
	
	//
	// Parse the header
	//
	while ((len = nextfoldedline(&header, &buf, &buflen)) > 2)
	{
		if (buf[len-2] == '\r') len -= 2;
		else if (buf[len-1] == '\n') --len;
		
		// convert to UTF-8
		len = rfc2047_to_utf8(&buf, &buflen, len);
		
		// terminate
		buf[len] = 0;
		
		
		//
		// handle normal fields (string attributes)
		//
		for (int i=0; fields[i].rfc_name; ++i)
		{
			if (strncasecmp(buf, fields[i].rfc_name, fields[i].rfc_size))
				continue;
			
			switch (fields[i].attr_type){
			case B_STRING_TYPE:
				out->AddString(fields[i].attr_name, buf + fields[i].rfc_size);
				break;
			
			case B_TIME_TYPE:
				when = parsedate(buf + fields[i].rfc_size, time((time_t *)NULL));
				if (when == -1) when = time((time_t *)NULL);
				out->AddData(B_MAIL_ATTR_WHEN, B_TIME_TYPE, &when, sizeof(when));
				break;
			}
		}
	} // elihw (len>2)
	
	free(buf);
}

void ParseRFC2822File(BMessage *out, FILE *in, const mail_header_field* fields)
{
	char *		buf = NULL;
	size_t		buflen = 0;
	int32		len;
	time_t		when;
	
	//
	// Parse the header
	//
	while ((len = readfoldedline(in, &buf, &buflen)) > 2)
	{
		if (buf[len-2] == '\r') len -= 2;
		else if (buf[len-1] == '\n') --len;
		
		// convert to UTF-8
		len = rfc2047_to_utf8(&buf, &buflen, len);
		
		// terminate
		buf[len] = 0;
		
		
		//
		// handle normal fields (string attributes)
		//
		for (int i=0; fields[i].rfc_name; ++i)
		{
			if (strncasecmp(buf, fields[i].rfc_name, fields[i].rfc_size))
				continue;
			
			switch (fields[i].attr_type){
			case B_STRING_TYPE:
				out->AddString(fields[i].attr_name, buf + fields[i].rfc_size);
				break;
			
			case B_TIME_TYPE:
				when = parsedate(buf + fields[i].rfc_size, time((time_t *)NULL));
				if (when == -1) when = time((time_t *)NULL);
				out->AddData(B_MAIL_ATTR_WHEN, B_TIME_TYPE, &when, sizeof(when));
				break;
			}
		}
	} // elihw (len>2)
	
	free(buf);
}


void ParseRFC2822File(BMessage *out, BPositionIO& in, const mail_header_field* fields)
{
	char *		buf = NULL;
	size_t		buflen = 0;
	int32		len;
	time_t		when;
	
	char byte;
	in.Read(&byte,1);
	in.Seek(SEEK_SET,0);
	
	//
	// Parse the header
	//
	while ((len = readfoldedline(in, &buf, &buflen)) > 2)
	{
		puts(buf);
		if (buf[len-2] == '\r') len -= 2;
		else if (buf[len-1] == '\n') --len;
		
		// convert to UTF-8
		len = rfc2047_to_utf8(&buf, &buflen, len);
		
		// terminate
		buf[len] = 0;
		
		
		//
		// handle normal fields (string attributes)
		//
		for (int i=0; fields[i].rfc_name; ++i)
		{
			if (strncasecmp(buf, fields[i].rfc_name, fields[i].rfc_size))
				continue;
			
			switch (fields[i].attr_type){
			case B_STRING_TYPE:
				out->AddString(fields[i].attr_name, buf + fields[i].rfc_size);
				break;
			
			case B_TIME_TYPE:
				when = parsedate(buf + fields[i].rfc_size, time((time_t *)NULL));
				if (when == -1) when = time((time_t *)NULL);
				out->AddData(B_MAIL_ATTR_WHEN, B_TIME_TYPE, &when, sizeof(when));
				break;
			}
		}
	} // elihw (len>2)
	
	free(buf);
}

void ParseRFC2822File(BMessage *out, BFile& in, const mail_header_field* fields)
{
	FILE *f = fdopen(in.Dup(), "r");
	if (f==NULL) return;

	ParseRFC2822File(out,f,fields);
	fclose(f);
}
