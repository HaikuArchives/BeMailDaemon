#include <String.h>
#include <List.h>
#include <Mime.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>

class _EXPORT MIMEMultipartContainer;

#include <MailContainer.h>
#include <MailAttachment.h>

typedef struct message_part {
	message_part(off_t start, off_t end) {this->start = start; this->end = end;}
	
	int32 start;
	int32 end;
} message_part;

MIMEMultipartContainer::MIMEMultipartContainer(const char *boundary, const char *this_is_an_MIME_message_text) :
	MailComponent(), 
	_boundary(boundary),
	_MIME_message_warning(this_is_an_MIME_message_text),
	_io_data(NULL) {
		AddHeaderField("MIME-Version","1.0");
		AddHeaderField("Content-Type","multipart/mixed");
	}
		
void MIMEMultipartContainer::SetBoundary(const char *boundary) {
	_boundary = boundary;
	
	BString type = "multipart/mixed; ";
	type << "boundary=\"" << boundary << "\"";
	AddHeaderField("Content-Type",type.String());
}

void MIMEMultipartContainer::SetThisIsAnMIMEMessageText(const char *text) {
	_MIME_message_warning = text;
}

void MIMEMultipartContainer::AddComponent(MailComponent *component) {
	_components_in_code.AddItem(component);
	_components_in_raw.AddItem(NULL);
}

MailComponent *MIMEMultipartContainer::GetComponent(int32 index) {
	if (_components_in_code.ItemAt(index) != NULL)
		return (MailComponent *)(_components_in_code.ItemAt(index)); //--- Handle easy case
		
	message_part *part = (message_part *)(_components_in_raw.ItemAt(index));
	if (part == NULL)
		return NULL;
		
	_io_data->Seek(part->start,SEEK_SET);
	
	MailComponent component;
	component.Instantiate(_io_data,part->end - part->start);
	
	BMimeType type, super;
	component.MIMEType(&type);
	type.GetSupertype(&super);
	
	puts(type.Type());
	
	MailComponent *piece;
	//---------ATT-This code *desperately* needs to be improved
	if (super == "multipart") {
		/*if (type == "multipart/x-bfile")
			piece = new AttributedMailAttachment;
		else*/
			piece = new MIMEMultipartContainer;
	} else {
		const char *disposition = component.HeaderField("Content-Disposition");
		if ((disposition == NULL) || (strncasecmp(disposition,"Attachment",strlen("Attachment")) != 0))
			piece = new PlainTextBodyComponent;
		else
			piece = new SimpleMailAttachment;
	}
	/* Debug code 
	_io_data->Seek(part->start,SEEK_SET);
	char *data = new char[part->end - part->start + 1];
	_io_data->Read(data,part->end - part->start);
	data[part->end - part->start] = 0;
	puts((char *)(data));
	printf("Instantiating from %d to %d (%d octets)\n",part->start, part->end, part->end - part->start);
	*/
	
	_io_data->Seek(part->start,SEEK_SET);
	piece->Instantiate(_io_data,part->end - part->start);
	
	_components_in_code.ReplaceItem(index,piece);
	
	return piece;
}

int32 MIMEMultipartContainer::CountComponents() const {
	return _components_in_code.CountItems();
}

status_t MIMEMultipartContainer::GetDecodedData(BPositionIO * /*data*/) {
	return B_BAD_TYPE; //------We don't play dat
}

status_t MIMEMultipartContainer::SetDecodedData(BPositionIO * /*data*/) {
	return B_BAD_TYPE; //------We don't play dat
}

status_t MIMEMultipartContainer::Instantiate(BPositionIO *data, size_t length) {
	_io_data = data;
	
	if (length & 0x80000000)
		return B_BAD_VALUE;

	off_t position = data->Position();
	MailComponent::Instantiate(data,length);
	
	length -= (data->Position() - position);
		
	BString end_delimiter;
	BString type = HeaderField("Content-Type");
	if (type.FindFirst("multipart") < 0)
		return B_BAD_TYPE;
		
	if (type.FindFirst("boundary=") < 0)
		return B_BAD_TYPE;
		
	type.Remove(0, type.FindFirst("boundary=") + 9);
	if (type.FindFirst(' ') >= 0)
		type.Truncate(type.FindFirst(' '));
			
	if (type.ByteAt(0) == '\"')
		type.RemoveAll("\"");
		
	type.Prepend("--");

	end_delimiter << type << "--";
	
	int32 offset = data->Position();
	int32 start = offset + (int32)length;
	ssize_t len;
	BString line;
	char *buf;
	int32 last_boundary = -1;

	while (offset < start) {
		len = line.Length();
		buf = line.LockBuffer(len + 255) + len;
		len = data->ReadAt(offset,buf,90);
		buf[len] = 0;
		line.UnlockBuffer(len);
				
		if (len <= 0)
			break;
		
		len = line.FindFirst("\r\n");
		
		if (len < 0) {
			offset += line.Length();
			continue;
		}
		
		
		line.Truncate(len);
		
		if (strncmp(line.String(),type.String(), type.Length()) == 0) {
			if (last_boundary >= 0) {
				_components_in_raw.AddItem(new message_part(last_boundary, offset));
				_components_in_code.AddItem(NULL);
			}
			
			last_boundary = offset + len + 2;
			
			if (strncmp(line.String(),end_delimiter.String(), end_delimiter.Length()) == 0)
				break;
		}
		
		line = "";
		offset += len + 2 /* CRLF */;
	}
	
	return B_OK;
}

status_t MIMEMultipartContainer::Render(BPositionIO *render_to) {
	MailComponent::Render(render_to);
	
	BString delimiter;
	delimiter << "\r\n--" << _boundary << "\r\n";
		
	render_to->Write(_MIME_message_warning,strlen(_MIME_message_warning));
	render_to->Write("\r\n",2);
	
	
	for (int32 i = 0; i < _components_in_code.CountItems() /* both have equal length, so pick one at random */; i++) {
		render_to->Write(delimiter.String(),delimiter.Length());
		if (_components_in_code.ItemAt(i) != NULL) { //---- _components_in_code has precedence
			
			MailComponent *code = (MailComponent *)_components_in_code.ItemAt(i);
			code->Render(render_to); //----Easy enough
		} else {
			uint8 copy_buf[255];
			ssize_t len_to_copy = 255;
			message_part *part = (message_part *)_components_in_raw.ItemAt(i);
			
			for (off_t begin = part->start; begin < part->end; begin += 255) {
				len_to_copy = ((part->end - begin) >= 255) ? 255 : (part->end - begin);
				
				_io_data->ReadAt(begin,copy_buf,len_to_copy);
				render_to->Write(copy_buf,len_to_copy);
			} //------Also not difficult
		}
	}
	
	delimiter.Truncate(delimiter.Length() - 2 /* strip CRLF */);
	delimiter << "--\r\n";
	
	render_to->Write(delimiter.String(),delimiter.Length());
	
	return B_OK;
}