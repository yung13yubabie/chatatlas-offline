#include "../src/request_control.hpp"
#include "../src/bridge_timing.hpp"
#include <cassert>
#include <iostream>
using namespace atlas;
struct Stop{};
struct FakeControl{bool stopped=false;void check(){if(stopped)throw Stop();}};
struct FakeHelpers{
 FakeControl* control=nullptr;int sent=0;
 void request(){BridgeRequestGate gate([&]{if(control)control->check();},[&]{if(control)control->check();});gate.beforeSend();++sent;gate.afterSend();gate.afterResponse();}
 void field(){request();}void account(){field();}
};
int main(){FakeHelpers h;FakeControl c,outer;
 {RequestControlScope scope(h.control,c);h.account();c.stopped=true;bool caught=false;try{h.account();}catch(const Stop&){caught=true;}assert(caught&&h.sent==1);}
 assert(h.control==nullptr);
 // Constructor/probe/enumeration may each comprise multiple helper requests.
 c.stopped=false;h.sent=0;{RequestControlScope scope(h.control,c);h.request();c.stopped=true;try{h.field();assert(false);}catch(const Stop&){}assert(h.sent==1);}
 h.control=&outer;try{RequestControlScope scope(h.control,c);h.field();}catch(const Stop&){}assert(h.control==&outer);
 std::cout<<"Nested helper stop gate and scoped lifetime checks passed\n";
}
