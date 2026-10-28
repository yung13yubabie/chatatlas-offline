#include "core.hpp"
namespace atlas {
const char*web_template=R"ATLAS(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self' 'sha256-@@CSP_SCRIPT@@'; style-src 'sha256-@@CSP_STYLE@@'; img-src 'self' data:; media-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'">
<meta name="referrer" content="no-referrer"><title>聊藏 · 知識地圖</title><style>@@STYLE@@</style></head>
<body><a class="skip" href="#content">跳到內容</a>
<header class="app-header"><div class="wordmark"><svg viewBox="0 0 34 34" aria-hidden="true"><path d="M8 9L25 7L23 26L8 22Z"/><circle cx="8" cy="9" r="4"/><circle cx="25" cy="7" r="4"/><circle cx="23" cy="26" r="4"/><circle cx="8" cy="22" r="3"/></svg><div><h1 id="app-title">聊藏</h1><span>對話收藏</span></div></div><div class="header-note"><span id="sample-label" hidden>人工範例</span><span class="offline"><i></i>離線收藏</span><details class="collection-info"><summary aria-label="關於這份收藏">收藏說明</summary><div class="info-popover"><strong>內容留在這份資料夾</strong><p>閱讀、搜尋與關聯探索不需連線。請保留整份資料夾，避免圖片或檔案遺失。</p><p>關鍵字只協助找內容，不代表人物之間有真實關係。每個連結都能回到原文核對。</p></div></details></div></header>
<div class="app-shell"><aside class="sidebar"><div class="sidebar-heading">我的收藏<span id="chat-count"></span></div><button id="all-chats" class="all-chats">全部聊天室</button><nav id="chats" aria-label="聊天室"></nav><div class="sidebar-bottom"><span class="tiny-map">✦</span><p>選一個聊天室，<br>從熟悉的名字開始。</p></div></aside>
<main id="content"><div class="page-heading"><div><div class="eyebrow">我的對話收藏</div><h2 id="title">知識地圖</h2><p id="coverage"></p></div><div id="stats" class="stats"></div></div>
<div class="workspace-toolbar"><div class="view-tabs" role="tablist" aria-label="閱讀方式"><button id="view-map" role="tab" aria-controls="map-section" tabindex="0" aria-selected="true">關聯地圖</button><button id="view-reading" role="tab" aria-controls="reading-section" tabindex="-1" aria-selected="false">閱讀內容</button><button id="view-files" role="tab" aria-controls="files-section" tabindex="-1" aria-selected="false">圖片與檔案</button></div><label class="search"><span>搜尋</span><input id="search" type="search" placeholder="搜尋主題、人物或檔名" autocomplete="off" aria-label="搜尋主題、人物或檔名"></label><details class="filter-menu"><summary>篩選<span id="filter-count" hidden></span></summary><div class="filter-popover"><label>範圍<select id="scope"><option value="chat">目前聊天室</option><option value="all">全部聊天室</option></select></label><label>內容<select id="kind"><option value="all">全部內容</option><option value="message">對話</option><option value="album">相簿</option><option value="note">記事本</option><option value="thread">討論串</option><option value="files">附件</option><option value="issues">待補件</option></select></label><label>開始日期<input id="since" type="date"></label><label>結束日期<input id="until" type="date"></label><label>發言者<input id="sender" placeholder="姓名或暱稱"></label><button id="clear" class="secondary">清除篩選</button></div></details></div>
<div class="active-context"><span id="filter-summary">全部內容</span><button id="reset-filters" class="text-button" hidden>清除篩選</button></div><p id="notice" role="status" aria-live="polite"></p><div class="download-status"><span id="download-notice" role="status" aria-live="polite"></span><button id="cancel-download" class="secondary" hidden>停止另存</button></div>
<section id="map-section" class="map-workspace view-panel" role="tabpanel" aria-labelledby="view-map" tabindex="0"><div class="map-area"><div class="map-caption"><span>選一個節點，看看相關對話</span><span id="map-size"></span></div><svg id="knowledge-map" viewBox="0 0 1000 640" role="group" aria-label="可點選的主題、人物、內容與附件"><defs><pattern id="dots" width="24" height="24" patternUnits="userSpaceOnUse"><circle cx="1" cy="1" r="1" fill="#d9dfd9"/></pattern></defs><rect width="1000" height="640" fill="url(#dots)"/></svg><div id="map-empty" class="map-empty" hidden>這裡還沒有符合條件的內容。</div><div class="map-bottom"><div class="legend"><span class="topic">關鍵字</span><span class="person">人物</span><span class="source">內容</span><span class="file">附件</span></div><div class="map-controls"><button id="clear-map-selection" class="text-button" hidden>顯示全部</button><button id="zoom-out" class="icon-button" aria-label="縮小地圖">−</button><button id="map-center" class="icon-button" aria-label="重設地圖縮放與位置">⌖</button><span id="zoom-level" aria-live="polite">100%</span><button id="zoom-in" class="icon-button" aria-label="放大地圖">＋</button></div></div></div><aside id="node-detail" class="detail-panel" aria-live="polite"></aside></section>
<section id="reading-section" class="view-panel" role="tabpanel" aria-labelledby="view-reading" tabindex="0" hidden><div class="reading-heading"><h3 id="reading-label">全部內容</h3><button id="clear-node" class="secondary" hidden>顯示全部</button></div><div id="items"></div><div class="pagination"><button id="previous" class="secondary">上一頁</button><span id="page"></span><button id="next" class="secondary">下一頁</button></div></section>
<section id="files-section" class="view-panel" role="tabpanel" aria-labelledby="view-files" tabindex="0" hidden><div class="files-toolbar"><span>檔案已保存在這份收藏裡，可以反覆另存。</span><div><button id="save-selected" class="secondary" disabled>另存選取項目</button><span id="selected-count"></span></div></div><div id="file-grid" class="file-grid"></div><div class="pagination"><button id="file-previous" class="secondary">上一頁</button><span id="file-page"></span><button id="file-next" class="secondary">下一頁</button></div></section>
<footer><span>聊藏 · 你的離線知識庫</span><span>時間 UTC+8</span></footer></main></div>
<dialog id="preview" aria-label="檔案預覽"><button id="close-preview" class="secondary">關閉</button><div id="preview-content"></div></dialog>
<script type="application/json" id="manifest">@@MANIFEST@@</script><script>@@SCRIPT@@</script></body></html>
)ATLAS";
const char*web_css=R"ATLAS(:root{font-family:system-ui,"Microsoft JhengHei",sans-serif;color:#26362f;background:#f7f8f5;font-size:15px;color-scheme:light;--green:#38685a;--line:#dce3dc;--muted:#5f7167;--paper:#fff;--soft:#edf2ed;--motion:180ms;--ease:cubic-bezier(.2,.8,.2,1)}
*{box-sizing:border-box}body{margin:0}button,input,select{font:inherit}button{cursor:pointer;background:var(--green);color:#fff;border:1px solid transparent;border-radius:9px;padding:10px 15px;font-weight:600;min-height:40px;transition:background-color var(--motion),border-color var(--motion),color var(--motion),opacity var(--motion)}button:hover{filter:brightness(.96)}button:disabled{opacity:.45;cursor:default}button.secondary{background:#fff;border-color:#cbd7cd;color:#385647;font-weight:500}.text-button{background:transparent;color:var(--green);font-size:12px;min-height:36px;padding:6px 9px}.text-button:hover{background:#e8efe8}input,select{border:1px solid #cbd7cd;border-radius:8px;background:#fff;color:#263e32;padding:10px 12px;min-width:0;min-height:40px}input::placeholder{color:#748177}input:focus-visible,select:focus-visible,button:focus-visible,summary:focus-visible,a:focus-visible,.view-panel:focus-visible{outline:3px solid #89ae9d;outline-offset:3px}[hidden]{display:none!important}
.app-header{height:82px;background:#fff;border-bottom:1px solid var(--line);padding:0 32px;display:flex;align-items:center;justify-content:space-between}.wordmark{display:flex;gap:13px;align-items:center}.wordmark svg{width:35px;height:35px;fill:var(--green)}.wordmark svg path{fill:none;stroke:#7a9e8b;stroke-width:1.6}.wordmark h1{font-size:23px;font-weight:700;letter-spacing:.12em;line-height:1;margin:0 0 6px}.wordmark span{font-size:12px;color:var(--muted)}.header-note{display:flex;align-items:center;gap:16px;color:#6a725f;font-size:12px}.offline{display:flex;gap:7px;align-items:center;color:#3d6654;background:#edf4ed;padding:7px 11px;border-radius:18px}.offline i{width:6px;height:6px;border-radius:50%;background:#548367}.collection-info{position:relative}.collection-info summary{list-style:none;cursor:pointer;min-height:40px;display:flex;align-items:center;border-radius:7px}.info-popover{position:absolute;right:0;top:45px;z-index:15;background:#fff;border:1px solid var(--line);box-shadow:0 10px 35px #26362f12;padding:20px;width:320px;border-radius:12px;line-height:1.75;color:var(--muted)}.info-popover strong{color:#26362f}.info-popover p{margin:10px 0 0}
.app-shell{display:grid;grid-template-columns:232px minmax(0,1fr);min-height:calc(100vh - 82px)}.sidebar{padding:30px 16px;border-right:1px solid var(--line);background:#fcfdfa;display:flex;flex-direction:column;gap:8px}.sidebar-heading{display:flex;justify-content:space-between;align-items:center;padding:0 12px;margin:0 0 12px;color:var(--muted);font-size:12px;letter-spacing:.04em}.sidebar-heading span{background:#e9eee7;padding:2px 7px;border-radius:10px}.sidebar nav{display:flex;flex-direction:column;gap:6px}.sidebar button{text-align:left;background:transparent;color:#53675b;font-size:14px;font-weight:500;border-radius:9px;padding:13px 12px;line-height:1.5}.sidebar button:hover{background:#f0f4ed}.sidebar button.active{color:#254d3d;background:#e7efe6;font-weight:650;box-shadow:inset 3px 0 #49785e}.sidebar .all-chats{border-bottom:1px solid var(--line);border-radius:0;margin-bottom:10px;padding-bottom:18px}.sidebar .all-chats.active{box-shadow:none;background:transparent;color:#315c48}.chat-label{display:block;overflow-wrap:anywhere}.chat-meta{display:block;font-size:12px;color:#6d7d6c;font-weight:400;margin-top:5px}.sidebar-bottom{margin:auto 12px 4px;padding-top:70px;color:#73836e;font-size:12px;line-height:1.8}.tiny-map{font-size:22px;color:#72916a}.sidebar-bottom p{margin:7px 0}
main{min-width:0;padding:34px 32px 18px;max-width:1800px;width:100%;margin:0 auto}.page-heading{display:flex;align-items:center;justify-content:space-between;gap:20px;margin:0 0 26px}.eyebrow{font-size:12px;color:var(--muted);letter-spacing:.06em;margin-bottom:8px}.page-heading h2{font-size:28px;font-weight:650;margin:0;color:#2d4c3b;letter-spacing:.02em;overflow-wrap:anywhere}.page-heading p{font-size:13px;color:var(--muted);margin:9px 0 0;line-height:1.7}.stats{font-size:12px;color:#60715e;white-space:nowrap}.workspace-toolbar{display:flex;align-items:center;gap:12px}.view-tabs{display:flex;gap:3px;background:#eaf0e8;border-radius:10px;padding:4px;flex-shrink:0}.view-tabs button{background:transparent;color:#5b7059;font-size:13px;padding:9px 13px;font-weight:500;border:0;min-height:38px}.view-tabs [aria-selected=true]{background:#fff;color:#2e5540;box-shadow:0 1px 4px #33462c15}.search{margin-left:auto;position:relative;min-width:180px;max-width:350px;flex:1}.search>span{position:absolute;left:12px;top:12px;font-size:13px;color:#687a61}.search input{width:100%;font-size:13px;padding-left:48px;height:44px}.filter-menu{position:relative;font-size:13px;color:#526b4c}.filter-menu summary{cursor:pointer;padding:12px 13px;border:1px solid #cbd7cd;border-radius:8px;list-style:none;background:#fff;white-space:nowrap;min-height:44px}.filter-menu summary>span{margin-left:7px;background:#e7efe5;padding:1px 5px;border-radius:5px;font-size:11px}.filter-menu[open] summary{border-color:#72936e;background:#f5f8f3}.filter-popover{z-index:12;position:absolute;right:0;top:52px;background:#fff;box-shadow:0 12px 35px #32462b18;border:1px solid var(--line);border-radius:12px;padding:20px;display:grid;grid-template-columns:1fr 1fr;gap:15px;width:350px}.filter-popover label{display:flex;flex-direction:column;gap:7px;font-size:12px;color:#5d7154;min-width:0}.filter-popover input,.filter-popover select{width:100%;font-size:13px}.filter-popover button{align-self:end}.active-context{min-height:42px;display:flex;align-items:center;gap:12px;color:#65775c;font-size:12px}.view-panel{animation:panel-in var(--motion) var(--ease)}
.map-workspace{display:grid;grid-template-columns:minmax(0,1fr) 320px;min-height:570px;border:1px solid #d6e0d2;background:#fff;border-radius:14px;overflow:hidden;box-shadow:0 5px 20px #45573b04}.map-area{position:relative;min-width:0;background:#fbfcf9;display:flex;flex-direction:column}.map-caption{padding:20px 22px 5px;display:flex;justify-content:space-between;gap:10px;font-size:12px;color:#667a5f;z-index:1}.map-caption span:last-child{color:#718367}.map-area>svg{width:100%;flex:1;min-height:450px;display:block;touch-action:pan-y}.map-bottom{padding:10px 18px 17px;display:flex;align-items:center;justify-content:space-between;gap:10px}.legend{display:flex;flex-wrap:wrap;gap:13px;font-size:11px;color:#617657}.legend span:before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;margin-right:5px}.legend .topic:before{background:#a97a3d}.legend .person:before{background:#737fa9}.legend .source:before{background:#638a75}.legend .file:before{background:#a66d7c}.map-controls{display:flex;align-items:center;background:#fff;border:1px solid #d4dfcf;border-radius:8px;overflow:hidden;flex-shrink:0}.icon-button{font-size:17px;border:0;border-radius:0;background:#fff;color:#516b46;padding:5px 10px;min-width:36px}.map-controls .text-button{border-right:1px solid var(--line);border-radius:0}#zoom-level{font-size:11px;min-width:35px;text-align:center;color:#62765a}.map-empty{position:absolute;inset:43% 12% auto;text-align:center;color:#637958;font-size:14px;line-height:1.7;background:#fbfcf9;padding:18px}.graph-edge{stroke:#d1dccb;stroke-width:1.2;transition:opacity var(--motion),stroke var(--motion)}.graph-edge.mentions{stroke-dasharray:3 5;stroke:#d5c9b2}.graph-edge.active{stroke:#77956b;stroke-width:2}.graph-edge.muted{opacity:.2}.graph-node{cursor:pointer;outline:none;transition:opacity var(--motion)}.graph-node text{fill:#4b6542;font-family:system-ui,"Microsoft JhengHei",sans-serif;font-size:15px;paint-order:stroke;stroke:#fbfcf9;stroke-width:5px;stroke-linejoin:round}.graph-node.chat text{font-size:17px;font-weight:650;fill:#31533d}.graph-node .node-halo{fill:transparent;stroke:transparent;stroke-width:2}.graph-node:hover .node-halo,.graph-node:focus .node-halo{fill:#cbdac326;stroke:#80a074}.graph-node:focus-visible .node-halo{stroke:#416e50;stroke-width:3}.graph-node.selected .node-halo{fill:#ccdcca50;stroke:#6f9866}.graph-node.selected text{fill:#264a30;font-weight:650}.graph-node.muted{opacity:.27}
.detail-panel{border-left:1px solid var(--line);padding:25px 22px;background:#fff;max-height:670px;overflow:auto}.detail-panel.revealing{animation:detail-in 200ms var(--ease)}.detail-type{font-size:12px;color:#566d4c;padding:5px 9px;background:#eef3e9;display:inline-block;border-radius:5px}.detail-panel h3{font-size:21px;line-height:1.5;color:#33542e;margin:14px 0 8px;overflow-wrap:anywhere}.detail-subtitle{font-size:13px;line-height:1.75;color:#637556;margin:0 0 20px}.detail-entry{border-top:1px solid #e6ece0;padding:17px 0}.detail-entry .meta{font-size:11px;gap:6px}.detail-entry .text{font-size:14px;color:#4f6744;line-height:1.85;display:-webkit-box;-webkit-line-clamp:5;-webkit-box-orient:vertical;overflow:hidden;white-space:pre-wrap;margin:9px 0}.detail-entry h4{font-size:14px;line-height:1.6;margin:8px 0;color:#385b2c}.detail-open{font-size:12px;border:0;background:transparent;padding:6px 0;color:#3a6939;font-weight:550;min-height:36px}.detail-panel>.secondary{width:100%;margin-top:15px;font-size:13px}.detail-count{font-size:12px;color:#5e7452;line-height:1.8;margin:18px 0 2px}.detail-panel .attachment{flex-wrap:wrap}.detail-panel .attachment .file-info{flex:1 1 130px;min-width:120px}.detail-panel .attachment img{width:56px;height:48px}.detail-panel .attachment button{font-size:12px;padding:7px 9px}
.reading-heading{display:flex;justify-content:space-between;align-items:center;margin:20px 0 14px}.reading-heading h3{font-size:18px;font-weight:550;color:#3d5e31;margin:0}.reading-heading button{font-size:13px}.card{background:#fff;border:1px solid #dce4d6;border-radius:12px;margin:12px 0;padding:23px 25px;scroll-margin-top:20px}.card:focus{outline:3px solid #89ae7d;outline-offset:3px}.meta{display:flex;flex-wrap:wrap;gap:9px;align-items:center;font-size:12px;color:#697b5d}.name{color:#365e2c;font-size:13px;font-weight:650}.type{font-size:11px;border-radius:4px;padding:3px 6px;background:#eef3e9;color:#5b714d}.card h3{font-size:18px;line-height:1.7;margin:13px 0 7px;color:#385a2e}.text{font-size:15px;line-height:1.85;white-space:pre-wrap;overflow-wrap:anywhere;margin:13px 0 0;color:#3f5935}.card>.secondary{font-size:12px;padding:6px 9px;margin:11px 8px 0 0}.context{font-size:12px;color:#6b7c5e}.attachment{display:flex;align-items:center;gap:12px;border-top:1px solid #e7ece1;margin-top:16px;padding-top:16px}.attachment img{width:65px;height:56px;object-fit:cover;border-radius:8px;background:#f0f3ec}.attachment input{accent-color:#4f7940}.file-info{flex:1;min-width:0}.filename{font-size:14px;font-weight:550;overflow-wrap:anywhere;color:#456438}.status{font-size:12px;color:#627653;line-height:1.8;margin-top:4px}.warning{color:#8c6226}.save-file{background:#e9f1e3;color:#3e692d;font-size:12px;padding:8px 11px;font-weight:550}.attachment .secondary{font-size:12px;padding:8px 10px}.empty{padding:65px 20px;border:1px dashed #c8d5bf;border-radius:12px;text-align:center;font-size:14px;color:#5e764e}.files-toolbar{display:flex;justify-content:space-between;align-items:center;gap:20px;padding:15px 0 23px;font-size:13px;color:#607651}.files-toolbar>div{display:flex;align-items:center;gap:10px}.files-toolbar button{font-size:13px}.file-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(235px,1fr));gap:17px}.file-card{background:#fff;border:1px solid #d7e1cf;border-radius:12px;overflow:hidden}.file-cover{height:165px;background:#eef3e8;display:flex;align-items:center;justify-content:center;color:#8a9e77;font-size:36px;position:relative}.file-cover img{width:100%;height:100%;object-fit:cover}.file-card-body{padding:17px}.file-card-body h3{font-size:14px;color:#446735;margin:0 0 8px;overflow-wrap:anywhere}.file-card-body p{font-size:12px;color:#637a52;margin:8px 0;line-height:1.6}.file-card-actions{display:flex;align-items:center;gap:8px;margin-top:16px}.file-card-actions input{accent-color:#527d3f}.file-card-actions button{font-size:12px;padding:7px 10px}.file-card-actions .save-file{margin-left:auto}.pagination{display:flex;justify-content:center;align-items:center;gap:20px;margin:26px 0;font-size:13px;color:#617a4f}.pagination button{font-size:13px}#notice,.download-status{font-size:13px;color:#88632d;line-height:1.8;margin:10px 0}#notice:empty,.download-status:has(#download-notice:empty):has(#cancel-download[hidden]){display:none}.download-status{display:flex;gap:12px;align-items:center;flex-wrap:wrap}.download-status button{font-size:12px}footer{display:flex;justify-content:space-between;margin:24px 0 0;padding:14px 0 0;border-top:1px solid #dce4d3;color:#687c58;font-size:11px}dialog{border:1px solid #cbdcc0;border-radius:14px;max-width:90vw;max-height:90vh;padding:16px}dialog::backdrop{background:#243a2677}#preview-content img,#preview-content video{display:block;max-width:80vw;max-height:76vh;margin:14px auto}#preview-content audio{margin:20px}.skip{position:fixed;top:-100px;padding:12px;background:#fff;z-index:20;color:var(--green)}.skip:focus{top:0}
@keyframes panel-in{from{opacity:.3;transform:translateY(5px)}to{opacity:1;transform:translateY(0)}}@keyframes detail-in{from{opacity:.4;transform:translateX(7px)}to{opacity:1;transform:translateX(0)}}
@media(min-width:1600px){.map-workspace{grid-template-columns:minmax(0,1fr) 350px}}@media(max-width:1200px){.app-shell{grid-template-columns:200px minmax(0,1fr)}main{padding:28px 22px 15px}.map-workspace{grid-template-columns:minmax(0,1fr) 275px}.detail-panel{padding:22px 18px}.workspace-toolbar{flex-wrap:wrap}.search{max-width:none}.view-tabs{margin-right:auto}.map-caption{padding-left:17px;padding-right:17px}.map-bottom{align-items:flex-start;flex-wrap:wrap}.legend{gap:10px}.map-controls{margin-left:auto}}
@media(max-width:820px){button,input,select{min-height:44px}.app-header{height:76px;padding:0 18px}.wordmark h1{font-size:22px}.wordmark span{font-size:11px}.header-note{gap:8px;font-size:11px}.offline{padding:6px 9px}.collection-info summary{font-size:11px}.info-popover{width:min(320px,calc(100vw - 36px));top:44px}.app-shell{grid-template-columns:1fr}.sidebar{padding:12px 16px;border-right:0;border-bottom:1px solid var(--line);display:flex;flex-direction:row;overflow:auto;gap:8px}.sidebar-heading,.sidebar-bottom{display:none}.sidebar nav{flex-direction:row;gap:7px}.sidebar button{white-space:nowrap;padding:9px 12px;font-size:13px;flex-shrink:0}.sidebar .all-chats{margin:0;border:0;border-radius:8px;padding:9px 12px;display:block}.sidebar .all-chats.active{background:#e7efe6}.sidebar button.active{box-shadow:inset 0 -2px #49785e}.chat-meta{display:none}.chat-label{white-space:nowrap}main{padding:25px 18px 14px}.page-heading{margin-bottom:21px;align-items:flex-start;flex-wrap:wrap;gap:10px}.page-heading h2{font-size:24px}.eyebrow{font-size:11px}.stats{font-size:12px}.page-heading p{max-width:100%;font-size:12px}.workspace-toolbar{gap:10px}.view-tabs{order:0;flex:1;justify-content:space-between;margin:0}.view-tabs button{padding:8px 9px;font-size:12px;min-height:40px}.filter-menu{order:1}.search{flex-basis:100%;order:2;margin:0}.filter-popover{width:min(350px,calc(100vw - 36px));right:0}.map-workspace{grid-template-columns:1fr}.map-area>svg{min-height:350px}.detail-panel{border-left:0;border-top:1px solid var(--line);max-height:none;padding:22px}.map-bottom{padding:10px 14px 17px;gap:12px}.map-caption{font-size:11px;padding:18px 14px 5px}.map-caption span:last-child:empty{display:none}.detail-panel h3{font-size:20px}.attachment{flex-wrap:wrap}.attachment .file-info{min-width:130px;flex:1 1 130px}.attachment button{flex-shrink:0}.files-toolbar{align-items:start;flex-direction:column;gap:12px}.file-grid{grid-template-columns:repeat(auto-fill,minmax(220px,1fr))}.card{padding:20px}.graph-node text{font-size:22px}.graph-node.chat text{font-size:24px}.graph-node .node-halo{r:32px}.graph-node .node-hit{r:30px}.legend{font-size:11px}.icon-button{min-width:40px}.map-controls{max-width:100%}.map-controls .text-button{font-size:11px;padding:4px 7px}#zoom-level{font-size:10px;min-width:31px}.active-context{font-size:12px}.header-note #sample-label{display:none}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{animation:none!important;transition:none!important;scroll-behavior:auto!important}}
)ATLAS";
const char*web_script=R"ATLAS("use strict";
window.VaultGraphModel = (() => {
 const stop=new Set(['我們','你們','他們','這個','那個','可以','已經','目前','如果','所以','以及','還是','沒有','一下','一起','這是','範例','人工','內容','資料','來源','附件','訊息','圖片','檔案','保存','下載','更新','完整','使用','提供','其他','自己','一個','就是','只有','需要','進行','不會','本次','仍會','這份','相同','請問','時候','如何','什麼','這裡','今天','明天','昨天','謝謝']);
 const hash=s=>{let h=2166136261;for(const ch of s){h^=ch.codePointAt(0);h=Math.imul(h,16777619);}return (h>>>0).toString(36);};
 const personKey=i=>'person:'+(i.sender_ref||i.chat_id+':name:'+i.sender_name);
 const label=i=>i.title||i.text?.replace(/\s+/g,' ').slice(0,28)||({message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'}[i.kind]||'內容');
 const tags=i=>{const values=[...(Array.isArray(i.tags)?i.tags:[]),...Array.from((i.text||'').matchAll(/#([\p{L}\p{N}_\-]{2,24})/gu),m=>m[1])];return [...new Set(values.filter(x=>typeof x==='string'&&x.length>=2&&x.length<=32))].slice(0,12);};
 function terms(rows){
  const counts=new Map(),explicit=new Set();let segmenter;
  try{segmenter=new Intl.Segmenter('zh-Hant',{granularity:'word'});}catch{}
  for(const row of rows){if(['recalled','deleted'].includes(row.state))continue;
   const local=new Set(tags(row));for(const t of local)explicit.add(t);
   const text=(row.title+' '+row.text).normalize('NFKC');
   const words=segmenter?Array.from(segmenter.segment(text),x=>x.isWordLike?x.segment:''):text.match(/[A-Za-z][A-Za-z0-9_-]{2,24}/g)||[];
   for(let w of words){w=w.trim();if(w.length<2||w.length>18||stop.has(w)||/^\d+$/.test(w))continue;local.add(w);}
   for(const t of local){if(!counts.has(t))counts.set(t,new Set());counts.get(t).add(row.id);}
  }
  return [...counts].filter(([t,ids])=>explicit.size?explicit.has(t):(ids.size>=3&&t.length>=2&&!['大家','容易','一份','示意','參考','素材','回程','一起','準備','需要','整理'].includes(t))).sort((a,b)=>(explicit.has(b[0])-explicit.has(a[0]))||(b[1].size-a[1].size)||a[0].localeCompare(b[0],'zh-Hant')).slice(0,explicit.size?12:6).map(([term,ids])=>({term,ids,explicit:explicit.has(term)}));
 }
 function build(rows,chats,options={}){
  const totalRows=rows.length;chats=chats.slice(0,20);const chatIds=new Set(chats.map(c=>c.id));rows=rows.filter(i=>chatIds.has(i.chat_id));
  const nodes=new Map(),links=new Map(),source=new Map(rows.map(i=>[i.id,i]));
  const max=options.limit||120,types=new Set(options.types||['topic','person','source','file']);
  function node(id,type,title,items=[],extra={}){if(nodes.has(id))return nodes.get(id);if(nodes.size>=max&&type!=='chat')return null;const n={id,type,title,items:[...new Set(items)],...extra};nodes.set(id,n);return n;}
  function edge(a,b,type,label){if(!a||!b||a===b||!nodes.has(a)||!nodes.has(b))return;const id=[a,b,type].join('|');links.set(id,{id,source:a,target:b,type,label});}
  const roots=new Map();
  for(const c of chats){const ids=rows.filter(i=>i.chat_id===c.id).map(i=>i.id);if(ids.length||chats.length===1){const id='chat:'+c.id;roots.set(c.id,id);node(id,'chat',c.title,ids,{chatId:c.id});}}
  for(const c of chats)if(c.parent_id&&roots.has(c.parent_id)&&roots.has(c.id))edge(roots.get(c.parent_id),roots.get(c.id),'belongs','社群中的聊天室');
  if(types.has('topic'))for(const t of terms(rows)){
   const n=node('topic:'+t.term,'topic',t.term,[...t.ids],{explicit:t.explicit});if(!n)continue;
   for(const cid of new Set([...t.ids].map(id=>source.get(id)?.chat_id)))edge(roots.get(cid),n.id,'mentions',t.explicit?'使用標籤':'提到關鍵字');
  }
  if(types.has('person')){
   const people=new Map();
   for(const i of rows){if(!i.sender_name)continue;const pid=personKey(i);if(!people.has(pid))people.set(pid,{name:i.sender_name,ids:[],chatIds:new Set(),identified:!!i.sender_ref});people.get(pid).ids.push(i.id);people.get(pid).chatIds.add(i.chat_id);}
   for(const [id,p]of [...people].sort((a,b)=>b[1].ids.length-a[1].ids.length).slice(0,20)){const n=node(id,'person',p.name,p.ids,{identified:p.identified});if(n)for(const cid of p.chatIds)edge(roots.get(cid),id,'speaker','這裡的發言者');}
  }
  const anchors=rows.filter(i=>['note','album','thread'].includes(i.kind));
  const chosen=(options.detail?rows:(anchors.length?anchors:rows.slice(0,4))).slice().sort((a,b)=>{const rank=x=>({note:0,album:1,thread:2,album_item:3,comment:4,message:5}[x.kind]??6);return rank(a)-rank(b)||String(b.sent_at||'').localeCompare(a.sent_at||'');}).slice(0,options.detail?60:18);
  if(types.has('source'))for(const i of chosen){const n=node('source:'+i.id,'source',label(i),[i.id],{kind:i.kind,itemId:i.id});if(!n)continue;edge(roots.get(i.chat_id),n.id,'belongs','聊天室中的內容');}
  for(const i of chosen){const own='source:'+i.id;if(!nodes.has(own))continue;
   if(i.parent_id)edge('source:'+i.parent_id,own,'context','所屬內容');
   const reply=rows.find(r=>r.chat_id===i.chat_id&&r.source_id===i.reply_source_id);if(reply)edge('source:'+reply.id,own,'reply','引用回覆');
   const person=personKey(i);if(i.sender_name)edge(person,own,'author','發言');
   for(const n of nodes.values())if(n.type==='topic'&&n.items.includes(i.id))edge(n.id,own,'mentions',n.explicit?'使用標籤':'提到關鍵字');
  }
  if(types.has('file'))for(const i of rows)for(const a of i.attachments||[]){const n=node('file:'+a.id,'file',a.name,[i.id],{attachment:a,itemId:i.id});if(!n)continue;const parent=nodes.has('source:'+i.id)?'source:'+i.id:(i.parent_id&&nodes.has('source:'+i.parent_id)?'source:'+i.parent_id:roots.get(i.chat_id));edge(parent,n.id,'attachment','這裡保存的附件');for(const t of nodes.values())if(t.type==='topic'&&t.items.includes(i.id))edge(t.id,n.id,'mentions','相關內容的附件');}
  // Every displayed non-room node has an observed containing room as a fallback.
  for(const n of nodes.values())if(n.type!=='chat'&&![...links.values()].some(e=>e.source===n.id||e.target===n.id)){const row=source.get(n.items[0]);if(row)edge(roots.get(row.chat_id),n.id,'belongs','聊天室中的內容');}
  return {nodes:[...nodes.values()],edges:[...links.values()],totalItems:totalRows,limited:totalRows!==rows.length||chosen.length<rows.length||nodes.size>=max,source};
 }
 return {build,label,terms};
})();

"use strict";
window.VaultGraphView = (svg,choose) => {
 const ns='http://www.w3.org/2000/svg',motion=window.matchMedia('(prefers-reduced-motion: reduce)');
 let model={nodes:[],edges:[]},elements=new Map(),byId=new Map(),positions=new Map(),selected=null,focused=null,zoom=1,pan={x:0,y:0},drag=null,frame=0,panDrag=null,suppressClick=false;
 const make=(tag,attrs={})=>{const x=document.createElementNS(ns,tag);for(const[k,v]of Object.entries(attrs))x.setAttribute(k,String(v));return x;};
 const layer=make('g'),edgeLayer=make('g',{'class':'graph-edges'}),nodeLayer=make('g');layer.append(edgeLayer,nodeLayer);svg.append(layer);
 const colors={chat:'#38685a',topic:'#a97a3d',person:'#737fa9',source:'#638a75',file:'#a66d7c'},radii={chat:25,topic:16,person:13,source:9,file:11};
 const transform=()=>{layer.setAttribute('transform',`translate(${500+pan.x} ${320+pan.y}) scale(${zoom}) translate(-500 -320)`);const level=document.getElementById('zoom-level');if(level)level.textContent=Math.round(zoom*100)+'%';};
 function paint(){for(const e of model.edges){const a=byId.get(e.source),b=byId.get(e.target),line=elements.get(e.id);if(!a||!b)continue;line.setAttribute('x1',a.x);line.setAttribute('y1',a.y);line.setAttribute('x2',b.x);line.setAttribute('y2',b.y);}for(const n of model.nodes){elements.get(n.id).setAttribute('transform',`translate(${n.x} ${n.y})`);positions.set(n.id,{x:n.x,y:n.y});}layoutLabels();}

 // Resolve labels in graph coordinates so zooming and panning preserve decisions.
 // Room names and the active/focused node win; hidden text remains available via
 // the node's accessible name, native SVG title and the source inspector.
 const measure=document.createElement('canvas').getContext('2d');
 function labelMetrics(n){const text=elements.get(n.id)?.querySelector('text');if(!text)return;const mobile=window.innerWidth<820,size=n.type==='chat'?(mobile?24:17):(mobile?22:15);measure.font='650 '+size+'px system-ui, "Microsoft JhengHei", sans-serif';n.labelWidth=measure.measureText(text.textContent).width+8;n.labelHeight=size*1.35;n.labelSize=size;}
 function layoutLabels(){
  const occupied=[],priority={chat:1,topic:2,person:3,file:4,source:5},ordered=[...model.nodes].sort((a,b)=>{const rank=n=>n.id===focused?-2:n.id===selected?-1:priority[n.type]??6;return rank(a)-rank(b)||a.id.localeCompare(b.id);});
  const overlaps=(a,b)=>a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;
  for(const n of ordered){const text=elements.get(n.id)?.querySelector('text');if(!text)continue;if(!n.labelWidth)labelMetrics(n);const w=n.labelWidth,h=n.labelHeight,r=radii[n.type],gap=9,size=n.labelSize;
   const candidates=[{x:0,y:r+gap+size,anchor:'middle'},{x:0,y:-r-gap,anchor:'middle'},{x:r+gap,y:size*.35,anchor:'start'},{x:-r-gap,y:size*.35,anchor:'end'}];
   let chosen=null;for(const c of candidates){const x=n.x+c.x-(c.anchor==='middle'?w/2:c.anchor==='end'?w:0),y=n.y+c.y-size*1.05,box={x:x-3,y:y-3,w:w+6,h:h+6};
    if(box.x<12||box.x+box.w>988||box.y<10||box.y+box.h>620||occupied.some(b=>overlaps(box,b)))continue;
    if(model.nodes.some(other=>other!==n&&overlaps(box,{x:other.x-radii[other.type]-3,y:other.y-radii[other.type]-3,w:radii[other.type]*2+6,h:radii[other.type]*2+6})))continue;
    chosen={...c,box};break;
   }
   // A selected/focused label is always readable: priority means it is placed
   // before other labels; allowing marker overlap is preferable to hiding it.
   if(!chosen&&(n.id===selected||n.id===focused||n.type==='chat'))for(const c of candidates){const box={x:n.x+c.x-(c.anchor==='middle'?w/2:c.anchor==='end'?w:0)-3,y:n.y+c.y-size*1.05-3,w:w+6,h:h+6};if(!occupied.some(b=>overlaps(box,b))){chosen={...c,box};break;}}
   if(chosen){text.setAttribute('x',chosen.x);text.setAttribute('y',chosen.y);text.setAttribute('text-anchor',chosen.anchor);text.setAttribute('visibility','visible');text.setAttribute('data-label-visible','true');occupied.push(chosen.box);}else{text.setAttribute('visibility','hidden');text.setAttribute('data-label-visible','false');}
  }
 }
 function highlight(id){selected=id;const near=new Set([id]),chosen=byId.get(id);if(chosen?.type==='chat'){const items=new Set(chosen.items);for(const n of model.nodes)if(n.items.some(x=>items.has(x)))near.add(n.id);}for(const e of model.edges)if(e.source===id||e.target===id){near.add(e.source);near.add(e.target);}for(const n of model.nodes){const item=elements.get(n.id);item.classList.toggle('selected',n.id===id);item.classList.toggle('muted',!!id&&!near.has(n.id));item.setAttribute('aria-pressed',String(n.id===id));}for(const e of model.edges){const line=elements.get(e.id);line.classList.toggle('active',e.source===id||e.target===id);line.classList.toggle('muted',!!id&&e.source!==id&&e.target!==id);}const reset=document.getElementById('clear-map-selection');if(reset)reset.hidden=!id;layoutLabels();}
 function step(){const nodes=model.nodes;for(let k=0;k<3;k++){
  for(const n of nodes){n.fx=(500-n.x)*.002;n.fy=(320-n.y)*.002;}
  for(let i=0;i<nodes.length;i++)for(let j=i+1;j<nodes.length;j++){const a=nodes[i],b=nodes[j];let dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy)||.1;const f=Math.min(7,4200/(d*d));a.fx-=dx/d*f;a.fy-=dy/d*f;b.fx+=dx/d*f;b.fy+=dy/d*f;}
  for(const e of model.edges){const a=byId.get(e.source),b=byId.get(e.target);if(!a||!b)continue;const dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy)||.1,w=e.type==='belongs'?.010:.005,f=(d-(e.type==='belongs'?125:145))*w;a.fx+=dx/d*f;a.fy+=dy/d*f;b.fx-=dx/d*f;b.fy-=dy/d*f;}
  for(const n of nodes){if(drag?.node===n)continue;n.x=Math.max(72,Math.min(925,n.x+n.fx));n.y=Math.max(60,Math.min(570,n.y+n.fy));}
 }}
 function settle(steps){cancelAnimationFrame(frame);if(motion.matches){for(let i=0;i<steps;i++)step();paint();return;}let turn=0;const tick=()=>{step();paint();if(++turn<steps)frame=requestAnimationFrame(tick);};tick();}
 function point(event){const p=svg.createSVGPoint();p.x=event.clientX;p.y=event.clientY;return p.matrixTransform(layer.getScreenCTM().inverse());}
 function focusNode(id){for(const n of model.nodes)elements.get(n.id).setAttribute('tabindex',String(n.id===id?0:-1));elements.get(id)?.focus();}
 function navigate(n,event){const directions={ArrowRight:[1,0],ArrowLeft:[-1,0],ArrowDown:[0,1],ArrowUp:[0,-1]},direction=directions[event.key];if(event.key==='Home'||event.key==='End'){event.preventDefault();focusNode(model.nodes[event.key==='Home'?0:model.nodes.length-1]?.id);return;}if(!direction)return;event.preventDefault();let target=null,best=Infinity;for(const other of model.nodes){if(other===n)continue;const dx=other.x-n.x,dy=other.y-n.y,forward=dx*direction[0]+dy*direction[1];if(forward<=0)continue;const lateral=Math.abs(dx*direction[1]-dy*direction[0]),score=Math.hypot(dx,dy)+lateral*2;if(score<best){best=score;target=other;}}if(target)focusNode(target.id);}
 function set(value){cancelAnimationFrame(frame);const mobile=window.innerWidth<820;svg.setAttribute('viewBox',mobile?'150 20 700 600':'0 0 1000 640');const previousFocus=document.activeElement?.getAttribute('data-node-id');model=value;byId=new Map(model.nodes.map(n=>[n.id,n]));elements=new Map();edgeLayer.replaceChildren();nodeLayer.replaceChildren();let index=0,added=0;
  const count=Math.max(1,model.nodes.length);for(const n of model.nodes){const previous=positions.get(n.id),angle=index++*2.3999632297,rad=n.type==='chat'?90:130+Math.sqrt(index/count)*135;n.x=previous?.x??500+Math.cos(angle)*rad;n.y=previous?.y??315+Math.sin(angle)*rad;if(!previous)added++;}
  for(const e of model.edges){const line=make('line',{'class':'graph-edge '+e.type});const title=make('title');title.textContent=e.label;line.append(title);edgeLayer.append(line);elements.set(e.id,line);}
  for(const n of model.nodes){const g=make('g',{'class':'graph-node '+n.type,role:'button',tabindex:n===model.nodes[0]?0:-1,'aria-label':n.title,'data-node-id':n.id,'data-node-type':n.type}),hit=make('circle',{r:Math.max(22,radii[n.type]+9),fill:'transparent','class':'node-hit'}),halo=make('circle',{r:radii[n.type]+7,'class':'node-halo'}),dot=make('circle',{r:radii[n.type],fill:colors[n.type]}),text=make('text',{y:radii[n.type]+22,'text-anchor':'middle'}),maxLabel=mobile?8:12;text.textContent=n.title.length>maxLabel?n.title.slice(0,maxLabel)+'…':n.title;const title=make('title');title.textContent=n.title;g.append(hit,halo,dot,text,title);
   g.addEventListener('focus',()=>{focused=n.id;layoutLabels();});g.addEventListener('blur',()=>{if(focused===n.id)focused=null;layoutLabels();});
   g.addEventListener('click',e=>{e.stopPropagation();if(suppressClick){suppressClick=false;return;}focusNode(n.id);highlight(n.id);choose(n);});
   g.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();highlight(n.id);choose(n);}else if(e.key==='Escape'){e.preventDefault();highlight(null);choose(null);}else navigate(n,e);});
   g.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.stopPropagation();g.setPointerCapture(e.pointerId);const p=point(e);drag={node:n,offsetX:n.x-p.x,offsetY:n.y-p.y,startX:e.clientX,startY:e.clientY};suppressClick=false;});
   g.addEventListener('pointermove',e=>{if(drag?.node!==n)return;if(Math.hypot(e.clientX-drag.startX,e.clientY-drag.startY)>5)suppressClick=true;const p=point(e);n.x=p.x+drag.offsetX;n.y=p.y+drag.offsetY;paint();});g.addEventListener('pointerup',()=>{drag=null;});g.addEventListener('pointercancel',()=>{drag=null;suppressClick=false;});nodeLayer.append(g);elements.set(n.id,g);labelMetrics(n);
  }transform();if(added)settle(positions.size?18:55);else paint();highlight(byId.has(selected)?selected:null);if(previousFocus&&byId.has(previousFocus))focusNode(previousFocus);
  // Bound retained positions when browsing large collections.
  if(positions.size>1500)positions=new Map(model.nodes.map(n=>[n.id,{x:n.x,y:n.y}]));
 }
 svg.addEventListener('pointerdown',e=>{if(e.target.closest('.graph-node')||e.button!==0)return;panDrag={x:e.clientX,y:e.clientY,px:pan.x,py:pan.y};svg.setPointerCapture(e.pointerId);});svg.addEventListener('pointermove',e=>{if(!panDrag)return;const rect=svg.getBoundingClientRect();pan.x=panDrag.px+(e.clientX-panDrag.x)*svg.viewBox.baseVal.width/rect.width;pan.y=panDrag.py+(e.clientY-panDrag.y)*svg.viewBox.baseVal.height/rect.height;transform();});svg.addEventListener('pointerup',e=>{if(panDrag&&Math.hypot(e.clientX-panDrag.x,e.clientY-panDrag.y)<5){highlight(null);choose(null);}panDrag=null;});svg.addEventListener('pointercancel',()=>{panDrag=null;drag=null;});
 function resized(){const mobile=window.innerWidth<820;svg.setAttribute('viewBox',mobile?'150 20 700 600':'0 0 1000 640');for(const n of model.nodes){const text=elements.get(n.id)?.querySelector('text'),limit=mobile?8:12;if(text)text.textContent=n.title.length>limit?n.title.slice(0,limit)+'…':n.title;labelMetrics(n);}layoutLabels();}window.addEventListener('resize',resized);
 function reduceChanged(){if(motion.matches){cancelAnimationFrame(frame);for(let i=0;i<30;i++)step();paint();}}motion.addEventListener('change',reduceChanged);
 return {set,select:highlight,zoom:amount=>{zoom=Math.max(.45,Math.min(2.5,zoom*amount));transform();},center:()=>{zoom=1;pan={x:0,y:0};transform();},destroy:()=>{cancelAnimationFrame(frame);window.removeEventListener('resize',resized);motion.removeEventListener('change',reduceChanged);}};
};

"use strict";
window.VaultDownloads = (catalog, notify) => {
 const waiting=new Map();let busy=false,aborted=false,urls=[];
 const hex=bytes=>Array.from(new Uint8Array(bytes),b=>b.toString(16).padStart(2,"0")).join("");
 const stop=()=>{if(aborted)throw new DOMException("Cancelled","AbortError");};
 window.VaultBytes={add:(sha,index,value)=>{const item=waiting.get(sha+":"+index);if(item)item.receive(value);}};
 async function part(sha,index,entry){
  stop();if(!/^downloads\/[a-f0-9]{64}\/\d+\.js$/.test(entry.path)||entry.path!==`downloads/${sha}/${index}.js`)throw Error("Invalid path");
  const encoded=await new Promise((resolve,reject)=>{
   const key=sha+":"+index,script=document.createElement("script");let received=false;
   const timer=setTimeout(()=>finish(Error("Missing part")),15000);
   const finish=error=>{clearTimeout(timer);waiting.delete(key);script.remove();if(error)reject(error);};
   waiting.set(key,{receive:value=>{if(received||typeof value!=="string"||value.length>1048576||!/^[A-Za-z0-9+/]*={0,2}$/.test(value)){finish(Error("Invalid part"));return;}received=true;resolve(value);}});
   script.src=entry.path;script.onload=()=>finish(received?null:Error("Missing data"));script.onerror=()=>finish(Error("Missing file"));document.head.appendChild(script);
  });
  stop();const raw=atob(encoded),bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));
  if(bytes.length!==entry.size||!crypto.subtle||hex(await crypto.subtle.digest("SHA-256",bytes))!==entry.sha256)throw Error("Integrity check failed");
  return bytes;
 }
 async function saveFile(attachment){
  const entry=catalog[attachment.download];
  if(!entry||entry.sha256!==attachment.download||!Array.isArray(entry.parts)||!entry.parts.length)throw Error("No download data");
  let output=null;const chunks=[];let size=0;
  if(entry.size>64*1024*1024){
   if(!window.showSaveFilePicker){const error=Error();error.browserRequired=true;throw error;}
   const handle=await window.showSaveFilePicker({suggestedName:attachment.name});
   output=await handle.createWritable();
  }
  try{
   for(let n=0;n<entry.parts.length;n++){
    stop();notify(`正在準備 ${attachment.name}（${n+1} / ${entry.parts.length}）`);
    const bytes=await part(entry.sha256,n,entry.parts[n]);size+=bytes.length;
    if(output)await output.write(bytes);else chunks.push(bytes);
   }
   stop();if(size!==entry.size)throw Error("Wrong size");
   if(output){await output.close();output=null;}
   else{
    const blob=new Blob(chunks,{type:"application/octet-stream"});
    if(hex(await crypto.subtle.digest("SHA-256",await blob.arrayBuffer()))!==entry.sha256)throw Error("Wrong checksum");
    const url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download=attachment.name;
    document.body.appendChild(link);link.click();link.remove();urls.push(url);
    while(urls.length>4)URL.revokeObjectURL(urls.shift());
    setTimeout(()=>{URL.revokeObjectURL(url);urls=urls.filter(x=>x!==url);},60000);
   }
  }catch(error){if(output)await output.abort().catch(()=>{});throw error;}
 }
 return {
  busy:()=>busy,
  cancel:()=>{aborted=true;notify("正在停止另存，已完成的檔案仍會保留。");},
  save:async attachments=>{
   if(busy){notify("正在另存檔案，請等目前操作完成。");return;}
   if(attachments.length>1&&attachments.some(a=>catalog[a.download]?.size>64*1024*1024)){notify("選取內容含大檔案，請分別按各檔案的「另存檔案」，或用桌面程式製作 ZIP。");return;}
   busy=true;aborted=false;let count=0;
   try{for(const a of attachments){stop();await saveFile(a);count++;}notify(`已準備 ${count} 個檔案，請查看瀏覽器的下載項目或選定位置。`);}
   catch(error){notify(error.name==="AbortError"?"另存已取消，已完成的檔案仍會保留。":error.browserRequired?"這份檔案較大，請用新版 Edge 或 Chrome 開啟此頁後另存；原檔仍在附件資料夾。":"無法完成另存，請確認整個封存資料夾都在，並在桌面程式執行檔案檢查。");}
   finally{busy=false;}
  }
 };
};

"use strict";
(() => {
 const manifest=JSON.parse(document.getElementById('manifest').textContent),$=id=>document.getElementById(id);
 const data=new Map(),loading=new Map(),selected=new Map(),chatTitles=new Map(manifest.chats.map(c=>[c.id,c.title]));let chat=manifest.chats[0]?.id||null,page=0,filePage=0,version=0,current=[],filtered=[],view='map',focusNode=null,mapModel=null;
 const kinds={message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'},types={chat:'聊天室',topic:'關鍵字',person:'人物',source:'內容',file:'附件'};
 const states={saved:'已保存',preview:'只有預覽圖',pending:'尚未保存',failed:'暫時無法取得',unavailable:'來源已無法提供'};
 const el=(tag,text,cls)=>{const n=document.createElement(tag);if(text!==undefined)n.textContent=text;if(cls)n.className=cls;return n;};
 const localPath=p=>typeof p==='string'&&/^assets\/[a-f0-9]{2}\/[a-f0-9]{64}\.(png|jpg|gif|webp|wav|ogg|mp3|mp4|bin)$/.test(p)?p:null;
 const date=t=>t?new Date(t).toLocaleString('zh-TW',{timeZone:'Asia/Taipei',year:'numeric',month:'numeric',day:'numeric',hour:'2-digit',minute:'2-digit'}):'';
 const size=n=>n==null?'':n>=1048576?(n/1048576).toFixed(1)+' MB':n>=1024?(n/1024).toFixed(1)+' KB':n+' B';
 const downloads=window.VaultDownloads(manifest.downloads||{},message=>{$('download-notice').textContent=message;$('cancel-download').hidden=!downloads.busy();});
 const graph=window.VaultGraphView($('knowledge-map'),showNode);
 if(manifest.artificial_data){$('sample-label').hidden=false;document.title='聊藏 · 人工範例';}
 window.VaultData={add:(id,rows)=>{if(!data.has(id))data.set(id,[]);data.get(id).push(...rows);}};
 function load(id){if(loading.has(id))return loading.get(id);const c=manifest.chats.find(c=>c.id===id);data.set(id,[]);const job=(async()=>{for(const src of c.chunks){if(!/^reader\/[a-f0-9]{32}\/[a-f0-9]{64}-\d+\.js$/.test(src))throw Error();await new Promise((ok,fail)=>{const s=el('script');s.src=src;s.onload=ok;s.onerror=fail;document.head.appendChild(s);});}return data.get(id);})();loading.set(id,job);return job;}
 function setView(next){view=next;for(const [id,section,key]of [['view-map','map-section','map'],['view-reading','reading-section','reading'],['view-files','files-section','files']]){$(id).setAttribute('aria-selected',String(key===view));$(id).tabIndex=key===view?0:-1;$(section).hidden=key!==view;}if(view==='reading')drawReading();if(view==='files')drawFiles();}
 function nav(){const n=$('chats');n.replaceChildren();$('chat-count').textContent=manifest.chats.length;for(const c of manifest.chats){const b=el('button');b.append(el('span',c.title,'chat-label'),el('span',c.count+' 則收藏','chat-meta'));b.className=c.id===chat&&$('scope').value!=='all'?'active':'';b.setAttribute('aria-pressed',String(c.id===chat));b.onclick=()=>{chat=c.id;$('scope').value='chat';focusNode=null;page=0;filePage=0;nav();render();};n.append(b);}$('all-chats').classList.toggle('active',$('scope').value==='all');}
 function canPreview(a){const p=localPath(a.path);return p&&((['image','sticker'].includes(a.kind)&&/\.(png|jpg|gif|webp)$/.test(p))||(a.kind==='video'&&p.endsWith('.mp4'))||(a.kind==='audio'&&/\.(wav|ogg|mp3)$/.test(p)));}
 function preview(a){const box=$('preview-content');box.replaceChildren();const p=localPath(a.path);if(!canPreview(a))return;let media;if(['image','sticker'].includes(a.kind)){media=el('img');media.alt=a.name;}else{media=el(a.kind==='video'?'video':'audio');media.controls=true;}media.src=p;media.onerror=()=>box.replaceChildren(el('p','無法預覽這個檔案，仍可另存。'));box.append(media);$('preview').showModal();}
 function counts(){$('selected-count').textContent=selected.size?'已選 '+selected.size+' 項':'';$('save-selected').disabled=!selected.size;}
 async function save(a){await downloads.save([a]);$('cancel-download').hidden=true;}
 function checkbox(a){const cb=el('input');cb.type='checkbox';cb.checked=selected.has(a.id);cb.setAttribute('aria-label','選取 '+a.name);cb.onchange=()=>{if(cb.checked)selected.set(a.id,a);else selected.delete(a.id);counts();};return cb;}
 function attachment(a){const row=el('div',undefined,'attachment'),p=localPath(a.path);if(p&&['image','sticker'].includes(a.kind)&&/\.(png|jpg|gif|webp)$/.test(p)){const img=el('img');img.src=p;img.alt=a.name;img.loading='lazy';img.onerror=()=>img.remove();row.append(img);}const info=el('div',undefined,'file-info');info.append(el('div',a.name,'filename'),el('div',(a.saved_before?'先前保存 · ':'')+(states[a.status]||'待確認')+(a.size!=null?' · '+size(a.size):''),'status'+(a.status==='saved'?'':' warning')));row.append(info);if(p){if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);row.append(b);}const b=el('button','另存檔案','save-file');b.onclick=()=>save(a);row.append(b);}return row;}
 function meta(i){const line=el('div',undefined,'meta');line.append(el('span',i.sender_name||kinds[i.kind],'name'),el('span',kinds[i.kind],'type'));if(i.sent_at)line.append(el('time',date(i.sent_at)));if($('scope').value==='all'&&chatTitles.has(i.chat_id))line.append(el('span',chatTitles.get(i.chat_id),'origin'));return line;}
 function card(i){const c=el('article',undefined,'card');c.id='item-'+i.id;c.append(meta(i));if(i.title)c.append(el('h3',i.title));if(i.parent_id){const p=current.find(r=>r.id===i.parent_id);if(p){const b=el('button','查看所屬'+(kinds[p.kind]||'內容'),'secondary');b.onclick=()=>jump(p.id);c.append(b);}else c.append(el('p','所屬內容尚未收錄。','context'));}if(i.reply_source_id){const r=current.find(r=>r.chat_id===i.chat_id&&r.source_id===i.reply_source_id);if(r){const b=el('button','查看引用訊息','secondary');b.onclick=()=>jump(r.id);c.append(b);}else c.append(el('p','原訊息尚未收錄。','context'));}c.append(el('p',i.state==='recalled'?'此訊息已收回':i.state==='deleted'?'此內容已刪除':i.text,'text'));for(const a of i.attachments)c.append(attachment(a));return c;}
 function showNode(node,overview=false){if(!node){const first=mapModel?.nodes.find(n=>n.type==='chat');if(first)return showNode(first,true);graph.select(null);return;}graph.select(overview?null:node.id);const panel=$('node-detail');panel.classList.remove('revealing');panel.replaceChildren();requestAnimationFrame(()=>panel.classList.add('revealing'));panel.append(el('span',types[node.type]||'內容','detail-type'),el('h3',node.title));const subtitle=node.type==='topic'?(node.explicit?'這個標籤出現的對話與檔案。':'對話中出現這個關鍵字的地方。'):node.type==='person'?(node.identified?'來自這位成員的內容。':'以這個暱稱顯示的對話。'):node.type==='file'?'檔案保留在這份收藏中。':'沿著原文，看看對話如何連在一起。';panel.append(el('p',subtitle,'detail-subtitle'));
  if(node.attachment)panel.append(attachment(node.attachment));const related=node.items.map(id=>current.find(i=>i.id===id)).filter(Boolean);panel.append(el('div',related.length+' 則相關內容','detail-count'));
  for(const i of related.slice(0,3)){const entry=el('div',undefined,'detail-entry');entry.append(meta(i));if(i.title)entry.append(el('h4',i.title));entry.append(el('p',i.state==='recalled'?'此訊息已收回':i.state==='deleted'?'此內容已刪除':i.text,'text'));const b=el('button','閱讀原文 ↗','detail-open');b.onclick=()=>jump(i.id);entry.append(b);panel.append(entry);}
  if(related.length){const b=el('button','閱讀全部相關內容','secondary');b.onclick=()=>{focusNode={title:node.title,ids:new Set(node.items)};page=0;setView('reading');};panel.append(b);}
 }
 function drawReading(){const rows=focusNode?filtered.filter(i=>focusNode.ids.has(i.id)):filtered;const pages=Math.max(1,Math.ceil(rows.length/60));page=Math.max(0,Math.min(page,pages-1));$('reading-label').textContent=focusNode?'關於「'+focusNode.title+'」':'全部內容';$('clear-node').hidden=!focusNode;const box=$('items');box.replaceChildren();for(const i of rows.slice(page*60,(page+1)*60))box.append(card(i));if(!rows.length)box.append(el('div','沒有符合條件的內容。','empty'));$('page').textContent=(page+1)+' / '+pages;$('previous').disabled=page===0;$('next').disabled=page+1>=pages;}
 function drawFiles(){const box=$('file-grid');box.replaceChildren();const files=filtered.flatMap(i=>i.attachments.map(a=>({i,a})));const pages=Math.max(1,Math.ceil(files.length/36));filePage=Math.max(0,Math.min(filePage,pages-1));for(const {i,a}of files.slice(filePage*36,(filePage+1)*36)){const c=el('article',undefined,'file-card'),cover=el('div',undefined,'file-cover'),p=localPath(a.path);if(p&&['image','sticker'].includes(a.kind)&&canPreview(a)){const img=el('img');img.src=p;img.alt=a.name;img.loading='lazy';cover.append(img);cover.onclick=()=>preview(a);}else cover.append(el('span',a.kind==='video'?'▷':a.kind==='audio'?'♫':'▤'));const body=el('div',undefined,'file-card-body');body.append(el('h3',a.name),el('p',i.title||i.sender_name||'聊天室附件'),el('p',(states[a.status]||'待確認')+' · '+size(a.size)));const actions=el('div',undefined,'file-card-actions');if(p){actions.append(checkbox(a));if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);actions.append(b);}const b=el('button','另存檔案','save-file');b.onclick=()=>save(a);actions.append(b);}body.append(actions);c.append(cover,body);box.append(c);}if(!files.length)box.append(el('div','這個範圍還沒有圖片或檔案。','empty'));$('file-page').textContent=(filePage+1)+' / '+pages;$('file-previous').disabled=filePage===0;$('file-next').disabled=filePage+1>=pages;counts();}
 function isKind(i,kind){if(kind==='all')return true;if(kind==='files')return i.attachments.length>0;if(kind==='issues')return i.attachments.some(a=>a.status!=='saved');let p=i,seen=new Set();while(p&&!seen.has(p.id)){seen.add(p.id);if(p.kind===kind)return true;p=current.find(x=>x.id===p.parent_id);}return false;}
 async function render(){const turn=++version;$('notice').textContent='';$('content').setAttribute('aria-busy','true');$('filter-summary').textContent='正在整理內容…';try{const ids=$('scope').value==='all'?manifest.chats.map(c=>c.id):chat?[chat]:[];await Promise.all(ids.map(load));if(turn!==version)return;current=ids.flatMap(id=>data.get(id)||[]);const q=$('search').value.trim().toLocaleLowerCase(),sender=$('sender').value.trim().toLocaleLowerCase(),kind=$('kind').value;filtered=current.filter(i=>{const text=[i.text,i.title,i.sender_name,...i.attachments.map(a=>a.name)].join(' ').toLocaleLowerCase(),day=i.sent_at?new Date(i.sent_at).toLocaleDateString('en-CA',{timeZone:'Asia/Taipei'}):'';return (!q||text.includes(q))&&(!sender||i.sender_name.toLocaleLowerCase().includes(sender))&&isKind(i,kind)&&(!$('since').value||day&&day>=$('since').value)&&(!$('until').value||day&&day<=$('until').value);});const c=manifest.chats.find(c=>c.id===chat),all=$('scope').value==='all';$('title').textContent=all?'全部收藏':c?.title||'知識地圖';$('coverage').textContent=all?'跨聊天室找人、找事，也能回到原文。':'從人物、關鍵字或檔案，回到那段對話。';$('stats').textContent=filtered.length+' 則內容 · '+filtered.reduce((n,i)=>n+i.attachments.length,0)+' 份附件';const chats=manifest.chats.filter(c=>ids.includes(c.id));mapModel=window.VaultGraphModel.build(filtered,chats);graph.set(mapModel);$('map-size').textContent=mapModel.limited?'搜尋以探索更多內容':'';$('map-empty').hidden=filtered.length>0;const first=mapModel.nodes.find(n=>n.type==='chat');if(first){showNode(first,true);}else $('node-detail').replaceChildren(el('p','清除篩選，或選擇其他聊天室。','detail-subtitle'));drawReading();drawFiles();nav();if(manifest.problems?.some(p=>p.code==='file'))$('notice').textContent='部分檔案尚未齊全，可稍後補存。';}catch{if(turn===version)$('notice').textContent='部分內容讀不到，請確認整份收藏資料夾都在。';}finally{if(turn===version){$('content').setAttribute('aria-busy','false');updateFilterSummary();}}}
 async function jump(id){const target=current.find(i=>i.id===id);if(!target)return;for(const key of ['search','sender','since','until'])$(key).value='';$('kind').value='all';focusNode=null;await render();page=Math.floor(filtered.findIndex(i=>i.id===id)/60);setView('reading');const n=document.getElementById('item-'+id);n?.setAttribute('tabindex','-1');n?.scrollIntoView({block:'center'});n?.focus({preventScroll:true});}

 function updateFilterSummary(){const count=['search','sender','since','until'].filter(id=>$(id).value.trim()).length+Number($('kind').value!=='all');$('filter-count').textContent=count; $('filter-count').hidden=!count;$('reset-filters').hidden=!count;const scope=$('scope').value==='all'?'全部聊天室':'目前聊天室';$('filter-summary').textContent=count?scope+' · '+count+' 項篩選 · '+filtered.length+' 則結果':scope+' · 全部內容';}
 function clearFilters(){for(const id of ['search','sender','since','until'])$(id).value='';$('kind').value='all';focusNode=null;page=0;filePage=0;render();}
 const tabs=[$('view-map'),$('view-reading'),$('view-files')],views=['map','reading','files'];for(const [index,tab]of tabs.entries())tab.addEventListener('keydown',event=>{let next=index;if(event.key==='ArrowRight')next=(index+1)%tabs.length;else if(event.key==='ArrowLeft')next=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')next=0;else if(event.key==='End')next=tabs.length-1;else return;event.preventDefault();setView(views[next]);tabs[next].focus();});
 document.addEventListener('keydown',event=>{const editing=/INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName||'');if(event.key==='/'&&!editing&&!event.ctrlKey&&!event.metaKey&&!event.altKey){event.preventDefault();$('search').focus();}if(event.key==='Escape'){const open=document.querySelector('details[open]');if(open){open.open=false;open.querySelector('summary')?.focus();}}});
 document.addEventListener('click',event=>{for(const details of document.querySelectorAll('details[open]'))if(!details.contains(event.target))details.open=false;});
 $('reset-filters').onclick=clearFilters;$('clear-map-selection').onclick=()=>{showNode(null);$('knowledge-map').querySelector('[tabindex="0"]')?.focus();};
 $('preview').addEventListener('close',()=>{$('preview-content').replaceChildren();});
 for(const [id,key]of [['view-map','map'],['view-reading','reading'],['view-files','files']])$(id).onclick=()=>setView(key);
 for(const id of ['search','sender','since','until','kind','scope'])$(id).addEventListener('input',()=>{page=0;filePage=0;focusNode=null;render();});
 $('all-chats').onclick=()=>{$('scope').value='all';focusNode=null;page=0;render();};$('clear').onclick=clearFilters;$('clear-node').onclick=()=>{focusNode=null;page=0;drawReading();};$('previous').onclick=()=>{page--;drawReading();};$('next').onclick=()=>{page++;drawReading();};$('file-previous').onclick=()=>{filePage--;drawFiles();};$('file-next').onclick=()=>{filePage++;drawFiles();};$('zoom-in').onclick=()=>graph.zoom(1.2);$('zoom-out').onclick=()=>graph.zoom(1/1.2);$('map-center').onclick=()=>graph.center();$('close-preview').onclick=()=>{$('preview').close();$('preview-content').replaceChildren();};$('cancel-download').onclick=()=>downloads.cancel();$('save-selected').onclick=async()=>{await downloads.save([...selected.values()]);$('cancel-download').hidden=true;};nav();render();
})();
)ATLAS";
const char*worker_script=R"ATLAS($ErrorActionPreference = 'Stop'
# Preserve the bootstrap's buffered stdin reader if it already uses UTF-8.
if ([Console]::InputEncoding.CodePage -ne 65001) { [Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false) }
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VaultNative {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int command);
}
'@
$script:ProcessId = 0
$script:ProcessStarted = 0
$script:ProcessPath = $null
$script:SaveDialogNames = @('Save As','Save as','另存新檔','另存為','另存为','名前を付けて保存')
$script:Main = $null
$script:Cache = @{}
$script:Stage = $null
$script:AllowedNames = @('下載','儲存','另存新檔','另存','Download','Save','Save as','Save As','保存','ダウンロード','保存する')

function Node-Key($e) { return (($e.GetRuntimeId() | ForEach-Object { [string]$_ }) -join '.') }
function Node-Object($e) {
 $c=$e.Current
 $id=Node-Key $e
 $script:Cache[$id]=$e
 return @{ token=$id; name=$c.Name; automation_id=$c.AutomationId;
  control_type=($c.ControlType.ProgrammaticName -replace '^ControlType\.','');
  class_name=$c.ClassName; enabled=$c.IsEnabled; offscreen=$c.IsOffscreen }
}
function Assert-Window {
 if ($null -eq $script:Main) { throw 'not_connected' }
 $p=Get-Process -Id $script:ProcessId
 if ($p.ProcessName -cne 'LINE' -or $p.StartTime.ToUniversalTime().Ticks -ne $script:ProcessStarted -or $p.Path -ine $script:ProcessPath) { throw 'wrong_process' }
 if ($script:Main.Current.ProcessId -ne $script:ProcessId) { throw 'wrong_window' }
}
function Assert-Foreground {
 Assert-Window
 [uint32]$pid0=0
 [void][VaultNative]::GetWindowThreadProcessId([VaultNative]::GetForegroundWindow(),[ref]$pid0)
 if ($pid0 -ne $script:ProcessId) { throw 'focus_changed' }
}
function Assert-SaveDialog($dialog) {
 Assert-Window
 $c=$dialog.Current
 if ($c.ProcessId -ne $script:ProcessId -or $c.ClassName -cne '#32770' -or $c.Name -cnotin $script:SaveDialogNames -or !$c.IsEnabled -or $c.IsOffscreen -or $c.NativeWindowHandle -eq 0) { throw 'save_dialog_unsupported' }
 if ([VaultNative]::GetForegroundWindow() -ne [IntPtr]$c.NativeWindowHandle) { throw 'focus_changed' }
}
function Assert-SaveDestination($valuePattern,$dest) {
 if ($valuePattern.Current.Value -cne $dest -or (Test-Path -LiteralPath $dest)) { throw 'invalid_destination' }
}
function Get-LineDialogs {
 $wins=[System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition)
 return @($wins | Where-Object { $_.Current.ProcessId -eq $script:ProcessId -and $_.Current.ClassName -eq '#32770' })
}
function Resolve-Node($token) {
 Assert-Window
 if (!$script:Cache.ContainsKey([string]$token)) { throw 'stale_node' }
 $e=$script:Cache[[string]$token]
 if ($e.Current.ProcessId -ne $script:ProcessId) { throw 'wrong_process' }
 return $e
}
function Match-Node($e,$selector) {
 $c=$e.Current
 if ($selector.automation_id -and $c.AutomationId -cne [string]$selector.automation_id) { return $false }
 if ($selector.name -and $c.Name -cne [string]$selector.name) { return $false }
 if ($selector.control_type -and ($c.ControlType.ProgrammaticName -replace '^ControlType\.','') -ne [string]$selector.control_type) { return $false }
 return $true
}
function Find-Nodes($root,$selector,$max=6000) {
 $queue=New-Object System.Collections.Queue
 $queue.Enqueue($root)
 $found=New-Object System.Collections.ArrayList
 $walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker
 $count=0
 while ($queue.Count -gt 0) {
  $e=$queue.Dequeue();$count++
  if ($count -gt $max) { throw 'tree_limit' }
  if (Match-Node $e $selector) { [void]$found.Add($e) }
  $child=$walker.GetFirstChild($e)
  while ($null -ne $child) { $queue.Enqueue($child);$child=$walker.GetNextSibling($child) }
 }
 return ,$found.ToArray()
}

while ($null -ne ($line=[Console]::ReadLine())) {
 try {
  if ($line.Length -gt 131072) { throw 'request_limit' }
  $r=ConvertFrom-Json $line
  $result=$null
  switch ($r.action) {
   'connect' {
    $processes=@(Get-Process -Name LINE -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 })
    if ($processes.Count -ne 1) { throw 'line_window_ambiguous' }
    $p=$processes[0]
    $signature=Get-AuthenticodeSignature -FilePath $p.Path
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(?:^|,\s*)O="?(?:LINE Corporation|LY Corporation|NAVER Corporation)"?(?:,|$)') { throw 'publisher_unverified' }
    $script:ProcessId=$p.Id
    $script:ProcessStarted=$p.StartTime.ToUniversalTime().Ticks
    $script:ProcessPath=$p.Path
    $script:Main=[System.Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
    if ($script:Main.Current.ClassName -eq '#32770') { throw 'wrong_window' }
    $result=@{version=$p.FileVersionInfo.FileVersion; root=(Node-Object $script:Main); publisher_verified=$true}
   }
   'activate' {
    Assert-Window
    $p=Get-Process -Id $script:ProcessId
    [void][VaultNative]::ShowWindowAsync($p.MainWindowHandle,9)
    [void][VaultNative]::SetForegroundWindow($p.MainWindowHandle)
    $deadline=[DateTime]::UtcNow.AddSeconds(3)
    do {
     [uint32]$pid0=0
     [void][VaultNative]::GetWindowThreadProcessId([VaultNative]::GetForegroundWindow(),[ref]$pid0)
     if ($pid0 -eq $script:ProcessId) { break }
     Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    Assert-Foreground
    $result=$true
   }
   'inspect' {
    Assert-Window
    # Return counts only: never titles, text, AutomationIds, paths or tokens.
    $nodes=Find-Nodes $script:Main @{}
    $types=@{};$patterns=@{};$ids=0
    foreach ($e in $nodes) {
     $type=$e.Current.ControlType.ProgrammaticName -replace '^ControlType\.',''
     if (!$types.ContainsKey($type)) {$types[$type]=0};$types[$type]++
     if ($e.Current.AutomationId) {$ids++}
     foreach ($p in $e.GetSupportedPatterns()) {
      $name=$p.ProgrammaticName -replace '^.*\.',''
      if (!$patterns.ContainsKey($name)) {$patterns[$name]=0};$patterns[$name]++
     }
    }
    $result=@{node_count=$nodes.Count;control_types=$types;patterns=$patterns;nodes_with_automation_id=$ids}
   }
   'find' {
    Assert-Window
    $root=if($r.root){ Resolve-Node $r.root }else{$script:Main}
    $nodes=Find-Nodes $root $r.selector
    $result=@($nodes | ForEach-Object { Node-Object $_ })
   }
   'text' {
    $e=Resolve-Node $r.token
    $pattern=$null
    if ($e.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern,[ref]$pattern)) {
     $result=$pattern.DocumentRange.GetText(262144)
    } elseif ($e.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern,[ref]$pattern)) {
     $result=$pattern.Current.Value
    } else { $result=$e.Current.Name }
   }
   'select' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern,[ref]$pattern)) { throw 'selection_unsupported' }
    $pattern.Select();$result=$true
   }
   'scroll_up' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern,[ref]$pattern)) { throw 'scroll_unsupported' }
    if (!$pattern.Current.VerticallyScrollable -or $pattern.Current.VerticalScrollPercent -eq 0) { $result=$false }
    else { $pattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,[System.Windows.Automation.ScrollAmount]::LargeDecrement);$result=$true }
   }
   'scroll_down' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern,[ref]$pattern)) { throw 'scroll_unsupported' }
    if (!$pattern.Current.VerticallyScrollable -or $pattern.Current.VerticalScrollPercent -eq 100) { $result=$false }
    else { $pattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,[System.Windows.Automation.ScrollAmount]::LargeIncrement);$result=$true }
   }
   'invoke_navigation' {
    Assert-Foreground
    $e=Resolve-Node $r.token
    if ($e.Current.Name -notin @('相簿','記事本','討論串','Albums','Notes','Threads','照片⋅影片','照片・影片','Photos & videos','聊天','Chats','返回','關閉','Back','Close','戻る','閉じる')) { throw 'navigation_not_allowed' }
    $pattern=$e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke();$result=$true
   }
   'download' {
    Assert-Foreground
    $e=Resolve-Node $r.token
    if ($e.Current.Name -notin $script:AllowedNames) { throw 'download_action_not_allowed' }
    $dest=[System.IO.Path]::GetFullPath([string]$r.destination)
    $parent=[System.IO.Path]::GetDirectoryName($dest)
    if (!(Test-Path -LiteralPath $parent -PathType Container) -or [System.IO.Path]::GetFileName($dest) -ne 'payload.bin' -or (Test-Path -LiteralPath $dest)) { throw 'invalid_destination' }
    if ((Get-Item -LiteralPath $parent).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'linked_destination' }
    # Refuse an already open dialog instead of mistaking it for this download.
    if (@(Get-LineDialogs).Count -ne 0) { throw 'save_dialog_unsupported' }
    Assert-Foreground
    $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    $deadline=[DateTime]::UtcNow.AddSeconds(10);$dialog=$null
    while ([DateTime]::UtcNow -lt $deadline -and $null -eq $dialog) {
     Assert-Window
     $saveWindows=@(Get-LineDialogs)
     if ($saveWindows.Count -gt 1) { throw 'save_dialog_unsupported' }
     if ($saveWindows.Count -eq 1) { $dialog=$saveWindows[0] }
     if ($null -eq $dialog) { Start-Sleep -Milliseconds 100 }
    }
    if ($null -eq $dialog) { throw 'save_dialog_missing' }
    Assert-SaveDialog $dialog
    $edits=Find-Nodes $dialog @{control_type='Edit';automation_id='1001'} 2000
    if ($edits.Count -ne 1) { throw 'save_dialog_unsupported' }
    if ($edits[0].Current.ProcessId -ne $script:ProcessId -or !$edits[0].Current.IsEnabled) { throw 'save_dialog_unsupported' }
    $valuePattern=$edits[0].GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    if ($valuePattern.Current.IsReadOnly) { throw 'save_dialog_unsupported' }
    $valuePattern.SetValue($dest)
    if ($edits[0].GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value -cne $dest) { throw 'invalid_destination' }
    $buttons=Find-Nodes $dialog @{control_type='Button'} 2000
    $save=@($buttons | Where-Object { $_.Current.AutomationId -eq '1' -and ($_.Current.Name -replace '&','') -match '^(Save|儲存|保存)(\([A-Za-z]\))?$' -and $_.Current.ProcessId -eq $script:ProcessId -and $_.Current.IsEnabled -and !$_.Current.IsOffscreen })
    if ($save.Count -ne 1) { throw 'save_button_unsupported' }
    $invoke=$save[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    # Revalidate this exact dialog and destination immediately before invoking.
    # UIA cannot make this sequence atomic against a malicious same-user process.
    Assert-SaveDialog $dialog
    Assert-SaveDestination $valuePattern $dest
    $invoke.Invoke()
    $result=$true
   }
   'close' { exit 0 }
   default { throw 'unknown_action' }
  }
  @{ok=$true;result=$result} | ConvertTo-Json -Depth 20 -Compress | ForEach-Object { [Console]::WriteLine($_) }
 } catch {
  # Never return exception text, which can contain private paths or UI contents.
  $allowed=@('not_connected','wrong_process','wrong_window','focus_changed','stale_node','tree_limit','request_limit','line_window_ambiguous','publisher_unverified','selection_unsupported','scroll_unsupported','navigation_not_allowed','download_action_not_allowed','invalid_destination','linked_destination','save_dialog_missing','save_dialog_unsupported','save_button_unsupported','unknown_action')
  $code=if($_.Exception.Message -in $allowed){$_.Exception.Message}else{'uia_operation_failed'}
  @{ok=$false;code=$code} | ConvertTo-Json -Compress | ForEach-Object { [Console]::WriteLine($_) }
 }
}
)ATLAS";
}
