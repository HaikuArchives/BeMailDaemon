#include <NetEndpoint.h>
#include <String.h>

#include <MailAddon.h>

class SMTPProtocol : public MailFilter {
public:
	SMTPProtocol(BMessage *message, StatusView *view);
	virtual status_t InitCheck(BString *verbose);
	virtual MDStatus ProcessMailMessage
	(
		BPositionIO** io_message, BEntry* io_entry,
		BMessage* io_headers, BPath* io_folder, BString* io_uid
	);
	
	//----Perfectly good holdovers from the old days
	status_t Open(const char *server, int port, bool esmtp);
	status_t Login(const char *uid, const char *password);
	void Close();
	status_t Send(const char *to, const char *from, BPositionIO *message);
	
	int32 ReceiveLine(BString &line);
	status_t SendCommand(const char* cmd);
	
private:
	BNetEndpoint conn;
	BString fLog;
	BMessage *_settings;
	StatusView *status_view;
	int32 fAuthType;
	
	status_t err;
};
