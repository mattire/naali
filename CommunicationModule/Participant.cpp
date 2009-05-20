#include "StableHeaders.h"
#include "Foundation.h"
#include "CommunicationManager.h"
#include "Participant.h"

namespace Communication
{
	Participant::Participant(ContactPtr contact): contact_(contact), message_count_(0), last_message_time_stamp_("")
	{
		im_messages_ = IMMessageListPtr( new IMMessageList());
	}

	ContactPtr Participant::GetContact() const
	{
		return contact_;
	}

	//ContactInfoPtr Participant::GetCurrentContactInfo() const
	//{
	//	return current_contact_info_;
	//}

} // end of namespace: Communication