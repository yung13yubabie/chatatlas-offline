#include "bridge_timing.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace atlas;
using namespace std::chrono_literals;
namespace {
int checks=0;
void expect(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
struct StopRequested {};
template<class F> void expectStop(F fn,const char* label){bool stopped=false;try{fn();}catch(const StopRequested&){stopped=true;}expect(stopped,label);}
}
int main(){try{
 using Time=BridgeRequestGate::Time;
 Time time{};bool paused=true,stopped=false,sent=false,replyDrained=false;
 int boundaries=0,polls=0;std::vector<std::string> events;
 auto stop=[&]{++polls;if(stopped)throw StopRequested();};
 auto boundary=[&]{
  ++boundaries;events.push_back("boundary");stop();
  if(paused){
   expect(!sent||replyDrained,"pause never blocks an in-flight reply");
   time+=120s;paused=false; // Simulates a long cooperative wait without sleeping.
   stop();
  }
 };
 auto clock=[&]{return time;};
 BridgeRequestGate first(boundary,stop,clock);
 first.beforeSend();expect(!sent&&boundaries==1,"paused preflight happens before sending");
 sent=true;first.afterSend();expect(first.withinDeadline(),"preflight pause consumes no request budget");
 paused=true;time+=1s;const auto prior=boundaries;
 expect(first.withinDeadline(),"paused in-flight request keeps draining");
 expect(boundaries==prior&&paused,"poll does not enter cooperative pause");
 replyDrained=true;events.push_back("reply drained");first.afterResponse();
 expect(boundaries==2&&!paused,"pause is honored only after response drain");
 expect(events.size()==3&&events[1]=="reply drained"&&events[2]=="boundary","response is consumed before post-response pause");

 sent=false;replyDrained=false;
 BridgeRequestGate next(boundary,stop,clock);next.beforeSend();sent=true;next.afterSend();
 time+=34s;expect(next.withinDeadline(),"next request has a fresh deadline after long pause");
 time+=1s;expect(!next.withinDeadline(),"active in-flight wait expires at 35 seconds");

 sent=false;paused=true;stopped=true;
 BridgeRequestGate beforeStop(boundary,stop,clock);
 expectStop([&]{beforeStop.beforeSend();},"stop interrupts paused preflight before send");
 expect(!sent,"stopped preflight sends no request");
 paused=false;stopped=false;
 BridgeRequestGate inFlight(boundary,stop,clock);inFlight.beforeSend();inFlight.afterSend();
 paused=true;stopped=true;
 expectStop([&]{inFlight.withinDeadline();},"stop is checked during a paused in-flight operation");
 expectStop([&]{inFlight.afterResponse();},"stop is checked before post-response pause");

 paused=false;stopped=false;
 BridgeRequestGate hung(boundary,stop,clock);hung.beforeSend();hung.afterSend();
 paused=true;time+=36s;
 expect(!hung.withinDeadline(),"a hung worker still times out while pause was requested");
 expect(polls>0,"in-flight checks are not busy waits or no-ops");

 bool invalid=false;try{BridgeRequestGate invalidGate(boundary,stop,clock,0s);}catch(const std::invalid_argument&){invalid=true;}expect(invalid,"nonpositive timeout is rejected");
 std::cout<<checks<<" deterministic bridge pause/deadline checks passed; no LINE process or real-time sleep used.\n";
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
