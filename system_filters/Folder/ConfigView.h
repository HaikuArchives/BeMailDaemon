#ifndef CONFIG_VIEW
#define CONFIG_VIEW
/* ConfigView - the configuration view for the Folder filter
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <View.h>


class ConfigView : public BView
{
	public:
		ConfigView();
		void SetTo(BMessage *archive,BMessage *metadata);

		virtual	status_t Archive(BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize(float *width, float *height);

	private:
		BMessage *meta;
};


class FileControl : public BView
{
	public:
		FileControl(BRect rect,const char *label,const char *pathOfFile = NULL);

		void SetText(const char *pathOfFile);
		const char *Text() const;

		virtual	void GetPreferredSize(float *width, float *height);

	private:
		BTextControl	*fText;
		BButton			*fButton;

		BFilePanel		*fPanel;
};

class FileConfigView : public FileControl
{
	public:
		FileConfigView(const char *label,const char *name,bool useMeta = false,const char *defaultPath = NULL);

		void SetTo(BMessage *archive,BMessage *metadata);
		virtual	status_t Archive(BMessage *into,bool deep = true) const;

	private:
		BMessage	*fMeta;
		bool		fUseMeta;
		const char	*fName;
};

#endif	/* CONFIG_VIEW */
