#include <String.h>
#include <List.h>
#include <Mime.h>

#include <string.h>
#include <unistd.h>
#include <malloc.h>

class _EXPORT MIMEMultipartContainer;

#include <MailContainer.h>
#include <MailAttachment.h>


typedef struct message_part {
	message_part(off_t start, off_t end) { this->start = start; this->end = end; }
	
	int32 start;
	int32 end;
} message_part;

enum multipart_reader_states {
	CHECK_BOUNDARY_STATE = 4,
	CHECK_PART_STATE,
	
	FOUND_BOUNDARY_STATE,
	FOUND_END_STATE
};


MIMEMultipartContainer::MIMEMultipartContainer(const char *boundary, const char *this_is_an_MIME_message_text)
	:
	_boundary(NULL),
	_MIME_message_warning(this_is_an_MIME_message_text),
	_io_data(NULL) {		
		SetHeaderField("MIME-Version","1.0");
		SetHeaderField("Content-Type","multipart/mixed");
		SetBoundary(boundary);
	}

/*MIMEMultipartContainer::MIMEMultipartContainer(MIMEMultipartContainer &copy) :
	MailComponent(copy), 
	_boundary(copy._boundary),
	_MIME_message_warning(copy._MIME_message_warning),
	_io_data(copy._io_data) {		
		AddHeaderField("MIME-Version","1.0");
		AddHeaderField("Content-Type","multipart/mixed");
		SetBoundary(boundary);
	}*/

MIMEMultipartContainer::~MIMEMultipartContainer() {
	for (int32 i = 0; i < _components_in_raw.CountItems(); i++)
		delete (message_part *)_components_in_raw.ItemAt(i);

	for (int32 i = 0; i < _components_in_code.CountItems(); i++)
		delete (MailComponent *)_components_in_code.ItemAt(i);

	if (_boundary != NULL)
		free((void *)_boundary);
}

void MIMEMultipartContainer::SetBoundary(const char *boundary) {
	_boundary = strdup(boundary);
	
	BMessage structured;
	HeaderField("Content-Type",&structured);
	
	if (boundary == NULL)
		structured.RemoveName("boundary");
	else if (structured.ReplaceString("boundary",_boundary) != B_OK)
		structured.AddString("boundary",_boundary);
	
	SetHeaderField("Content-Type",&structured);
}

void MIMEMultipartContainer::SetThisIsAnMIMEMessageText(const char *text) {
	_MIME_message_warning = text;
}

status_t MIMEMultipartContainer::AddComponent(MailComponent *component) {
	if (!_components_in_code.AddItem(component))
		return B_ERROR;
	if (_components_in_raw.AddItem(NULL))
		return B_OK;

	_components_in_code.RemoveItem(component);
	return B_ERROR;
}

MailComponent *MIMEMultipartContainer::GetComponent(int32 index) {
	if (MailComponent *component = (MailComponent *)_components_in_code.ItemAt(index))
		return component;	//--- Handle easy case

	message_part *part = (message_part *)(_components_in_raw.ItemAt(index));
	if (part == NULL)
		return NULL;

	_io_data->Seek(part->start,SEEK_SET);
	
	MailComponent component;
	if (component.Instantiate(_io_data,part->end - part->start) < B_OK)
		return NULL;

	MailComponent *piece = component.WhatIsThis();
	
	/* Debug code 
	_io_data->Seek(part->start,SEEK_SET);
	char *data = new char[part->end - part->start + 1];
	_io_data->Read(data,part->end - part->start);
	data[part->end - part->start] = 0;
	puts((char *)(data));
	printf("Instantiating from %d to %d (%d octets)\n",part->start, part->end, part->end - part->start);
	*/
	_io_data->Seek(part->start,SEEK_SET);
	if (piece->Instantiate(_io_data,part->end - part->start) < B_OK)
	{
		delete piece;
		return NULL;
	}
	_components_in_code.ReplaceItem(index,piece);
	
	return piece;
}

int32 MIMEMultipartContainer::CountComponents() const {
	return _components_in_code.CountItems();
}

status_t MIMEMultipartContainer::RemoveComponent(int32 index) {
	if (index >= CountComponents())
		return B_BAD_INDEX;

	delete (MailComponent *)_components_in_code.RemoveItem(index);
	delete (message_part *)_components_in_raw.RemoveItem(index);

	return B_OK;
}

status_t MIMEMultipartContainer::ManualGetComponent(MailComponent *component, int32 index) {
	if (_components_in_code.ItemAt(index) != NULL)
		return B_NAME_IN_USE;
		
	message_part *part = (message_part *)(_components_in_raw.ItemAt(index));
	if (part == NULL)
		return B_BAD_INDEX;
	
	_io_data->Seek(part->start,SEEK_SET);
	
	return component->Instantiate(_io_data,part->end - part->start);
}

status_t MIMEMultipartContainer::GetDecodedData(BPositionIO *) {
	return B_BAD_TYPE; //------We don't play dat
}

status_t MIMEMultipartContainer::SetDecodedData(BPositionIO *) {
	return B_BAD_TYPE; //------We don't play dat
}

static int8 check_state(char *buffer, int32 pos, int32 bytes)
{
	buffer += pos;

	if (*buffer == '\r')
		return FOUND_BOUNDARY_STATE;
	if (!strncmp(buffer, "--\r", bytes - pos > 2 ? 3 : 2))
		return FOUND_END_STATE;

	return 0;
}

status_t MIMEMultipartContainer::Instantiate(BPositionIO *data, size_t length)
{
	for (int32 i = _components_in_code.CountItems();i-- > 0;)
		delete (MailComponent *)_components_in_code.RemoveItem(i);

	for (int32 i = _components_in_raw.CountItems();i-- > 0;)
		delete (message_part *)_components_in_raw.RemoveItem(i);

	_io_data = data;
	
	off_t position = data->Position();
	MailComponent::Instantiate(data,length);

	BMessage content_type;
	HeaderField("Content-Type",&content_type);
	if (strncmp(content_type.FindString("unlabeled"),"multipart",9) != 0)
		return B_BAD_TYPE;
		
	if (!content_type.HasString("boundary"))
		return B_BAD_TYPE;
		
	_boundary = strdup(content_type.FindString("boundary"));
	
	//
	//	Find container parts
	//
	
	int32 boundaryLength = strlen(_boundary);
	int32 boundaryPos = 0;
	const char *expected = "\r\n--";
	int8 state = 2;		// begin looking for '-'
	char buffer[4096];	// buffer size must be at least as long as boundaryLength

	off_t offset = data->Position();
	off_t end = length + position;
	int32 lastBoundary = -1;

	while (offset < end)
	{
		ssize_t bytes = (offset + sizeof(buffer)) > end ? end - offset : sizeof(buffer);
		bytes = data->Read(buffer, bytes);
		if (bytes <= 0)
			break;
		
		for (int32 i = 0;i < bytes;i++)
		{
			switch (state)
			{
				case CHECK_BOUNDARY_STATE:	// check for boundary
					if ((bytes - i) > boundaryLength)	// string fits
					{
						boundaryPos = 0;
						if (!strncmp(buffer + i, _boundary, boundaryLength))
							state = check_state(buffer,boundaryLength + i,bytes);
						else
							state = 0;
					}
					else if (!strncmp(buffer + i, _boundary, boundaryPos = bytes - i))
					{
						state++;
						i += boundaryPos;
					}
					else
						state = 0;
					break;
				case CHECK_PART_STATE:		// check for part of boundary
					if (!strncmp(buffer + i, _boundary + boundaryPos, boundaryLength - boundaryPos))
						state = check_state(buffer,boundaryLength - boundaryPos + i,bytes);
					else
						state = 0;
					break;
				default:
					if (buffer[i] == expected[state])
						state++;
					else if (state)
					{
						if (buffer[i] == expected[0])	// re-evaluate character
							state = 1;
						else
							state = 0;
					}
					break;
			}
			if (state >= FOUND_BOUNDARY_STATE)
			{
				if (lastBoundary >= 0)
				{
					_components_in_raw.AddItem(new message_part(lastBoundary, offset + i - boundaryPos - 2));
					_components_in_code.AddItem(NULL);
				}
				if (state == FOUND_END_STATE)
					break;

				i += boundaryLength - boundaryPos;
				lastBoundary = offset + i + 2;	// "--" boundary "\r\n"
				state = 0;
			}
		}
		if (state == FOUND_END_STATE)
			break;
		offset += bytes;
	}
	return B_OK;
}

status_t MIMEMultipartContainer::Render(BPositionIO *render_to) {
	MailComponent::Render(render_to);
	
	BString delimiter;
	delimiter << "\r\n--" << _boundary << "\r\n";
	
	if (_MIME_message_warning != NULL) {
		render_to->Write(_MIME_message_warning,strlen(_MIME_message_warning));
		render_to->Write("\r\n",2);
	}
	
	for (int32 i = 0; i < _components_in_code.CountItems() /* both have equal length, so pick one at random */; i++) {
		render_to->Write(delimiter.String(),delimiter.Length());
		if (_components_in_code.ItemAt(i) != NULL) { //---- _components_in_code has precedence
			
			MailComponent *code = (MailComponent *)_components_in_code.ItemAt(i);
			code->Render(render_to); //----Easy enough
		} else {
			// copy message contents

			uint8 buffer[1024];
			ssize_t length;
			message_part *part = (message_part *)_components_in_raw.ItemAt(i);
			
			for (off_t begin = part->start; begin < part->end; begin += sizeof(buffer)) {
				length = ((part->end - begin) >= sizeof(buffer)) ? sizeof(buffer) : (part->end - begin);
				
				_io_data->ReadAt(begin,buffer,length);
				render_to->Write(buffer,length);
			}
		}
	}
	
	render_to->Write(delimiter.String(),delimiter.Length() - 2);	// strip CRLF
	render_to->Write("--\r\n",4);
	
	return B_OK;
}
