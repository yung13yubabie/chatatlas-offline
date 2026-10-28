#include "core.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace atlas;
static J item(const J& ordinal){return {{"source_id","synthetic-item"},{"kind","message"},{"ordinal",ordinal}};}
int main(){
 for(const J& value:{J(0),J(-7),J(std::numeric_limits<int64_t>::min()),J(std::numeric_limits<int64_t>::max()),J(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))}){
  auto input=item(value);validateItem(input);assert(input["ordinal"]==value);
 }
 for(const J& value:{J(static_cast<uint64_t>(std::numeric_limits<int64_t>::max())+1),J(std::numeric_limits<uint64_t>::max()),J(nullptr),J(true),J("1"),J(1.25),J::array(),J::object()}){
  auto input=item(value);bool rejected=false;try{validateItem(input);}catch(const Error&){rejected=true;}assert(rejected);
 }
 J missing={{"source_id","synthetic-default"},{"kind","message"}};validateItem(missing);assert(missing["ordinal"]==0);
 std::cout<<"item value contract tests passed\n";
}
