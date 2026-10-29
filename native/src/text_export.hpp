#pragma once
#include "core.hpp"

namespace atlas {
// Exports only these stored chat IDs, preserving each source's namespace.
// A stored chat ID is key({"chat", source.accountFor(chat),
//                          source.storageChat(chat)["source_id"]}).
// Each successful call publishes a new private text-exports/<generation> folder.
// No prior export is replaced; cancellation leaves no new completed generation.
// TXT contains saved message/thread text and verified attachment availability,
// never item metadata, media key material, or login/session credentials.
fs::path exportChatText(Store&,const std::vector<std::string>& chatIds,Tick={});
}
