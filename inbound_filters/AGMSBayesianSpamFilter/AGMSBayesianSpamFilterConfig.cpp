/******************************************************************************
 * AGMSBayesianSpamFilter's configuration view.  Lets the user change various
 * settings related to the add-on, but not the server.
 *
 * $Log$
 * Revision 1.4  2002/12/16 16:03:27  agmsmith
 * Changed spam cutoff to 0.95 to work with default Chi-Squared scoring.
 *
 * Revision 1.3  2002/12/13 22:04:43  agmsmith
 * Changed default to turn on the Spam marker in the subject.
 *
 * Revision 1.2  2002/12/12 00:56:28  agmsmith
 * Added some new spam filter options - self training (not implemented yet)
 * and a button to edit the server settings.
 *
 * Revision 1.1  2002/11/03 02:06:15  agmsmith
 * Added initial version.
 *
 * Revision 1.7  2002/10/21 16:13:27  agmsmith
 * Added option to have no words mean spam.
 *
 * Revision 1.6  2002/10/11 20:01:28  agmsmith
 * Added sound effects (system beep) for genuine and spam, plus config option for it.
 *
 * Revision 1.5  2002/10/01 00:45:34  agmsmith
 * Changed default spam ratio to 0.56 from 0.9, for use with
 * the Gary Robinson method in AGMSBayesianSpamServer 1.49.
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
 * Revision 1.1  2002/09/21 20:48:11  agmsmith
 * Initial revision
 */

#include <stdlib.h>
#include <stdio.h>

#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <Message.h>
#include <Messenger.h>
#include <Roster.h>
#include <StringView.h>
#include <TextControl.h>

#include <MailAddon.h>
#include <FileConfigView.h>

using namespace Zoidberg;

static const char *kServerSignature =
	"application/x-vnd.agmsmith.AGMSBayesianSpamServer";

class AGMSBayesianSpamFilterConfig : public BView {
	public:
		AGMSBayesianSpamFilterConfig (BMessage *settings);

		virtual	void MessageReceived (BMessage *msg);
		virtual	void AttachedToWindow ();
		virtual	status_t Archive (BMessage *into, bool deep = true) const;
		virtual	void GetPreferredSize (float *width, float *height);

	private:
		void ShowSpamServerConfigurationWindow ();

		bool fAddSpamToSubject;
		BCheckBox *fAddSpamToSubjectCheckBoxPntr;
		bool fAutoTraining;
		BCheckBox *fAutoTrainingCheckBoxPntr;
		bool fBeepGenuine;
		BCheckBox *fBeepGenuineCheckBoxPntr;
		bool fBeepSpam;
		BCheckBox *fBeepSpamCheckBoxPntr;
		bool fBeepUncertain;
		BCheckBox *fBeepUncertainCheckBoxPntr;
		float fGenuineCutoffRatio;
		BTextControl *fGenuineCutoffRatioTextBoxPntr;
		bool fNoWordsMeansSpam;
		BCheckBox *fNoWordsMeansSpamCheckBoxPntr;
		bool fQuitServerWhenFinished;
		BCheckBox *fQuitServerWhenFinishedCheckBoxPntr;
		BButton *fServerSettingsButtonPntr;
		float fSpamCutoffRatio;
		BTextControl *fSpamCutoffRatioTextBoxPntr;
		static const uint32 kAddSpamToSubjectPressed = 'ASbj';
		static const uint32 kAutoTrainingPressed = 'AuTr';
		static const uint32 kBeepGenuinePressed = 'BpGn';
		static const uint32 kBeepSpamPressed = 'BpSp';
		static const uint32 kBeepUncertainPressed = 'BpUn';
		static const uint32 kNoWordsMeansSpam = 'NoWd';
		static const uint32 kQuitWhenFinishedPressed = 'QuWF';
		static const uint32 kServerSettingsPressed = 'SrvS';
};


AGMSBayesianSpamFilterConfig::AGMSBayesianSpamFilterConfig (BMessage *settings)
	:	BView (BRect (0,0,260,150), "agmsbayesianspamfilter_config",
			B_FOLLOW_LEFT | B_FOLLOW_TOP, 0),
		fAddSpamToSubject (true),
		fAddSpamToSubjectCheckBoxPntr (NULL),
		fAutoTraining (false),
		fAutoTrainingCheckBoxPntr (NULL),
		fBeepGenuine (false),
		fBeepGenuineCheckBoxPntr (NULL),
		fBeepSpam (false),
		fBeepSpamCheckBoxPntr (NULL),
		fBeepUncertain (false),
		fBeepUncertainCheckBoxPntr (NULL),
		fGenuineCutoffRatio (0.05f),
		fGenuineCutoffRatioTextBoxPntr (NULL),
		fNoWordsMeansSpam (true),
		fNoWordsMeansSpamCheckBoxPntr (NULL),
		fQuitServerWhenFinished (true),
		fQuitServerWhenFinishedCheckBoxPntr (NULL),
		fServerSettingsButtonPntr (NULL),
		fSpamCutoffRatio (0.95f),
		fSpamCutoffRatioTextBoxPntr (NULL)
{
	bool	tempBool;
	float	tempFloat;

	if (settings->FindBool ("AddMarkerToSubject", &tempBool) == B_OK)
		fAddSpamToSubject = tempBool;
	if (settings->FindBool ("AutoTraining", &tempBool) == B_OK)
		fAutoTraining = tempBool;
	if (settings->FindBool ("BeepGenuine", &tempBool) == B_OK)
		fBeepGenuine = tempBool;
	if (settings->FindBool ("BeepSpam", &tempBool) == B_OK)
		fBeepSpam = tempBool;
	if (settings->FindBool ("BeepUncertain", &tempBool) == B_OK)
		fBeepUncertain = tempBool;
	if (settings->FindFloat ("GenuineCutoffRatio", &tempFloat) == B_OK)
		fGenuineCutoffRatio = tempFloat;
	if (settings->FindBool ("NoWordsMeansSpam", &tempBool) == B_OK)
		fNoWordsMeansSpam = tempBool;
	if (settings->FindBool ("QuitServerWhenFinished", &tempBool) == B_OK)
		fQuitServerWhenFinished = tempBool;
	if (settings->FindFloat ("SpamCutoffRatio", &tempFloat) == B_OK)
		fSpamCutoffRatio = tempFloat;
}


void AGMSBayesianSpamFilterConfig::AttachedToWindow ()
{
	float		 deltaX;
	BStringView	*labelViewPntr;
	char		 numberString [30];
	BRect		 tempRect;
	char		*tempStringPntr;

	SetViewColor (ui_color (B_PANEL_BACKGROUND_COLOR));

	// Make the checkbox for choosing whether the spam is marked by a
	// modification to the subject of the mail message.

	tempRect = Bounds ();
	fAddSpamToSubjectCheckBoxPntr = new BCheckBox (
		tempRect,
		"AddToSubject",
		"Add \"[Spam %]\" in front of Subject.",
		new BMessage (kAddSpamToSubjectPressed));
	AddChild (fAddSpamToSubjectCheckBoxPntr);
	fAddSpamToSubjectCheckBoxPntr->ResizeToPreferred ();
	fAddSpamToSubjectCheckBoxPntr->SetValue (fAddSpamToSubject);
	fAddSpamToSubjectCheckBoxPntr->SetTarget (this);

	tempRect = Bounds ();
	tempRect.top = fAddSpamToSubjectCheckBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

	// Add the checkbox on the right for the no words means spam option.

	fNoWordsMeansSpamCheckBoxPntr = new BCheckBox (
		tempRect,
		"NoWordsMeansSpam",
		"or no words found.",
		new BMessage (kNoWordsMeansSpam));
	AddChild (fNoWordsMeansSpamCheckBoxPntr);
	fNoWordsMeansSpamCheckBoxPntr->ResizeToPreferred ();
	fNoWordsMeansSpamCheckBoxPntr->MoveBy (
		floorf (tempRect.right - fNoWordsMeansSpamCheckBoxPntr->Frame().right),
		0.0);
	fNoWordsMeansSpamCheckBoxPntr->SetValue (fNoWordsMeansSpam);
	fNoWordsMeansSpamCheckBoxPntr->SetTarget (this);

	// Add the box displaying the spam cutoff ratio to the left, in the space
	// remaining between the left edge and the no words checkbox.

	tempRect.right = fNoWordsMeansSpamCheckBoxPntr->Frame().left -
		be_plain_font->StringWidth ("a");
	tempStringPntr = "Spam above:";
	sprintf (numberString, "%06.4f", (double) fSpamCutoffRatio);
	fSpamCutoffRatioTextBoxPntr	= new BTextControl (
		tempRect,
		"spamcutoffratio",
		tempStringPntr,
		numberString,
		NULL /* BMessage */);
	AddChild (fSpamCutoffRatioTextBoxPntr);
	fSpamCutoffRatioTextBoxPntr->SetDivider (
		be_plain_font->StringWidth (tempStringPntr) +
		1 * be_plain_font->StringWidth ("a"));

	tempRect = Bounds ();
	tempRect.top = fSpamCutoffRatioTextBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

	// Add the box displaying the genuine cutoff ratio, on a line by itself.

	tempStringPntr = "Genuine below and uncertain above:";
	sprintf (numberString, "%06.4f", (double) fGenuineCutoffRatio);
	fGenuineCutoffRatioTextBoxPntr = new BTextControl (
		tempRect,
		"genuinecutoffratio",
		tempStringPntr,
		numberString,
		NULL /* BMessage */);
	AddChild (fGenuineCutoffRatioTextBoxPntr);
	fGenuineCutoffRatioTextBoxPntr->SetDivider (
		be_plain_font->StringWidth (tempStringPntr) +
		1 * be_plain_font->StringWidth ("a"));

	tempRect = Bounds ();
	tempRect.top = fGenuineCutoffRatioTextBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

    // Checkbox for automatically training on incoming mail.

	fAutoTrainingCheckBoxPntr = new BCheckBox (
		tempRect,
		"autoTraining",
		"Self-training on Incoming Messages.",
		new BMessage (kAutoTrainingPressed));
	AddChild (fAutoTrainingCheckBoxPntr);
	fAutoTrainingCheckBoxPntr->ResizeToPreferred ();
	fAutoTrainingCheckBoxPntr->SetValue (fAutoTraining);
	fAutoTrainingCheckBoxPntr->SetTarget (this);

	tempRect = Bounds ();
	tempRect.top = fAutoTrainingCheckBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

	// Button for editing the server settings.

	fServerSettingsButtonPntr = new BButton (
		tempRect,
		"serverSettings",
		"Edit Server Settings",
		new BMessage (kServerSettingsPressed));
	AddChild (fServerSettingsButtonPntr);
	fServerSettingsButtonPntr->ResizeToPreferred ();
	fServerSettingsButtonPntr->SetTarget (this);

	tempRect = Bounds ();
	tempRect.top = fServerSettingsButtonPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

    // Checkbox for closing the server when done.

	fQuitServerWhenFinishedCheckBoxPntr = new BCheckBox (
		tempRect,
		"quitWhenFinished",
		"Close AGMSBayesianSpamServer when Finished.",
		new BMessage (kQuitWhenFinishedPressed));
	AddChild (fQuitServerWhenFinishedCheckBoxPntr);
	fQuitServerWhenFinishedCheckBoxPntr->ResizeToPreferred ();
	fQuitServerWhenFinishedCheckBoxPntr->SetValue (fQuitServerWhenFinished);
	fQuitServerWhenFinishedCheckBoxPntr->SetTarget (this);

	tempRect = Bounds ();
	tempRect.top = fQuitServerWhenFinishedCheckBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;

    // A title and a trio of check boxes for the beep sounds.

	labelViewPntr = new BStringView (tempRect, "beep", "Beep for:");
	AddChild (labelViewPntr);
	labelViewPntr->ResizeToPreferred ();
	tempRect.left = labelViewPntr->Frame().right + 5;
	deltaX = (int) (tempRect.Width() / 3);
	tempRect.right = tempRect.left + deltaX;

	fBeepGenuineCheckBoxPntr = new BCheckBox (
		tempRect,
		"BeepGenuine",
		"Genuine",
		new BMessage (kBeepGenuinePressed));
	AddChild (fBeepGenuineCheckBoxPntr);
	fBeepGenuineCheckBoxPntr->ResizeToPreferred ();
	fBeepGenuineCheckBoxPntr->SetValue (fBeepGenuine);
	fBeepGenuineCheckBoxPntr->SetTarget (this);
    tempRect.left += deltaX;
	tempRect.right = tempRect.left + deltaX;

	fBeepUncertainCheckBoxPntr = new BCheckBox (
		tempRect,
		"BeepUncertain",
		"Uncertain",
		new BMessage (kBeepUncertainPressed));
	AddChild (fBeepUncertainCheckBoxPntr);
	fBeepUncertainCheckBoxPntr->ResizeToPreferred ();
	fBeepUncertainCheckBoxPntr->SetValue (fBeepUncertain);
	fBeepUncertainCheckBoxPntr->SetTarget (this);
    tempRect.left += deltaX;
	tempRect.right = tempRect.left + deltaX;

	fBeepSpamCheckBoxPntr = new BCheckBox (
		tempRect,
		"BeepSpam",
		"Spam",
		new BMessage (kBeepSpamPressed));
	AddChild (fBeepSpamCheckBoxPntr);
	fBeepSpamCheckBoxPntr->ResizeToPreferred ();
	fBeepSpamCheckBoxPntr->SetValue (fBeepSpam);
	fBeepSpamCheckBoxPntr->SetTarget (this);

	tempRect = Bounds ();
	tempRect.top = fBeepSpamCheckBoxPntr->Frame().bottom + 1;
	tempRect.bottom = tempRect.top + 20;
}


status_t
AGMSBayesianSpamFilterConfig::Archive (BMessage *into, bool deep) const
{
	status_t	errorCode;
	float		tempFloat;

	into->MakeEmpty();
	errorCode = into->AddBool ("AddMarkerToSubject", fAddSpamToSubject);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("AutoTraining", fAutoTraining);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("QuitServerWhenFinished", fQuitServerWhenFinished);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("BeepGenuine", fBeepGenuine);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("BeepSpam", fBeepSpam);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("BeepUncertain", fBeepUncertain);

	if (errorCode == B_OK)
		errorCode = into->AddBool ("NoWordsMeansSpam", fNoWordsMeansSpam);

	if (errorCode == B_OK) {
		tempFloat = fGenuineCutoffRatio;
		if (fGenuineCutoffRatioTextBoxPntr != NULL)
			tempFloat = atof (fGenuineCutoffRatioTextBoxPntr->Text());
		errorCode = into->AddFloat ("GenuineCutoffRatio", tempFloat);
	}

	if (errorCode == B_OK) {
		tempFloat = fSpamCutoffRatio;
		if (fSpamCutoffRatioTextBoxPntr != NULL)
			tempFloat = atof (fSpamCutoffRatioTextBoxPntr->Text());
		errorCode = into->AddFloat ("SpamCutoffRatio", tempFloat);
	}

	return errorCode;
}


void
AGMSBayesianSpamFilterConfig::GetPreferredSize (float *width, float *height) {
	*width = 260;
	*height = 150;
}


void
AGMSBayesianSpamFilterConfig::MessageReceived (BMessage *msg)
{
	switch (msg->what)
	{
		case kAddSpamToSubjectPressed:
			fAddSpamToSubject = fAddSpamToSubjectCheckBoxPntr->Value ();
			break;
		case kAutoTrainingPressed:
			fAutoTraining = fAutoTrainingCheckBoxPntr->Value ();
			break;
		case kBeepGenuinePressed:
			fBeepGenuine = fBeepGenuineCheckBoxPntr->Value ();
			break;
		case kBeepSpamPressed:
			fBeepSpam = fBeepSpamCheckBoxPntr->Value ();
			break;
		case kBeepUncertainPressed:
			fBeepUncertain = fBeepUncertainCheckBoxPntr->Value ();
			break;
		case kNoWordsMeansSpam:
			fNoWordsMeansSpam = fNoWordsMeansSpamCheckBoxPntr->Value ();
			break;
		case kQuitWhenFinishedPressed:
			fQuitServerWhenFinished =
				fQuitServerWhenFinishedCheckBoxPntr->Value ();
			break;
		case kServerSettingsPressed:
			ShowSpamServerConfigurationWindow ();
			break;
		default:
			BView::MessageReceived (msg);
	}
}


void
AGMSBayesianSpamFilterConfig::ShowSpamServerConfigurationWindow () {
	status_t    errorCode = B_OK;
	BMessage    maximizeCommand;
	BMessenger	messengerToServer;
	BMessage    replyMessage;
	team_id		serverTeam;

	// Make sure the server is running.
	if (!be_roster->IsRunning (kServerSignature)) {
		errorCode = be_roster->Launch (kServerSignature);
		if (errorCode != B_OK)
			goto ErrorExit;
	}

	// Set up the messenger to the database server.
	serverTeam = be_roster->TeamFor (kServerSignature);
	if (serverTeam < 0)
		goto ErrorExit;
	messengerToServer =
		BMessenger (kServerSignature, serverTeam, &errorCode);
	if (!messengerToServer.IsValid ())
		goto ErrorExit;

	// Wait for the server to finish starting up, and for it to create the window.
	snooze (2000000);

	// Tell it to show its main window, in case it is hidden in server mode.
	maximizeCommand.what = B_SET_PROPERTY;
	maximizeCommand.AddBool ("data", false);
	maximizeCommand.AddSpecifier ("Minimize");
	maximizeCommand.AddSpecifier ("Window", 0L);
	errorCode = messengerToServer.SendMessage (&maximizeCommand, &replyMessage);
	if (errorCode != B_OK)
		goto ErrorExit;
	return; // Successful.

ErrorExit:
	(new BAlert ("SpamFilterConfig Error", "Sorry, unable to launch the "
		"AGMSBayesianSpamServer program to let you edit the server "
		"settings.", "Close"))->Go ();
	return;
}


BView *
instantiate_config_panel (BMessage *settings, BMessage *metadata)
{
	return new AGMSBayesianSpamFilterConfig (settings);
}
