#include <Locker.h>
#include <String.h>
#include <regex.h>
#include <ctype.h>
#include <stdio.h>
#include <malloc.h>

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

void subject2thread(BString& string)
{
	static char translation[256];
	static re_pattern_buffer *rebuf=NULL, re;
	static BLocker remakelock;
	static size_t nsub = 1;
	if (rebuf==NULL && remakelock.Lock())
	{
		if (rebuf==NULL)
		{
			for (int i=0; i<256; ++i) translation[i]=i;
			for (int i='a'; i<='z'; ++i) translation[i]=toupper(i);
			
			re.translate = translation;
			re.regs_allocated = REGS_FIXED;
			re_syntax_options = RE_SYNTAX_POSIX_EXTENDED;

			char pattern[] = PATTERN;
			// count subexpressions in PATTERN
			for (int i=0; i<sizeof(pattern); ++i)
			{
				if (pattern[i] == '\\') ++i;
				else if (pattern[i] == '(') ++nsub;
			}
			
			const char* err = re_compile_pattern(pattern,sizeof(pattern)-1,&re);
			if (err == NULL) rebuf = &re;
			else fprintf(stderr, "Failed to compile the regex: %s\n", err);
		}
		remakelock.Unlock();
	}
	if (rebuf)
	{
		struct re_registers regs;
		// can't be static if this function is to be thread-safe
		
		regs.num_regs = nsub;
		regs.start = (regoff_t*)malloc(nsub*sizeof(regoff_t));
		regs.end = (regoff_t*)malloc(nsub*sizeof(regoff_t));
		
		for (int start=0;
		    (start=re_search(rebuf, string.String(), string.Length(),
							0, string.Length(), &regs)) >= 0;
			)
		{
			//
			// we found something
			//
			
			// don't delete [bemaildaemon]...
			if (start == regs.start[1]) start = regs.start[2];
			
			string.Remove(start,regs.end[0]-start);
			if (start) string.Insert(' ',1,start);
		}

		free(regs.start);
		free(regs.end);
	}
}
