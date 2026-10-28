#pragma once
namespace atlas {
// The scope gives every nested helper request the operation's control without
// storing a dangling Control pointer when a reusable source outlives its task.
template<class Control> class RequestControlScope {
 Control*& slot;Control* previous;
public:
 RequestControlScope(Control*& target,Control& current):slot(target),previous(target){slot=&current;}
 ~RequestControlScope(){slot=previous;}
 RequestControlScope(const RequestControlScope&)=delete;
};
}
