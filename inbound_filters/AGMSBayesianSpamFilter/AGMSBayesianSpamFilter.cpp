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
 *
 * Revision 1.8  2002/10/21 16:12:09  agmsmith
 * Added option for spam if no words found, use new method of saving
 * the attribute which avoids hacking the rest of the mail system.
 *
 * Revision 1.7  2002/10/11 20:01:28  agmsmith
 * Added sound effects (system beep) for genuine and spam, plus config option
 * for it.
 *
 * Revision 1.6  2002/10/01 00:45:34  agmsmith
 * Changed default spam ratio to 0.56 from 0.9, for use with
 * the Gary Robinson method in AGMSBayesianSpamServer 1.49.
 *
 * Revision 1.5  2002/09/25 13:23:21  agmsmith
 * Don't leave the data stream at the initial position, try leaving it
 * at the end.  Was having mail progress bar problems.
 *
 * Revision 1.4  2002/09/23 19:14:13  agmsmith
 * Added an option to have the server quit when done.
 *
 * Revision 1.3  2002/09/23 03:33:34  agmsmith
 * First working version, with cutoff ratio and subject modification,
 * and an attribute added if a patch is made to the Folder filter.
 *
 * Revision 1.2  2002/09/21 20:57:22  agmsmith
 * Fixed bugs so now it compiles.
 *
 * Revision 1.1  2002/09/21 20:47:15  agmsmith
 * Initial revision
 */

#include <Beep.h>
#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>

#include <stdlib.h>
#include <stdio.h>

#include "AGMSBayesianSpamFilter.h"

using namespace Zoidberg;

// The names match the ones set up by AGMSBayesianSpamServer for sound effects.
static const char *kAGMSBayesBeepGenuineName = "AGMSBayes-Genuine";
static const char *kAGMSBayesBeepSpamName = "AGMSBayes-Spam";

static const char *kServerSignature =
	"application/x-vnd.agmsmith.AGMSBayesianSpamServer";


AGMSBayesianSpamFilter::AGMSBayesianSpamFilter (BMessage *settings)
	:	Mail::Filter (settings),
		fAddSpamToSubject (false),
		fBeepGenuine (false),
		fBeepSpam (false),
		fNoWordsMeansSpam (false),
		fQuitServerWhenFinished (true),
		fServerSearchDone (false),
		fSpamCutoffRatio (0.56f)
{
	bool		tempBool;
	float		tempFloat;
	BMessenger	tempMessenger;

	if (settings != NULL) {
		if (settings->FindBool ("AddMarkerToSubject", &tempBool) == B_OK)
			fAddSpamToSubject = tempBool;
		if (settings->FindBool ("BeepGenuine", &tempBool) == B_OK)
			fBeepGenuine = tempBool;
		if (settings->FindBool ("BeepSpam", &tempBool) == B_OK)
			fBeepSpam = tempBool;
		if (settings->FindBool ("NoWordsMeansSpam", &tempBool) == B_OK)
			fNoWordsMeansSpam = tempBool;
		if (settings->FindBool ("QuitServerWhenFinished", &tempBool) == B_OK)
			fQuitServerWhenFinished = tempBool;
		if (settings->FindFloat ("SpamCutoffRatio", &tempFloat) == B_OK)
			fSpamCutoffRatio = tempFloat;
	}
}


AGMSBayesianSpamFilter::~AGMSBayesianSpamFilter ()
{
	BMessage quitMessage (B_QUIT_REQUESTED);

	if (fQuitServerWhenFinished && fMessengerToServer.IsValid ())
		fMessengerToServer.SendMessage (&quitMessage);
}


status_t
AGMSBayesianSpamFilter::InitCheck (BString* out_message)
{
	if (out_message != NULL)
		out_message->SetTo (
			"AGMSBayesianSpamFilter::InitCheck is never called!");
	return B_OK;
}


MDStatus
AGMSBayesianSpamFilter::ProcessMailMessage (
	BPositionIO** io_message,
	BEntry* io_entry,
	BMessage* io_headers,
	BPath* io_folder,
	BString* io_uid)
{
	ssize_t		 amountRead;
	off_t		 dataSize;
	BPositionIO	*dataStreamPntr = *io_message;
	status_t	 errorCode = B_OK;
	BString		 newSubjectString;
	BNode        nodeForOutputFile;
	const char	*oldSubjectStringPntr;
	char         percentageString [30];
	BMessage	 replyMessage;
	BMessage	 scriptingMessage;
	team_id		 serverTeam;
	float		 spamRatio;
	char		*stringBuffer = NULL;

	// Make sure the spam database server is running.  Launch if needed.
	// This code used to be in InitCheck, but apparently that isn't called.

	if (!fServerSearchDone) {
		fServerSearchDone = true;
		if (!be_roster->IsRunning (kServerSignature)) {
			errorCode = be_roster->Launch (kServerSignature);
			if (errorCode != B_OK)
				goto ErrorExit;
		}

		// Set up the messenger to the database server.
		serverTeam = be_roster->TeamFor (kServerSignature);
		if (serverTeam < 0)
			goto ErrorExit;
		fMessengerToServer =
			BMessenger (kServerSignature, serverTeam, &errorCode);
	}
	if (!fMessengerToServer.IsValid ())
		goto ErrorExit;

	// Copy the message to a string so that we can pass it to the spam
	// database (the even messier alternative is a temporary file). Do it
	// in a fashion which allows NUL bytes in the string.  This method of
	// course limits the message size to a few hundred megabytes.

	dataSize = dataStreamPntr->Seek (0, SEEK_END);
	if (dataSize <= 0)
		goto ErrorExit;

	stringBuffer = new char [dataSize + 1];
	if (stringBuffer == NULL)
		goto ErrorExit;

	dataStreamPntr->Seek (0, SEEK_SET);
	amountRead = dataStreamPntr->Read (stringBuffer, dataSize);
	if (amountRead != dataSize)
		goto ErrorExit;
	stringBuffer[dataSize] = 0; // Add an end of string NUL, just in case.
	errorCode = scriptingMessage.AddData ("data", B_STRING_TYPE,
		stringBuffer, dataSize + 1, false /* fixed size */);
	if (errorCode != B_OK)
		goto ErrorExit;

	// Send off a scripting command to the database server, asking it to
	// evaluate the string for spaminess.  Note that it can return ENOMSG
	// when there are no words (a good indicator of spam which is pure HTML
	// if you are using plain text only tokenization), so we could use that
	// as a spam marker too.

	scriptingMessage.what = B_SET_PROPERTY;
	scriptingMessage.AddSpecifier ("EvaluateString");
	errorCode = fMessengerToServer.SendMessage (&scriptingMessage,
		&replyMessage);
	if (errorCode != B_OK
		|| replyMessage.FindInt32 ("error", &errorCode) != B_OK)
		goto ErrorExit; // Unable to read the return code.
	if (errorCode == ENOMSG && fNoWordsMeansSpam)
		spamRatio = fSpamCutoffRatio; // Yes, no words and that means spam.
	else if (errorCode != B_OK
		|| replyMessage.FindFloat ("result", &spamRatio) != B_OK)
		goto ErrorExit; // Classification failed in one of many ways.

	// Store the spam ratio in an attribute called MAIL:ratio_spam,
	// attached to the eventual output file.

	if (io_entry != NULL && B_OK == nodeForOutputFile.SetTo (io_entry))
		nodeForOutputFile.WriteAttr ("MAIL:ratio_spam",
			B_FLOAT_TYPE, 0 /* offset */, &spamRatio, sizeof (spamRatio));

	// Also add it to the subject, if requested.

	if (fAddSpamToSubject
		&& spamRatio >= fSpamCutoffRatio
		&& io_headers->FindString ("Subject", &oldSubjectStringPntr) == B_OK) {
		newSubjectString.SetTo ("[Spam ");
		sprintf (percentageString, "%05.2f", spamRatio * 100.0);
		newSubjectString << percentageString << "%] ";
		newSubjectString << oldSubjectStringPntr;
		io_headers->ReplaceString ("Subject", newSubjectString);
	}

	// Beep if requested, different sounds for spam and genuine, as Jeremy
	// Friesner nudged me to get around to implementing.

	if (spamRatio >= fSpamCutoffRatio) {
		if (fBeepSpam)
			system_beep (kAGMSBayesBeepSpamName);
	} else {
		if (fBeepGenuine)
			system_beep (kAGMSBayesBeepGenuineName);
	}

	return MD_OK;

ErrorExit:
	fprintf (stderr, "Error exit from "
		"AGMSBayesianSpamFilter::ProcessMailMessage, code maybe %ld (%s).\n",
		errorCode, strerror (errorCode));
	delete stringBuffer;
	return MD_OK; // Not MD_ERROR so the message doesn't get left on server.
}


status_t
descriptive_name (
	BMessage *settings,
	char *buffer)
{
	bool		addMarker = false;
	float		cutoffRatio = 0.56f;
	bool		tempBool;
	float		tempFloat;

	if (settings != NULL) {
		if (settings->FindBool ("AddMarkerToSubject", &tempBool) == B_OK)
			addMarker = tempBool;
		if (settings->FindFloat ("SpamCutoffRatio", &tempFloat) == B_OK)
			cutoffRatio = tempFloat;
	}

	if (addMarker)
		sprintf (buffer,
			"Mark as Spam if ratio >= %06.4f from AGMSBayesianSpam.",
			(double) tempFloat);
	else
		strcpy (buffer, "Use AGMSBayesianSpam for Spam Ratio attribute.");
	return B_OK;
}


Mail::Filter *
instantiate_mailfilter (
	BMessage *settings,
	Mail::StatusView *statusView)
{
	return new AGMSBayesianSpamFilter (settings);
}
