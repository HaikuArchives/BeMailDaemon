#include <DataIO.h>
#include <String.h>
#include <File.h>
#include <Entry.h>
#include <Mime.h>
#include <ByteOrder.h>
#include <NodeInfo.h>

#include <malloc.h>

class _EXPORT SimpleMailAttachment;
class _EXPORT AttributedMailAttachment;

#include <MailAttachment.h>
#include <mail_encoding.h>
#include <NodeMessage.h>

#include <stdio.h>
//--------------SimpleMailAttachment-No attributes or awareness of the file system at large-----

SimpleMailAttachment::SimpleMailAttachment()
	: MailComponent(),
	_data(NULL),
	_we_own_data(false),
	_raw_data(NULL)
	{
		SetEncoding(base64);
		SetHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(BPositionIO *data, mail_encoding encoding)
	: MailComponent(),
	_data(data),
	_we_own_data(false),
	_raw_data(NULL)
	{
		SetEncoding(encoding);
		SetHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::SimpleMailAttachment(const void *data, size_t length, mail_encoding encoding)
	: MailComponent(),
	_data(new BMemoryIO(data,length)),
	_we_own_data(true),
	_raw_data(NULL)
	{
		SetEncoding(encoding);
		SetHeaderField("Content-Disposition","Attachment");
	}

SimpleMailAttachment::~SimpleMailAttachment() {
	if (_we_own_data)
		delete _data;
}

status_t SimpleMailAttachment::FileName(char *text) {
	BMessage content_type;
	HeaderField("Content-Type",&content_type);
	
	if (!content_type.HasString("name"))
		return B_NAME_NOT_FOUND;
	
	strncpy(text,content_type.FindString("name"),B_FILE_NAME_LENGTH);
	return B_OK;
}

void SimpleMailAttachment::SetFileName(const char *name) {
	BMessage content_type;
	HeaderField("Content-Type",&content_type);
	if (content_type.ReplaceString("name",name) != B_OK)
		content_type.AddString("name",name);
	
	SetHeaderField("Content-Type",&content_type);
}

status_t SimpleMailAttachment::GetDecodedData(BPositionIO *data) {
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

BPositionIO *SimpleMailAttachment::GetDecodedData() {
	ParseNow();
	
	return _data;
}

status_t SimpleMailAttachment::SetDecodedDataAndDeleteWhenDone(BPositionIO *data) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = true;
	
	return B_OK;
}

status_t SimpleMailAttachment::SetDecodedData(BPositionIO *data) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = data;
	_we_own_data = false;
	
	return B_OK;
}
	
status_t SimpleMailAttachment::SetDecodedData(const void *data, size_t length) {
	_raw_data = NULL;
	
	if (_we_own_data)
		delete _data;
	
	_data = new BMemoryIO(data,length);
	_we_own_data = true;
	
	return B_OK;
}
		
void SimpleMailAttachment::SetEncoding(mail_encoding encoding) {
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

mail_encoding SimpleMailAttachment::Encoding() {
	return _encoding;
}

status_t SimpleMailAttachment::SetToRFC822(BPositionIO *data, size_t length, bool parse_now) {
	//---------Massive memory squandering!---ALERT!----------
	if (_we_own_data)
		delete _data;
	
	off_t position = data->Position();
	MailComponent::SetToRFC822(data,length,parse_now);
	
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

void SimpleMailAttachment::ParseNow() {
	if (_raw_data == NULL)
		return;
	
	_raw_data->Seek(_raw_offset,SEEK_SET);
	
	char *src = (char *)malloc(_raw_length);
	size_t size = _raw_length;
	
	size = _raw_data->Read(src,_raw_length);
	
	BMallocIO *buffer = new BMallocIO;
	buffer->SetSize(size); //-------8bit is *always* more efficient than an encoding, so the buffer will *never* be larger than before
	
	size = decode(_encoding,(char *)(buffer->Buffer()),src,size);
	free(src);
	
	buffer->SetSize(size);
	
	_data = buffer;
	_we_own_data = true;
	
	_raw_data = NULL;
	
	return;
}

status_t SimpleMailAttachment::RenderToRFC822(BPositionIO *render_to) {
	MailComponent::RenderToRFC822(render_to);
	
	//---------Massive memory squandering!---ALERT!----------
	//if (_encoding != base64)
	//	return B_BAD_TYPE;

	_data->Seek(0,SEEK_END);
	size_t size = _data->Position();
	char *src = (char *)malloc(size);
	_data->Seek(0,SEEK_SET);
	
	size = _data->Read(src,size);
	
	char *dest = (char *)malloc(max_encoded_length(_encoding,size)); //--The encoded text will never be more than twice as large with any conceivable encoding
	
	switch (_encoding) {
		case base64:
			size = encode_base64(dest,src,size);
			free(src);
			break;
		case quoted_printable:
			size = decode_qp(dest,src,size);
			free(src);
			break;
		case no_encoding:
			src = dest;
			break;
		default:
			return B_BAD_TYPE;
	}
	
	render_to->Write(dest,size);
	free(dest);
	
	return B_OK;
}

//-------AttributedMailAttachment--Awareness of bfs, sends attributes--
AttributedMailAttachment::AttributedMailAttachment() : MIMEMultipartContainer("++++++BFile++++++"),
		_data(NULL),
		_attributes_attach(NULL) {}

AttributedMailAttachment::AttributedMailAttachment(BFile *file, bool delete_when_done) : MIMEMultipartContainer("++++++BFile++++++") {
	_data = new SimpleMailAttachment;
	AddComponent(_data);
	
	_attributes_attach = new SimpleMailAttachment;
	_attributes_attach->SetHeaderField("Content-Type","application/x-be_attribute; name=\"BeOS Attributes\"");
	AddComponent(_attributes_attach);
	
	SetHeaderField("Content-Type","multipart/x-bfile");
	SetHeaderField("Content-Disposition","Attachment");
	
	if (file != NULL)
		SetTo(file,delete_when_done);
}

AttributedMailAttachment::AttributedMailAttachment(entry_ref *ref) {
	_data = new SimpleMailAttachment;
	AddComponent(_data);
	
	_attributes_attach = new SimpleMailAttachment;
	_attributes_attach->SetHeaderField("Content-Type","application/x-be_attribute; name=\"BeOS Attributes\"");
	AddComponent(_attributes_attach);
	
	SetHeaderField("Content-Type","multipart/x-bfile");
	SetHeaderField("Content-Disposition","Attachment");
	
	if (ref != NULL)
		SetTo(ref);
}

AttributedMailAttachment::~AttributedMailAttachment() {
	//------------Our SimpleMailAttachments are deleted by MailContainer
}

void AttributedMailAttachment::SetTo(BFile *file, bool delete_file_when_done) {
	char buffer[512];
	_attributes << *file;
	
	BNodeInfo(file).GetType(buffer);
	_data->SetHeaderField("Content-Type",buffer);
	//---No way to get file name
	//_data->SetFileName(ref->name);
	
	if (delete_file_when_done)
		_data->SetDecodedDataAndDeleteWhenDone(file);
	else
		_data->SetDecodedData(file);
	
	//---Also, we have the make up the boundary out of whole cloth
	//------This is likely to give a completely random string---
	BString boundary;
	boundary << "BFile--" << (int32(file) ^ time(NULL)) << ":" << ~((int32)file ^ (int32)&buffer ^ (int32)&_attributes) << "--";
	SetBoundary(boundary.String()); 
}
	
void AttributedMailAttachment::SetTo(entry_ref *ref) {
	char buffer[512];
	BNode node(ref);
	_attributes << node;
	BFile *file = new BFile(ref,B_READ_ONLY); 
	
	_data->SetDecodedDataAndDeleteWhenDone(file);
	BNodeInfo(file).GetType(buffer);
	_data->SetHeaderField("Content-Type",buffer);
	
	SetFileName(ref->name);
	
	//------This is likely to give a completely random string---
	BString boundary;
	strcpy(buffer, ref->name);
	for (int32 i = strlen(buffer);i-- > 0;)
	{
		if (buffer[i] & 0x80)
			buffer[i] = 'x';
		else if (buffer[i] == ' ')
			buffer[i] = '_';
	}
	boundary << "BFile:" << buffer << "--" << ((int32)file ^ time(NULL)) << ":" << ~((int32)file ^ (int32)&buffer ^ (int32)&_attributes) << "--";
	SetBoundary(boundary.String());
}

void AttributedMailAttachment::SaveToDisk(BEntry *entry) {
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

void AttributedMailAttachment::SetEncoding(mail_encoding encoding) {
	_data->SetEncoding(encoding);
	_attributes_attach->SetEncoding(encoding);
}

mail_encoding AttributedMailAttachment::Encoding() {
	return _data->Encoding();
}

status_t AttributedMailAttachment::FileName(char *name) {
	return _data->FileName(name);
}
	
void AttributedMailAttachment::SetFileName(const char *name) {
	_data->SetFileName(name);
}

status_t AttributedMailAttachment::GetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		*node << _attributes;
		
	_data->GetDecodedData(data);
	return B_OK;
}

status_t AttributedMailAttachment::SetDecodedData(BPositionIO *data) {
	BNode *node = dynamic_cast<BNode *>(data);
	if (node != NULL)
		_attributes << *node;
		
	_data->SetDecodedData(data);
	return B_OK;
}
		
status_t AttributedMailAttachment::SetToRFC822(BPositionIO *data, size_t length, bool parse_now) {
	status_t err;
	
	err = MIMEMultipartContainer::SetToRFC822(data,length,parse_now);
	if (err < B_OK)
		return err;
		
	BMimeType type;
	MIMEMultipartContainer::MIMEType(&type);
	if (strcmp(type.Type(),"multipart/x-bfile") != 0)
		return B_BAD_TYPE;
		
	if (_data != NULL)
		delete _data;
	if (_attributes_attach != NULL)
		delete _attributes_attach;
	_attributes.MakeEmpty();

	// get data and attributes	
	if ((_data = (SimpleMailAttachment *)GetComponent(0)) == NULL)
		return B_BAD_VALUE;
	if ((_attributes_attach = (SimpleMailAttachment *)GetComponent(1)) == NULL
		|| _attributes_attach->GetDecodedData() == NULL)
		return B_BAD_VALUE;

	int32		len;
	int32		index = 0;
	type_code	code;
	char		*name;
	len = ((BMallocIO *)(_attributes_attach->GetDecodedData()))->BufferLength();
	char *start = (char *)malloc(len);
	_attributes_attach->GetDecodedData()->ReadAt(0,start,len);
	
	while (index < len) {
		name = &start[index];
		index += strlen(name) + 1;
		memcpy(&code, &start[index], sizeof(type_code));
		code = B_BENDIAN_TO_HOST_INT32(code);
		index += sizeof(type_code);
		memcpy(&length, &start[index], sizeof(length));
		length = B_BENDIAN_TO_HOST_INT64(length);
		index += sizeof(length);
		swap_data(code, &start[index], length, B_SWAP_BENDIAN_TO_HOST);
		_attributes.AddData(name, code, &start[index], length);
		index += length;
	}
	
	free(start);
	
	return B_OK;
}
	
status_t AttributedMailAttachment::RenderToRFC822(BPositionIO *render_to) {
	BMallocIO *io = new BMallocIO;
#if B_BEOS_VERSION_DANO
	const
#endif
	char *name;
	const void *data;
	void *allocd;
	type_code type, swap_typed;
	int64 length, swapped;
	ssize_t dataLen;
	for (int32 i = 0; _attributes.GetInfo(B_ANY_TYPE,i,&name,&type) == B_OK; i++) {
		_attributes.FindData(name,type,&data,&dataLen);
		io->Write(name,strlen(name) + 1);
		swap_typed = B_HOST_TO_BENDIAN_INT32(type);
		io->Write(&swap_typed,sizeof(type_code));
		length = dataLen;
		swapped = B_HOST_TO_BENDIAN_INT64(length);
		io->Write(&swapped,sizeof(int64));
		allocd = malloc(dataLen);
		memcpy(allocd,data,dataLen);
		swap_data(type, allocd, dataLen, B_SWAP_HOST_TO_BENDIAN);
		io->Write(allocd,dataLen);
		free(allocd);
	}
	
	_attributes_attach->SetDecodedDataAndDeleteWhenDone(io);
	
	status_t err = MIMEMultipartContainer::RenderToRFC822(render_to);
	
	return err;
}

status_t AttributedMailAttachment::MIMEType(BMimeType *mime) {
	return _data->MIMEType(mime);
}
