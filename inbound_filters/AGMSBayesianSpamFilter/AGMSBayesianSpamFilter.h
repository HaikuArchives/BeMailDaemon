#ifndef AGMS_BAYESIAN_SPAM_FILTER_H
#define AGMS_BAYESIAN_SPAM_FILTER_H
/******************************************************************************
 * AGMSBayesianSpamFilter - Uses Bayesian statistics to evaluate the spaminess
 * of a message.  The evaluation is done by a separate server, this add-on just
 * gets the text and uses scripting commands to get an evaluation from the
 * server.  If the server isn't running, it will be found and started up.  Once
 * the evaluation has been received, it is added to the message as an attribute
 * and optionally as an addition to the subject.  Some other add-on later in
 * the pipeline will use the attribute to delete the message or move it to some
 * other folder.
 *
 * Public Domain 2002, by Alexander G. M. Smith, no warranty.
 *
 * $Log$
 * Revision 1.3  2002/11/28 20:20:57  agmsmith
 * Now checks if the spam database is running in headers only mode, and
 * then only downloads headers if that is the case.
 *
 * Revision 1.2  2002/11/10 19:36:27  agmsmith
 * Retry launching server a few times, but not too many.
 *
 * Revision 1.1  2002/11/03 02:06:15  agmsmith
 * Added initial version.
 *
 * Revision 1.5  2002/10/21 16:13:59  agmsmith
 * Added option to have no words mean spam.
 *
 * Revision 1.4  2002/10/11 20:01:28  agmsmith
 * Added sound effects (system beep) for genuine and spam, plus config option
 * for it.
 *
 * Revision 1.3  2002/09/23 19:14:13  agmsmith
 * Added an option to have the server quit when done.
 *
 * Revision 1.2  2002/09/23 03:33:34  agmsmith
 * First working version, with cutoff ratio and subject modification,
 * and an attribute added if a patch is made to the Folder filter.
 *
 * Revision 1.1  2002/09/21 20:47:57  agmsmith
 * Initial revision
 */

#include <Message.h>
#include <List.h>
#include <MailAddon.h>


class AGMSBayesianSpamFilter : public Zoidberg::Mail::Filter {
	public:
		AGMSBayesianSpamFilter (BMessage *settings);
		virtual ~AGMSBayesianSpamFilter ();

		virtual status_t InitCheck (BString* out_message = NULL);

		virtual MDStatus ProcessMailMessage (BPositionIO** io_message,
			BEntry* io_entry,
			BMessage* io_headers,
			BPath* io_folder,
			BString* io_uid);

	private:
		bool fAddSpamToSubject;
		bool fAutoTraining;
		bool fBeepGenuine;
		bool fBeepSpam;
		bool fHeaderOnly;
		int fLaunchAttemptCount;
		BMessenger fMessengerToServer;
		bool fNoWordsMeansSpam;
		bool fQuitServerWhenFinished;
		float fSpamCutoffRatio;
};

#endif	/* AGMS_BAYESIAN_SPAM_FILTER_H */
