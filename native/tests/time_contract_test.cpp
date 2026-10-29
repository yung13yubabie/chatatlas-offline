#include "core.hpp"
#include <chrono>
#include <iostream>
using namespace atlas;
static unsigned checks=0;
static void expect(bool ok,const char* name){++checks;if(!ok)throw Error(name);}
static void reject(const std::string& input){bool caught=false;try{normalizeTime(input);}catch(const Error&){caught=true;}expect(caught,"Unsupported date accepted");}
int main(){try{
 // Independent standard-calendar boundary confirms the UTC year limit.
 constexpr auto end=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::sys_days{std::chrono::year{3000}/std::chrono::January/1}.time_since_epoch()).count();
 static_assert(end==32503680000LL);
 for(const auto& [input,want]:std::vector<std::pair<std::string,std::string>>{
 {"1970-01-01T00:00:00Z","1970-01-01T00:00:00.000+00:00"},
 {"1970-01-01T14:00:00+14:00","1970-01-01T00:00:00.000+00:00"},
 {"1970-01-01T00:00:00-14:00","1970-01-01T14:00:00.000+00:00"},
 {"2999-12-31T23:59:59.999999999Z","2999-12-31T23:59:59.999+00:00"},
 {"2999-12-31T09:59:59-14:00","2999-12-31T23:59:59.000+00:00"},
 {"2999-12-31T23:59:59+14:00","2999-12-31T09:59:59.000+00:00"},
 {"2026-01-01T00:00:00+08:00","2025-12-31T16:00:00.000+00:00"},
 {"2026-12-31T23:59:59-08:00","2027-01-01T07:59:59.000+00:00"},
 {"2024-02-29T12:34:56.1+05:30","2024-02-29T07:04:56.100+00:00"},
 {"2000-02-29T23:59:59.1234-03:30","2000-03-01T03:29:59.123+00:00"},
 {"2026-10-29T08:00:00-00:00","2026-10-29T08:00:00.000+00:00"}}){auto got=normalizeTime(input);expect(got==want,"UTC normalization changed");expect(normalizeTime(got)==got,"UTC normalization is not idempotent");}
 for(const auto input:{"1970-01-01T00:00:00+14:00","1970-01-01T13:59:59.999+14:00","1970-01-01T00:00:00+00:01","2999-12-31T10:00:00-14:00","2999-12-31T23:59:59-14:00","2999-12-31T23:59:59-00:01","1969-12-31T23:59:59Z","3000-01-01T00:00:00Z","2023-02-29T00:00:00Z","2100-02-29T00:00:00Z","2026-01-01T24:00:00Z","2026-01-01T00:00:60Z","2026-01-01T00:00:00+14:01","2026-01-01T00:00:00+15:00","2026-01-01T00:00:00","malformed"})reject(input);
 expect(normalizeTime("").empty(),"Absent optional time changed");auto current=now();expect(normalizeTime(current)==current,"System clock output not canonical");
 std::cout<<"UTC time contract: "<<checks<<" passed (Linux/Windows supported range; no system clock changes)\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
