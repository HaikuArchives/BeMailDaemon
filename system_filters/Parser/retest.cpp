#include <stdio.h>
#include <String.h>
void subject2thread(BString&);

int main(int argc, char** argv)
{
	BString string;
	char buf[1024];
	while (gets(buf))
	{
		string = buf;
		subject2thread(string);
		puts(string.String());
	}
	return 0;
}
