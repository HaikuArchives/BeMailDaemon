/* Attachment - classes which handle mail attachments
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <DataIO.h>
#include <String.h>
#include <File.h>
#include <Entry.h>
#include <Mime.h>
#include <ByteOrder.h>
#include <NodeInfo.h>

#include <malloc.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT SimpleAttachment;
	class _EXPORT AttributedAttachment;
	class _EXPORT Attachment;
}
}

#include <MailAttachment.h>
#include <mail_encoding.h>
#include <NodeMessage.h>


using namespace Zoidberg;
using Mail::Attachment;
using Mail::SimpleAttachment;
using Mail::AttributedAttachment;

//--------------SimpleAttachment-No attributes or awareness of the file system at large-----

SimpleAttachment::SimpleAttachment()
	: Attachment(),
	fStatus(B_NO_INIT),
	_data(NULL),
	_raw_data(NULL),
	_we_own_data(false)
{
	Initialize(base64);
}

SimpleAttachment::SimpleAttachment(BPositionIO *data, mail_encoding encoding)
	: Attachment(),
	_data(data),
	_raw_data(NULL),
	_we_own_data(false)
{
	fStatus = data == NULL ? B_BAD_VALUE : B_OK;

	Initialize(encoding);
}

SimpleAttachment::SimpleAttachment(const void *data, size_t length, mail_encoding encoding)
	: Attachment(),
	_data(new BMemoryIO(data,length)),
	_raw_data(NULL),
	_we_own_data(true)
{
	fStatus = data == NULL ? B_BAD_VALUE : B_OK;

	Initialize(encoding);
}

SimpleAttachment::SimpleAttachment(BFile *file, bool delete_when_done)
	: Attachment(),
	_data(NULL),
	_raw_data(NULL),
	_we_own_data(false)
{
	Initialize(base64);
	SetTo(file,delete_when_done);
}

SimpleAttachment::SimpleAttachment(entry_ref *ref)
	: Attachment(),
	_data(NULL),
	_raw_data(NULL),
	_we_own_data(false)
{
	Initialize(base64);
	SetTo(ref);
}

SimpleAttachment::~SimpleAttachment()
{
	if (_we_own_data)
		delete _data;
}

void SimpleAttachment::Initialize(mail_encoding encoding)
{
	SetEncoding(encoding);
	SetHeaderField("Content-Disposition","Attachment");
}

status_t SimpleAttachment::SetTo(BFile *file, bool delete_file_when_done)
{
	char type[B_MIME_TYPE_LENGTH] = "application/octet-stream";

	BNodeInfo nodeInfo(file);
	if (nodeInfo.InitCheck() == B_OK)
		nodeInfo.GetType(type);

	SetHeaderField("Content-Type",type);
	//---No way to get file name (see SetTo(entry_ref *))
	//SetFileName(ref->name);
	
	if (delete_file_when_done)
		SetDecodedDataAndDeleteWhenDone(file);
	else
		SetDecodedData(file);

	return fStatus = B_OK;
}

status_t SimpleAttachment::SetTo(entry_ref *ref)
{
	BFile *file = new BFile(ref,B_READ_ONLY);

	if ((fStatus = file->InitCheck()) < B_OK)
	{
		delete file;
		return fStatus;
	}
	if (SetTo(file,true) < B_OK)
		// fStatus is set by SetTo()
		return fStatus;

	SetFileName(ref->name);
	return fStatus = B_OK;
}

status_t SimpleAttachment::InitCheck()
{
	return fStatus;
}

status_t SimpleAttachment::FileName(char *text) {
	BMessage content_type;
	HeaderField("Content-Type",&content_type);
	
	const char *fileName = content_type.FindString("name");
	if (!fileName)
	{
		content_type.MakeEmpty();
		HeaderField("Content-Disposition",&content_type);
		fileName = content_type.FindString("name");
	}
	if (!fileName)
	{
		content_type.MakeEmpty();
		HeaderField("Content-Location",&content_type);
		fileName = content_type.FindString("unlabeled");
	}
	if (!fileName)
		return B_NAME_NOT_FOUND;

	strncpy(text,fileName,B_FILE_NAME_LENGTH);
	return B_OK;
}

void SimpleAttachment::SetFileName(const char *name) {
	BMessage content_type;
	HeaderField("Content-Type",&content_type);
	if (content_type.ReplaceString("name",name) != B_OK)
		content_type.AddString("name",name);
	
	SetHeaderField("Content-Type",&content_type);
}

status_t SimpleAttachment::GetDecodedData(BPositionIO *data) {
	ParseNow();
	
	if (!_data)
		return B_IO_ERROR;

	char buffer[256];
	ssize_t length;
	_data->Seek(0,SEEK_SET);

	while ((length = _data->Read(buffer,sizeof(buffer))) > 0)
		data->Write(buffer,length);

	return B_OK;
}

BPositionIO *SimpleAttachment::GetDecodedData() {
	ParseNow();
	
	return _data;
}

status_t SimpleAttachment::SetDecodedDataAndDeleteWhenDone(BPositionIO *data) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = true;
	
	return B_OK;
}

status_t SimpleAttachment::SetDecodedData(BPositionIO *data) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = false;
	
	return B_OK;
}
	
status_t SimpleAttachment::SetDecodedData(const void *data, size_t length) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = new BMemoryIO(data,length);
	_we_own_data = true;
	
	return B_OK;
}
		
void SimpleAttachment::SetEncoding(mail_encoding encoding) {
	_encoding = encoding;

	char *cte = NULL; //--Content Transfer Encoding
	switch (_encoding) {
		case base64:
			cte = "base64";
			break;
		case quoted_printable:
			cte = "quoted-printable";
			break;
		case no_encoding:
			cte = "7bit";
			break;
		case uuencode:
			cte = "uuencode";
			break;
	}

	SetHeaderField("Content-Transfer-Encoding",cte);
}

mail_encoding SimpleAttachment::Encoding() {
	return _encoding;
}

status_t SimpleAttachment::SetToRFC822(BPositionIO *data, size_t length, bool parse_now) {
	//---------Massive memory squandering!---ALERT!----------
	if (_we_own_data)
		delete _data;
	
	off_t position = data->Position();
	Mail::Component::SetToRFC822(data,length,parse_now);
	
	// this actually happens...
	if (data->Position() - position > length)
		return B_ERROR;

	length -= (data->Position() - position);
	
	_raw_data = data;
	_raw_length = length;
	_raw_offset = data->Position();
	
	BString encoding = HeaderField("Content-Transfer-Encoding");
	if (encoding.IFindFirst("base64") >= 0)
		_encoding = base64;
	else if (encoding.IFindFirst("quoted-printable") >= 0)
		_encoding = quoted_printable;
	else if (encoding.IFindFirst("uuencode") >= 0)
		_encoding = uuencode;
	else
		_encoding = no_encoding;
		
	if (parse_now)
		ParseNow();
		
	return B_OK;
}

void SimpleAttachment::ParseNow() {
	if (_raw_data == NULL || _raw_length == 0)
		return;
	
	_raw_data->Seek(_raw_offset,SEEK_SET);
	
	char *src = (char *)malloc(_raw_length);
	size_t size = _raw_length;
	
	size = _raw_data->Read(src,_raw_length);
	
	BMallocIO *buffer = new BMallocIO;
	buffer->SetSize(size); //-------8bit is *always* more efficient than an encoding, so the buffer will *never* be larger than before
	
	size = decode(_encoding,(char *)(buffer->Buffer()),src,size,0);
	free(src);
	
	buffer->SetSize(size);
	
	_data = buffer;
	_we_own_data = true;
	
	_raw_data = NULL;
	
	return;
}

status_t SimpleAttachment::RenderToRFC822(BPositionIO *render_to) {
	Mail::Component::RenderToRFC822(render_to);
	
	//---------Massive memory squandering!---ALERT!----------
	//	now with error checks, dumb :-) -- axeld.

	_data->Seek(0,SEEK_END);
	off_t size = _data->Position();
	char *src = (char *)malloc(size);
	if (src == NULL)
		return B_NO_MEMORY;

	_data->Seek(0,SEEK_SET);

	ssize_t read = _data->Read(src,size);
	if (read < B_OK)
		return read;

	//--The encoded text will never be more than twice as large with any conceivable encoding
	char *dest = (char *)malloc(max_encoded_length(_encoding,read));
	if (dest == NULL)
		return B_NO_MEMORY;

	switch (_encoding) {
		case base64:
			size = encode_base64(dest,src,read);
			free(src);
			break;
		case quoted_printable:
			size = decode_qp(dest,src,read,0);
			free(src);
			break;
		case no_encoding:
			src = dest;
			break;
		default:
			return B_BAD_TYPE;
	}

	read = render_to->Write(dest,size);
	free(dest);

	return read > 0 ? B_OK : read;
}


//-------AttributedAttachment--Awareness of bfs, sends attributes--
//	#pragma mark -


AttributedAttachment::AttributedAttachment()
	: Attachment(),
	fContainer(NULL),
	fStatus(B_NO_INIT),
	_data(NULL),
	_attributes_attach(NULL)
{
}

AttributedAttachment::AttributedAttachment(BFile *file, bool delete_when_done)
	: Attachment(),
	fContainer(NULL),
	_data(NULL),
	_attributes_attach(NULL)
{
	SetTo(file,delete_when_done);
}

AttributedAttachment::AttributedAttachment(entry_ref *ref)
	: Attachment(),
	fContainer(NULL),
	_data(NULL),
	_attributes_attach(NULL)
{
	SetTo(ref);
}

AttributedAttachment::~AttributedAttachment() {
	// Our SimpleAttachments are deleted by fContainer
	delete fContainer;
}


status_t AttributedAttachment::Initialize()
{
	// _data & _attributes_attach will be deleted by the container
	if (fContainer != NULL)
		delete fContainer;

	fContainer = new MIMEMultipartContainer("++++++BFile++++++");

	_data = new SimpleAttachment();
	fContainer->AddComponent(_data);

	_attributes_attach = new SimpleAttachment();
	_attributes.MakeEmpty();
	_attributes_attach->SetHeaderField("Content-Type","application/x-be_attribute; name=\"BeOS Attributes\"");
	fContainer->AddComponent(_attributes_attach);
	
	fContainer->SetHeaderField("Content-Type","multipart/x-bfile");
	fContainer->SetHeaderField("Content-Disposition","Attachment");

	// also set the header fields of this component, in case someone asks
	SetHeaderField("Content-Type","multipart/x-bfile");
	SetHeaderField("Content-Disposition","Attachment");

	return B_OK;
}


status_t AttributedAttachment::SetTo(BFile *file, bool delete_file_when_done)
{
	if (file == NULL)
		return fStatus = B_BAD_VALUE;

	if ((fStatus = Initialize()) < B_OK)
		return fStatus;

	_attributes << *file;

	if ((fStatus = _data->SetTo(file,delete_file_when_done)) < B_OK)
		return fStatus;

	// Set boundary

	//---Also, we have the make up the boundary out of whole cloth
	//------This is likely to give a completely random string---
	BString boundary;
	boundary << "BFile--" << (int32(file) ^ time(NULL)) << "-" << ~((int32)file ^ (int32)&fStatus ^ (int32)&_attributes) << "--";
	fContainer->SetBoundary(boundary.String());
	
	return fStatus = B_OK;
}

status_t AttributedAttachment::SetTo(entry_ref *ref)
{
	if (ref == NULL)
		return fStatus = B_BAD_VALUE;

	if ((fStatus = Initialize()) < B_OK)
		return fStatus;

	BNode node(ref);
	if ((fStatus = node.InitCheck()) < B_OK)
		return fStatus;

	_attributes << node;

	if ((fStatus = _data->SetTo(ref)) < B_OK)
		return fStatus;

	// Set boundary

	//------This is likely to give a completely random string---
	BString boundary;
	char buffer[512];
	strcpy(buffer, ref->name);
	for (int32 i = strlen(buffer);i-- > 0;)
	{
		if (buffer[i] & 0x80)
			buffer[i] = 'x';
		else if (buffer[i] == ' ' || buffer[i] == ':')
			buffer[i] = '_';
	}
	buffer[32] = '\0';
	boundary << "BFile-" << buffer << "--" << ((int32)_data ^ time(NULL)) << "-" << ~((int32)_data ^ (int32)&buffer ^ (int32)&_attributes) << "--";
	fContainer->SetBoundary(boundary.String());

	return fStatus = B_OK;
}

status_t AttributedAttachment::InitCheck()
{
	return fStatus;
}

void AttributedAttachment::SaveToDisk(BEntry *entry) {
	BString path = "/tmp/";
	char name[255] = "";
	_data->FileName(name);
	path << name;
	
	BFile file(path.String(),B_READ_WRITE | B_CREATE_FILE);
	(BNode&)file << _attributes;
	_data->GetDecodedData(&file);
	file.Sync();
	
	entry->SetTo(path.String());
}

void AttributedAttachment::SetEncoding(mail_encoding encoding) {
	_data->SetEncoding(encoding);
	_attributes_attach->SetEncoding(encoding);
}

mail_encoding AttributedAttachment::Encoding() {
	return _data->Encoding();
}

status_t AttributedAttachment::FileName(char *name) {
	return _data->FileName(name);
}
	
void AttributedAttachment::SetFileName(const char *name) {
	_data->SetFileName(name);
}

status_t AttributedAttachment::GetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		*node << _attributes;
		
	_data->GetDecodedData(data);
	return B_OK;
}

status_t AttributedAttachment::SetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		_attributes << *node;
		
	_data->SetDecodedData(data);
	return B_OK;
}
		
status_t AttributedAttachment::SetToRFC822(BPositionIO *data, size_t length, bool parse_now)
{
	status_t err = Initialize();
	if (err < B_OK)
		return err;

	err = fContainer->SetToRFC822(data,length,parse_now);
	if (err < B_OK)
		return err;

	BMimeType type;
	fContainer->MIMEType(&type);
	if (strcmp(type.Type(),"multipart/x-bfile") != 0)
		return B_BAD_TYPE;

	// get data and attributes	
	if ((_data = dynamic_cast<SimpleAttachment *>(fContainer->GetComponent(0))) == NULL)
		return B_BAD_VALUE;

	if ((_attributes_attach = dynamic_cast<SimpleAttachment *>(fContainer->GetComponent(1))) == NULL
		|| _attributes_attach->GetDecodedData() == NULL)
		return B_BAD_VALUE;

	int32 len = ((BMallocIO *)(_attributes_attach->GetDecodedData()))->BufferLength();
	char *start = (char *)malloc(len);
	if (start == NULL)
		return B_NO_MEMORY;

	if (_attributes_attach->GetDecodedData()->ReadAt(0,start,len) < len)
		return B_IO_ERROR;

	int32 index = 0;
	while (index < len) {
		char *name = &start[index];
		index += strlen(name) + 1;

		type_code code;
		memcpy(&code, &start[index], sizeof(type_code));
		code = B_BENDIAN_TO_HOST_INT32(code);
		index += sizeof(type_code);

		int64 buf_length;
		memcpy(&buf_length, &start[index], sizeof(buf_length));
		buf_length = B_BENDIAN_TO_HOST_INT64(buf_length);
		index += sizeof(buf_length);

		swap_data(code, &start[index], buf_length, B_SWAP_BENDIAN_TO_HOST);
		_attributes.AddData(name, code, &start[index], buf_length);
		index += buf_length;
	}
	free(start);

	return B_OK;
}
	
status_t AttributedAttachment::RenderToRFC822(BPositionIO *render_to) {
	BMallocIO *io = new BMallocIO;
#if B_BEOS_VERSION_DANO
	const
#endif
	char *name;
	type_code type, swap_typed;
	for (int32 i = 0; _attributes.GetInfo(B_ANY_TYPE,i,&name,&type) == B_OK; i++) {
		const void *data;
		ssize_t dataLen;
		_attributes.FindData(name,type,&data,&dataLen);
		io->Write(name,strlen(name) + 1);
		swap_typed = B_HOST_TO_BENDIAN_INT32(type);
		io->Write(&swap_typed,sizeof(type_code));

		int64 length, swapped;
		length = dataLen;
		swapped = B_HOST_TO_BENDIAN_INT64(length);
		io->Write(&swapped,sizeof(int64));

		void *allocd = malloc(dataLen);
		if (allocd == NULL)
			return B_NO_MEMORY;
		memcpy(allocd,data,dataLen);
		swap_data(type, allocd, dataLen, B_SWAP_HOST_TO_BENDIAN);
		io->Write(allocd,dataLen);
		free(allocd);
	}
	
	_attributes_attach->SetDecodedDataAndDeleteWhenDone(io);
	
	return fContainer->RenderToRFC822(render_to);
}

status_t AttributedAttachment::MIMEType(BMimeType *mime) {
	return _data->MIMEType(mime);
}


//	The reserved function stubs
//	#pragma mark -

void Attachment::_ReservedAttachment1() {}
void Attachment::_ReservedAttachment2() {}
void Attachment::_ReservedAttachment3() {}
void Attachment::_ReservedAttachment4() {}

void SimpleAttachment::_ReservedSimple1() {}
void SimpleAttachment::_ReservedSimple2() {}
void SimpleAttachment::_ReservedSimple3() {}

void AttributedAttachment::_ReservedAttributed1() {}
void AttributedAttachment::_ReservedAttributed2() {}
void AttributedAttachment::_ReservedAttributed3() {}
