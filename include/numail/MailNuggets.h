#ifndef ZOIDBERG_SCHEDULED_EVENT_H
#define ZOIDBERG_SCHEDULED_EVENT_H
#include <Archivable.h>

struct ScheduledEvent: public BArchivable
{
	enum Period{ YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, SECOND, NPERIODS };
	struct Repetition
	{
		Period period;
		vector<bool> mask;
	};
	vector<Repetion> active;
	bigtime_t[2] time_range;
	
	// For p>=0 and q>0, if the p-th bit of mask is set in
	// the q-th Repetition of active, then the p-th period
	// of every "active" active[q-1].period is "active".
	// We take the time period:
	//
	//    active[0].mask.size()*active[0].period
	//
	// as the base time period, assuming all are "active".
	//
	// Examples:
	// "the third thursday of every month"
	// => { {MONTH, 0xFFF}, {WEEK, 0x8}, {DAY, 0x8} }
	// (DAY must be determined by rotating the DAY mask by
	// the day number of the first of its enclosing period).
	// 
	// "the third week of August"
	// => { {MONTH, 0x080}, {WEEK, 0x8} }
	//
	// "the friday after the third wednesday of April"
	// => { {MONTH, 0x008}, {WEEK, 0x8}, {DAY, 0x4}, {DAY, 0x10} }
	
	time_t NextOccurrenceAfter(time_t date);
	time_t EventLength();
	
	ScheduledOccurrence(BMessage*);
	virtual Archive(BMessage*, bool);
static BArchiveable* instantiate(BMessage*);
};

class MailAddress;
class MailAccount;

#endif
