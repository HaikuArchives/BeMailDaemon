#include <NetEndpoint.h>
#include <String.h>

#include <MailProtocol.h>

class POP3Protocol : public Mail::SimpleProtocol {
  public:
  	POP3Protocol(BMessage *settings, Mail::StatusView *status);
  	~POP3Protocol();
  
	void SetStatusReporter(Mail::StatusView *view);
	status_t Open(const char *server, int port, int protocol);
	status_t Login(const char *uid, const char *password, int method);
	int32 Messages(void);
	status_t UniqueIDs();
	status_t Retrieve(int32 message, BPositionIO *write_to);
	status_t GetHeader(int32 message, BPositionIO *write_to);
	void Delete(int32 num);
	size_t MessageSize(int32 index);
	size_t MailDropSize(void);

protected:
	status_t RetrieveInternal(const char *command,int32 message, BPositionIO *write_to, bool show_progress);
	
	int32 ReceiveLine(BString &line);
	status_t SendCommand(const char* cmd);
	void MD5Digest (unsigned char *in, char *out); // MD5 Digest
	
private:
	BNetEndpoint conn;
	BString		fLog;
	Mail::StatusView *status_view;			
};
