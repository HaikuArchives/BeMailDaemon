/* Filter - the base class for all mail filters
**
** Copyright 2001 Dr. Zoidberg Enterprises. All rights reserved.
*/


#include <String.h>

namespace Zoidberg {
namespace Mail {
	class _EXPORT Filter;
}
}

#include "MailAddon.h"

using Zoidberg::Mail::Filter;


Filter::Filter(BMessage *)
{
	//----do nothing-----
}

Filter::~Filter()
{
}


void Filter::_ReservedFilter1() {}
void Filter::_ReservedFilter2() {}
void Filter::_ReservedFilter3() {}
void Filter::_ReservedFilter4() {}

