#pragma once
namespace atlas {
// A thumbnail is optional once the matching original is actually saved.
// Blob integrity remains checked independently, including retained previews.
inline constexpr const char* requiredAttachmentSql=
 "NOT (a.variant='thumbnail' AND EXISTS(SELECT 1 FROM attachments original "
 "WHERE original.chat_id=a.chat_id AND original.item_id=a.item_id "
 "AND original.source_id=a.source_id AND original.version=a.version "
 "AND original.variant='full' AND original.status='saved' AND original.blob_id IS NOT NULL))";
}
