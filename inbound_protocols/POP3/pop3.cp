/* POP3Protocol - implementation of the POP3 protocol
**
** Copyright 2001-2002 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <stdlib.h>
#include <stdio.h>
#include <DataIO.h>
#include <Alert.h>
#include <Debug.h>
#include <socket.h>
#include <netdb.h>
#include <errno.h>

#include <status.h>
#include <StringList.h>
#include <ProtocolConfigView.h>
#include <ChainRunner.h>

#include <MDRLanguage.h>

#include "pop3.h"
#include "md5.h"

using namespace Zoidberg;

#define POP3_RETRIEVAL_TIMEOUT 60000000
#define CRLF	"\r\n"

#define pop3_error(string) runner->ShowError(string)

POP3Protocol::POP3Protocol(BMessage *settings, Mail::ChainRunner *status)
	: Mail::SimpleProtocol(settings,status),
	fNumMessages(-1),
	fMailDropSize(0)
{
	Init();
}


POP3Protocol::~POP3Protocol()
{
	SendCommand("QUIT" CRLF);

	closesocket(conn);
}


status_t POP3Protocol::Open(const char *server, int port, int)
{
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Connecting to POP3 Server...","POP3サーバに接続しています..."));

	if (port <= 0)
		port = 110;

	fLog = "";

	//-----Prime the error message
	BString error_msg;
	error_msg << MDR_DIALECT_CHOICE ("Error while connecting to server ","サーバに接続中にエラーが発生しました ") << server;
	if (port != 110)
		error_msg << ":" << port;
	
	uint32 hostIP = inet_addr(server);  // first see if we can parse it as a numeric address
	if ((hostIP == 0)||(hostIP == (uint32)-1)) {
		struct hostent * he = gethostbyname(server);
		hostIP = he ? *((uint32*)he->h_addr) : 0;
	}

	if (hostIP == 0) {
		error_msg << MDR_DIALECT_CHOICE (": Connection refused or host not found",": ：接続が拒否されたかサーバーが見つかりません");
		pop3_error(error_msg.String());

		return B_NAME_NOT_FOUND;
	}
	
	conn = socket(AF_INET, SOCK_STREAM, 0);
	if (conn >= 0) {
		struct sockaddr_in saAddr;
		memset(&saAddr, 0, sizeof(saAddr));
		saAddr.sin_family      = AF_INET;
		saAddr.sin_port        = htons(port);
		saAddr.sin_addr.s_addr = hostIP;
		int result = connect(conn, (struct sockaddr *) &saAddr, sizeof(saAddr));
		if (result < 0) {
			closesocket(conn);
			conn = -1;
			error_msg << ": " << strerror(errno);
			pop3_error(error_msg.String());
			return errno;
		}
	} else {
		error_msg << ": Could not allocate socket.";
		pop3_error(error_msg.String());
		return B_ERROR;
	}
	
	BString line;
	if( ReceiveLine(line) <= 0) {
		error_msg << ": " << strerror(errno);
		pop3_error(error_msg.String());

		return B_ERROR;
	}

	if(strncmp(line.String(),"+OK",3) != 0) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << line.String();
		pop3_error(error_msg.String());

		return B_ERROR;
	}

	fLog = line;

	return B_OK;
}


status_t POP3Protocol::Login(const char *uid, const char *password, int method)
{
	status_t err;

	BString error_msg;
	error_msg << MDR_DIALECT_CHOICE ("Error while authenticating user ","ユーザー認証中にエラーが発生しました ") << uid;

	if (method == 1) {	//APOP
		int32 index = fLog.FindFirst("<");
		if(index != B_ERROR) {
			runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending APOP authentication...","APOP認証情報を送信中..."));
			int32 end = fLog.FindFirst(">",index);
			BString timestamp("");
			fLog.CopyInto(timestamp,index,end-index+1);
			timestamp += password;
			char md5sum[33];
			MD5Digest((unsigned char*)timestamp.String(),md5sum);
			BString cmd = "APOP ";
			cmd += uid;
			cmd += " ";
			cmd += md5sum;
			cmd += CRLF;

			err = SendCommand(cmd.String());
			if (err != B_OK) {
				error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
				pop3_error(error_msg.String());

				return err;
			}

			return B_OK;
		} else {
			error_msg << MDR_DIALECT_CHOICE (": The server does not support APOP.","サーバはAPOPをサポートしていません");
			pop3_error(error_msg.String());
			return B_NOT_ALLOWED;
		}
	}
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending username...","ユーザーID送信中..."));

	BString cmd = "USER ";
	cmd += uid;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
		pop3_error(error_msg.String());

		return err;
	}

	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Sending password...","パスワード送信中..."));
	cmd = "PASS ";
	cmd += password;
	cmd += CRLF;

	err = SendCommand(cmd.String());
	if (err != B_OK) {
		error_msg << MDR_DIALECT_CHOICE (". The server said:\n","サーバのメッセージです\n") << fLog;
		pop3_error(error_msg.String());

		return err;
	}

	return B_OK;
}


status_t POP3Protocol::Stat()
{
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Getting mailbox size...","メールボックスのサイズを取得しています..."));

	if (SendCommand("STAT" CRLF) < B_OK)
		return B_ERROR;

	int32 messages,dropSize;
	if (sscanf(fLog.String(),"+OK %ld %ld",&messages,&dropSize) < 2)
		return B_ERROR;

	fNumMessages = messages;
	fMailDropSize = dropSize;

	return B_OK;
}


int32 POP3Protocol::Messages()
{
	if (fNumMessages < 0)
		Stat();

	return fNumMessages;
}


size_t POP3Protocol::MailDropSize()
{
	if (fNumMessages < 0)
		Stat();

	return fMailDropSize;
}


status_t POP3Protocol::Retrieve(int32 message, BPositionIO *write_to)
{
	status_t returnCode;
	BString cmd;
	cmd << "RETR " << message + 1 << CRLF;
	returnCode = RetrieveInternal(cmd.String(), message, write_to, true);
	runner->ReportProgress(0 /* bytes */, 1 /* messages */);

	if (returnCode == B_OK) { // Some debug code.
		int32 message_len = MessageSize(message);
 		write_to->Seek (0, SEEK_END);
		if (write_to->Position() != message_len) {
			printf ("POP3Protocol::Retrieve Note: message size is %d, was "
			"expecting %ld, for message #%ld.  Could be a transmission error "
			"or a bad POP server implementation (does it remove escape codes "
			"when it counts size?).\n",
			(int) write_to->Position(), message_len, message);
		}
	}

	return returnCode;
}


status_t POP3Protocol::GetHeader(int32 message, BPositionIO *write_to)
{
	BString cmd;
	cmd << "TOP " << message + 1 << " 0" << CRLF;
	return RetrieveInternal(cmd.String(),message,write_to, false);
}


status_t POP3Protocol::RetrieveInternal(const char *command, int32 message,
	BPositionIO *write_to, bool post_progress)
{
	const int bufSize = 1024 * 30;

	// To avoid waiting for the non-arrival of the next data packet, try to
	// receive only the message size, plus the 3 extra bytes for the ".\r\n"
	// after the message.  Of course, if we get it wrong (or it is a huge
	// message or has lines starting with escaped periods), it will then switch
	// back to receiving full buffers until the message is done.
	int amountToReceive = MessageSize (message) + 3;
	if (amountToReceive >= bufSize || amountToReceive <= 0)
		amountToReceive = bufSize - 1;

	BString bufBString; // Used for auto-dealloc on return feature.
	char *buf = bufBString.LockBuffer (bufSize);
	int amountInBuffer = 0;
	int amountReceived;
	int testIndex;
	char *testStr;
	bool cont = true;
	bool flushWholeBuffer = false;
	write_to->Seek(0,SEEK_SET);

	if (SendCommand(command) != B_OK)
		return B_ERROR;
	
	struct timeval tv;
	struct fd_set fds; 

	tv.tv_sec = long(POP3_RETRIEVAL_TIMEOUT / 1e6); 
	tv.tv_usec = long(POP3_RETRIEVAL_TIMEOUT-(tv.tv_sec * 1e6)); 
	
	/* Initialize (clear) the socket mask. */ 
	FD_ZERO(&fds);
	
	/* Set the socket in the mask. */ 
	FD_SET(conn, &fds);
	
	while (cont) {
		if (select(32, &fds, NULL, NULL, &tv) == 0) {
			// No data available, even after waiting a minute.
			fLog = "POP3 timeout - no data received after a long wait.";
			return B_ERROR;
		}
		if (amountToReceive > bufSize - 1 - amountInBuffer)
			amountToReceive = bufSize - 1 - amountInBuffer;

		amountReceived = recv(conn,buf + amountInBuffer, amountToReceive,0);

		if (amountReceived < 0) {
			fLog = strerror(errno);
			return errno;
		}
		if (amountReceived == 0) {
			fLog = "POP3 data supposedly ready to receive but not received!";
			return B_ERROR; // Shouldn't happen, but...
		}
		amountToReceive = bufSize - 1; // For next time, read a full buffer.
		amountInBuffer += amountReceived;
		buf[amountInBuffer] = 0; // NUL stops tests past the end of buffer.

		// Look for lines starting with a period.  A single period by itself on
		// a line "\r\n.\r\n" marks the end of the message (thus the need for
		// at least five characters in the buffer for testing).  A period
		// "\r\n.Stuff" at the start of a line get deleted "\r\nStuff", since
		// POP adds one as an escape code to let you have message text with
		// lines starting with a period.  For convenience, assume that no
		// messages start with a period on the very first line, so we can
		// search for the previous line's "\r\n".

		for (testIndex = 0; testIndex <= amountInBuffer - 5; testIndex++) {
			testStr = buf + testIndex;
			if (testStr[0] == '\r' && testStr[1] == '\n' && testStr[2] == '.') {
				if (testStr[3] == '\r' && testStr[4] == '\n') {
					// Found the end of the message marker.  Ignore remaining data.
					if (amountInBuffer > testIndex + 5)
						printf ("POP3Protocol::RetrieveInternal Ignoring %d bytes "
							"of extra data past message end.\n",
							amountInBuffer - (testIndex + 5));
					amountInBuffer = testIndex + 2; // Don't include ".\r\n".
					buf[amountInBuffer] = 0;
					cont = false;
				} else {
					// Remove an extra period at the start of a line.
					// Inefficient, but it doesn't happen often that you have a
					// dot starting a line of text.  Of course, a file with a
					// lot of double period lines will get processed very
					// slowly.
					memmove (buf + testIndex + 2, buf + testIndex + 3,
						amountInBuffer - (testIndex + 3) + 1 /* for NUL at end */);
					amountInBuffer--;
					// Watch out for the end of buffer case, when the POP text
					// is "\r\n..X".  Don't want to leave the resulting
					// "\r\n.X" in the buffer (flush out the whole buffer),
					// since that will get mistakenly evaluated again in the
					// next loop and delete a character by mistake.
					if (testIndex >= amountInBuffer - 4 && testStr[2] == '.') {
						printf ("POP3Protocol::RetrieveInternal: Jackpot!  You have "
							"hit the rare situation with an escaped period at the "
							"end of the buffer.  Aren't you happy it decodes it "
							"correctly?\n");
						flushWholeBuffer = true;
					}
				}
			}
		}

		if (cont && !flushWholeBuffer) {
			// Dump out most of the buffer, but leave the last 4 characters for
			// comparison continuity, in case the line starting with a period
			// crosses a buffer boundary.
			if (amountInBuffer > 4) {
				write_to->Write(buf, amountInBuffer - 4);
				if (post_progress)
					runner->ReportProgress(amountInBuffer - 4,0);
				memmove (buf, buf + amountInBuffer - 4, 4);
				amountInBuffer = 4;
			}
		} else { // Dump everything - end of message or flushing the whole buffer.
			write_to->Write(buf, amountInBuffer);
			if (post_progress)
				runner->ReportProgress(amountInBuffer,0);
			amountInBuffer = 0;
		}
	}
	return B_OK;
}


status_t POP3Protocol::UniqueIDs() {
	status_t ret = B_OK;
	runner->ReportProgress(0,0,MDR_DIALECT_CHOICE ("Getting UniqueIDs...","固有のIDを取得中..."));

	ret = SendCommand("UIDL" CRLF);
	if (ret != B_OK) return ret;

	BString result;
	int32 uid_offset;
	while (ReceiveLine(result) > 0) {
		if (result.ByteAt(0) == '.')
			break;

		uid_offset = result.FindFirst(' ') + 1;
		result.Remove(0,uid_offset);
		unique_ids->AddItem(result.String());
	}

	if (SendCommand("LIST"CRLF) != B_OK)
		return B_ERROR;

	int32 b;
	while (ReceiveLine(result) > 0) {
		if (result.ByteAt(0) == '.')
			break;

		b = result.FindLast(" ");
		if (b >= 0)
			b = atol(&(result.String()[b]));
		else
			b = 0;
		sizes.AddItem((void *)(b));
	}

	return ret;
}


void POP3Protocol::Delete(int32 num) {
	BString cmd = "DELE ";
	cmd << (num+1) << CRLF;
	if (SendCommand(cmd.String()) != B_OK) {
		// Error
	}
	#if DEBUG
	 puts(fLog.String());
	#endif
}


size_t POP3Protocol::MessageSize(int32 index) {
	return (size_t)(sizes.ItemAt(index));
}


int32 POP3Protocol::ReceiveLine(BString &line) {
	int32 len = 0,rcv;
	int8 c = 0;
	bool flag = false;

	line = "";
	
	struct timeval tv;
	struct fd_set fds; 

	tv.tv_sec = long(POP3_RETRIEVAL_TIMEOUT / 1e6); 
	tv.tv_usec = long(POP3_RETRIEVAL_TIMEOUT-(tv.tv_sec * 1e6)); 
	
	/* Initialize (clear) the socket mask. */ 
	FD_ZERO(&fds);
	
	/* Set the socket in the mask. */ 
	FD_SET(conn, &fds); 
	int result = select(32, &fds, NULL, NULL, &tv);
	
	if (result < 0)
		return result;
	
	if (result > 0) {
		while (true) { // Hope there's an end of line out there else this gets stuck.
			rcv = recv(conn,&c,1,0);
			if (rcv < 0)
				return errno; //--An error!
				
			if((c == '\n') || (rcv == 0 /* EOF */))
				break;

			if (c == '\r') {
				flag = true;
			} else {
				if (flag) {
					len++;
					line += '\r';
					flag = false;
				}
				len++;
				line += (char)c;
			}
		}
	} else {
		fLog = "POP3 socket timeout.";
	}
	return len;
}


status_t POP3Protocol::SendCommand(const char* cmd) {
	int32 len;

	// Flush any accumulated garbage data before we send our command, so we
	// don't misinterrpret responses from previous commands (that got left over
	// due to bugs) as being from this command.
	
	struct timeval tv;
	struct fd_set fds; 

	tv.tv_sec = long(1000 / 1e6); 
	tv.tv_usec = long(1000-(tv.tv_sec * 1e6));  /* very short timeout, hangs with 0 in R5 */
	
	/* Initialize (clear) the socket mask. */ 
	FD_ZERO(&fds);
	
	/* Set the socket in the mask. */ 
	FD_SET(conn, &fds);
	
	while (select(32, &fds, NULL, NULL, &tv) > 0) {
		int amountReceived;
		char tempString [1025];
		amountReceived = recv (conn,tempString, sizeof (tempString) - 1,0);
		if (amountReceived < 0)
			return errno;
		tempString [amountReceived] = 0;
		printf ("POP3Protocol::SendCommand Bug!  Had to flush %d bytes: %s\n",
			amountReceived, tempString);
		if (amountReceived == 0)
			break;
	}

	if (send(conn,cmd, ::strlen(cmd),0) < 0) {
		fLog = strerror(errno);
		printf ("POP3Protocol::SendCommand Send \"%s\" failed, code %d: %s\n",
			cmd, errno, fLog.String());
		return errno;
	}

	fLog="";
	status_t err = B_OK;

	while(true) {
		len = ReceiveLine(fLog);
		if(len <= 0 || fLog.ICompare("+OK",3) == 0)
			break;

		else if(fLog.ICompare("-ERR",4) == 0) {
			printf("POP3Protocol::SendCommand \"%s\" got error message "
				"from server: %s\n", cmd, fLog.String());
			err = B_ERROR;
			break;
		} else {
			printf("POP3Protocol::SendCommand \"%s\" got nonsense message "
				"from server: %s\n", cmd, fLog.String());
			err = B_BAD_VALUE; //-------If it's not +OK, and it's not -ERR, then what the heck is it? Presume an error
			break;
		}
	}
	return err;
}


void POP3Protocol::MD5Digest (unsigned char *in,char *ascii_digest)
{
	int i;
	MD5_CTX context;
	unsigned char digest[16];

	MD5Init(&context);
	MD5Update(&context, in, ::strlen((char*)in));
	MD5Final(digest, &context);

  	for (i = 0;  i < 16;  i++)
    	sprintf(ascii_digest+2*i, "%02x", digest[i]);

	return;
}


Mail::Filter *instantiate_mailfilter(BMessage *settings, Mail::ChainRunner *runner)
{
	return new POP3Protocol(settings,runner);
}


BView* instantiate_config_panel(BMessage *settings,BMessage *)
{
	Mail::ProtocolConfigView *view = new Mail::ProtocolConfigView(Mail::MP_HAS_USERNAME | Mail::MP_HAS_AUTH_METHODS | Mail::MP_HAS_PASSWORD | Mail::MP_HAS_HOSTNAME | Mail::MP_CAN_LEAVE_MAIL_ON_SERVER);
	view->AddAuthMethod("Plain Text");
	view->AddAuthMethod("APOP");

	view->SetTo(settings);

	return view;
}
