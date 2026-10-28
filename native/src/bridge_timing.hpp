#pragma once
#include <chrono>
#include <functional>
#include <stdexcept>
#include <utility>

namespace atlas {
// Pause only while no request is executing. In-flight replies must keep draining
// so a pause cannot strand a UIA save action or fill its output pipe.
class BridgeRequestGate {
public:
 using Clock=std::chrono::steady_clock;
 using Time=Clock::time_point;
 using Now=std::function<Time()>;
private:
 enum class Phase { Before, Prepared, InFlight, Complete };
 std::function<void()> boundaryCheck,stopCheck;
 Now now;
 Clock::duration timeout;
 Time deadline{};
 Phase phase=Phase::Before;
public:
 BridgeRequestGate(std::function<void()> boundary,std::function<void()> stop,
                   Now clock=[] { return Clock::now(); },
                   Clock::duration limit=std::chrono::seconds(35))
   :boundaryCheck(std::move(boundary)),stopCheck(std::move(stop)),now(std::move(clock)),timeout(limit) {
  if(timeout<=Clock::duration::zero())throw std::invalid_argument("Invalid bridge timeout");
 }
 void beforeSend(){
  if(phase!=Phase::Before)throw std::logic_error("Bridge request already prepared");
  boundaryCheck();phase=Phase::Prepared;
 }
 void afterSend(){
  if(phase!=Phase::Prepared)throw std::logic_error("Bridge request not prepared");
  deadline=now()+timeout;phase=Phase::InFlight;
 }
 bool withinDeadline(){
  if(phase!=Phase::InFlight)throw std::logic_error("No bridge request in flight");
  stopCheck();return now()<deadline;
 }
 void afterResponse(){
  if(phase!=Phase::InFlight)throw std::logic_error("No bridge response expected");
  stopCheck();phase=Phase::Complete;boundaryCheck();
 }
};
}
