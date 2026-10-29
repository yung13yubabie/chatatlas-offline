#include "core.hpp"
namespace atlas {
const char*web_template=R"ATLAS(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self' 'sha256-@@CSP_SCRIPT@@'; style-src 'sha256-@@CSP_STYLE@@'; img-src 'self' data:; media-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'">
<meta name="referrer" content="no-referrer"><title>聊藏 · 離線聊天記錄</title><style>@@STYLE@@</style></head>
<body><a class="skip" href="#content">跳到訊息內容</a>
<header class="app-header"><div class="wordmark"><svg viewBox="0 0 34 34" aria-hidden="true"><path d="M8 9L25 7L23 26L8 22Z"/><circle cx="8" cy="9" r="4"/><circle cx="25" cy="7" r="4"/><circle cx="23" cy="26" r="4"/><circle cx="8" cy="22" r="3"/></svg><div><h1 id="app-title">聊藏</h1><span>離線聊天記錄</span></div></div><div class="header-note"><span id="sample-label" hidden>人工範例</span><span class="offline"><i></i>本機離線閱讀</span><details class="collection-info"><summary aria-label="關於這份聊天記錄">使用說明</summary><div class="info-popover"><strong>訊息與原檔都留在本機</strong><p>本頁不需連線。請保留整份資料夾，圖片、影片與檔案才能持續開啟。</p><p>訊息紀錄依保存時間排序；日期篩選不會刪除已保存訊息。關聯地圖只協助探索，每個節點都能回到原文。</p></div></details></div></header>
<div class="app-shell"><aside class="sidebar"><div class="sidebar-heading"><span>聊天室</span><span id="chat-count"></span></div><button id="all-chats" class="all-chats">全部聊天室<span>總覽與關聯地圖</span></button><div class="chat-filters"><label for="chat-search">尋找聊天室</label><input id="chat-search" type="search" placeholder="名稱或對象" autocomplete="off"><label class="sr-only" for="chat-kind">聊天室類型</label><select id="chat-kind"><option value="all">全部類型</option><option value="direct">個人對話</option><option value="group">群組</option><option value="community">社群</option><option value="subchat">社群聊天室</option></select></div><div id="chat-results" class="sidebar-result" aria-live="polite"></div><nav id="chats" aria-label="聊天室清單"></nav><div class="chat-pagination"><button id="chat-previous" class="secondary" aria-label="上一頁聊天室">上一頁</button><span id="chat-page" aria-live="polite"></span><button id="chat-next" class="secondary" aria-label="下一頁聊天室">下一頁</button></div><p class="sidebar-bottom">每頁顯示 8 個聊天室。選取聊天室即可閱讀已保存的完整訊息。</p></aside>
<main id="content"><div class="page-heading"><div><div class="eyebrow" id="section-label">聊天記錄</div><h2 id="title">訊息紀錄</h2><p id="coverage"></p></div><div id="stats" class="stats"></div></div>
<div class="workspace-toolbar"><div class="view-tabs" role="tablist" aria-label="閱讀方式"><button id="view-reading" role="tab" aria-controls="reading-section" tabindex="0" aria-selected="true">訊息紀錄</button><button id="view-files" role="tab" aria-controls="files-section" tabindex="-1" aria-selected="false">圖片、影片與檔案</button><button id="view-map" role="tab" aria-controls="map-section" tabindex="-1" aria-selected="false">關聯地圖</button></div><label class="search"><span class="sr-only">搜尋訊息、人物或檔名</span><input id="search" type="search" placeholder="搜尋訊息、人物或檔名" autocomplete="off" aria-label="搜尋訊息、人物或檔名"></label><details class="filter-menu"><summary>篩選<span id="filter-count" hidden></span></summary><div class="filter-popover"><label>範圍<select id="scope"><option value="chat">目前聊天室</option><option value="all">全部聊天室</option></select></label><label>訊息類型<select id="kind"><option value="all">全部訊息</option><option value="message">對話</option><option value="album">相簿</option><option value="note">記事本</option><option value="thread">討論串</option><option value="files">含圖片、影片或檔案</option><option value="issues">檔案需處理</option></select></label><label>開始日期<input id="since" type="date"></label><label>結束日期<input id="until" type="date"></label><label>發話者<input id="sender" placeholder="姓名或暱稱"></label><button id="clear" class="secondary">清除篩選</button></div></details></div>
<div class="active-context"><span id="filter-summary">目前聊天室 · 全部訊息</span><button id="reset-filters" class="text-button" hidden>清除篩選</button></div><p id="notice" class="notice" role="status" aria-live="polite"></p><div class="download-status"><span id="download-notice" role="status" aria-live="polite"></span><button id="cancel-download" class="secondary" hidden>停止另存</button></div>
<section id="reading-section" class="view-panel" role="tabpanel" aria-labelledby="view-reading" tabindex="0"><div class="reading-heading"><h3 id="reading-label">已保存的完整訊息</h3><button id="clear-node" class="secondary" hidden>顯示全部訊息</button></div><div id="items"></div><div class="pagination"><button id="previous" class="secondary">上一頁</button><span id="page" aria-live="polite"></span><button id="next" class="secondary">下一頁</button></div></section>
<section id="files-section" class="view-panel" role="tabpanel" aria-labelledby="view-files" tabindex="0" hidden><div class="files-heading"><div><h3>圖片、影片與檔案</h3><p id="files-description">依檔名與保存狀態查看每個項目。</p></div><div class="file-filters"><label>保存狀態<select id="file-status"><option value="all">全部狀態</option><option value="available">已保存原檔</option><option value="preview">僅有預覽</option><option value="pending">等待保存</option><option value="failed">保存失敗</option><option value="unavailable">未取得</option></select></label><label>檔案類型<select id="file-kind"><option value="all">全部類型</option><option value="image">圖片</option><option value="video">影片</option><option value="audio">語音</option><option value="file">一般檔案</option><option value="sticker">貼圖</option></select></label></div></div><div id="file-summary" class="file-summary" aria-label="檔案保存狀態摘要"></div><div id="file-selection-toolbar" class="files-toolbar"><div class="file-selection-actions"><button id="select-file-page" class="secondary">選取本頁可另存檔案</button><button id="select-file-filtered" class="secondary">選取篩選結果</button><button id="clear-file-selection" class="text-button">清除全部選取</button></div><div><span id="selected-count" aria-live="polite"></span><button id="save-selected" disabled>另存選取項目</button></div></div><div id="file-grid" class="file-grid"></div><div class="pagination"><button id="file-previous" class="secondary">上一頁</button><span id="file-page" aria-live="polite"></span><button id="file-next" class="secondary">下一頁</button></div></section>
<section id="map-section" class="map-workspace view-panel" role="tabpanel" aria-labelledby="view-map" tabindex="0" hidden><div class="map-area"><div class="map-caption"><span>選取節點，查看相關訊息</span><span id="map-size"></span></div><svg id="knowledge-map" viewBox="0 0 1000 640" role="group" aria-label="可點選的主題、人物、訊息與檔案"><defs><pattern id="dots" width="24" height="24" patternUnits="userSpaceOnUse"><circle cx="1" cy="1" r="1" fill="#e1e7e2"/></pattern></defs><rect width="1000" height="640" fill="url(#dots)"/></svg><div id="map-empty" class="map-empty" hidden>沒有符合條件的訊息。請清除篩選或選擇其他聊天室。</div><div class="map-bottom"><div class="legend"><span class="topic">關鍵字</span><span class="person">人物</span><span class="source">訊息</span><span class="file">圖片與檔案</span></div><div class="map-controls"><button id="clear-map-selection" class="text-button" hidden>顯示全部</button><button id="zoom-out" class="icon-button" aria-label="縮小地圖">−</button><button id="map-center" class="icon-button" aria-label="重設地圖縮放與位置">⌖</button><span id="zoom-level" aria-live="polite">100%</span><button id="zoom-in" class="icon-button" aria-label="放大地圖">＋</button></div></div></div><aside id="node-detail" class="detail-panel" aria-live="polite"></aside></section>
<footer><span>聊藏 · 本機聊天記錄</span><span>時間 UTC+8 · 不使用線上資源</span></footer></main></div>
<dialog id="preview" aria-label="檔案預覽"><button id="close-preview" class="secondary">關閉</button><div id="preview-content"></div></dialog>
<script type="application/json" id="manifest">@@MANIFEST@@</script><script>@@SCRIPT@@</script></body></html>
)ATLAS";
const char*web_css=R"ATLAS(:root{font-family:system-ui,"Microsoft JhengHei",sans-serif;color:#21312e;background:#f5f7f5;font-size:15px;color-scheme:light;--green:#236b58;--line:#dce4df;--muted:#62726a;--paper:#fff;--soft:#edf3ef;--motion:180ms;--ease:cubic-bezier(.2,.8,.2,1)}
*{box-sizing:border-box}body{margin:0}button,input,select{font:inherit}button{cursor:pointer;background:var(--green);color:#fff;border:1px solid transparent;border-radius:7px;padding:9px 14px;font-weight:600;min-height:40px;transition:background-color var(--motion),border-color var(--motion),color var(--motion)}button:hover{background:#195643}button:active{transform:translateY(1px)}button:disabled{opacity:.45;cursor:default;transform:none}button.secondary{background:#fff;border-color:#cbd7cf;color:#304b3e;font-weight:500}button.secondary:hover{background:#f0f5f1;border-color:#97b1a1}.text-button{background:transparent;color:var(--green);font-size:12px;min-height:36px;padding:6px 9px}.text-button:hover{background:#e8f0ea}input,select{border:1px solid #cbd7cf;border-radius:7px;background:#fff;color:#21312e;padding:9px 11px;min-width:0;min-height:40px}input::placeholder{color:var(--muted)}input:focus-visible,select:focus-visible,button:focus-visible,summary:focus-visible,a:focus-visible,.view-panel:focus-visible{outline:3px solid #7da990;outline-offset:3px}[hidden]{display:none!important}.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}
.app-header{height:76px;background:#fff;border-bottom:1px solid var(--line);padding:0 28px;display:flex;align-items:center;justify-content:space-between;gap:16px}.wordmark{display:flex;gap:12px;align-items:center}.wordmark svg{width:32px;height:32px;fill:var(--green)}.wordmark svg path{fill:none;stroke:#84a991;stroke-width:1.6}.wordmark h1{font-size:22px;font-weight:700;letter-spacing:.08em;line-height:1;margin:0 0 6px}.wordmark span{font-size:12px;color:var(--muted)}.header-note{display:flex;align-items:center;gap:16px;color:var(--muted);font-size:12px}.offline{display:flex;gap:7px;align-items:center;color:#326748;background:#edf4ee;padding:7px 11px;border-radius:5px;white-space:nowrap}.offline i{width:6px;height:6px;border-radius:50%;background:#3f8757}.collection-info{position:relative}.collection-info summary{list-style:none;cursor:pointer;min-height:40px;display:flex;align-items:center;border-radius:7px;white-space:nowrap}.info-popover{position:absolute;right:0;top:45px;z-index:15;background:#fff;border:1px solid var(--line);box-shadow:0 8px 24px #21312e12;padding:20px;width:320px;border-radius:9px;line-height:1.75;color:var(--muted)}.info-popover strong{color:#21312e}.info-popover p{margin:10px 0 0}
.app-shell{display:grid;grid-template-columns:272px minmax(0,1fr);min-height:calc(100vh - 76px)}.sidebar{padding:24px 16px 16px;border-right:1px solid var(--line);background:#fbfcfa;display:flex;flex-direction:column;gap:8px;align-self:start;position:sticky;top:0;max-height:100vh}.sidebar-heading{display:flex;justify-content:space-between;align-items:center;padding:0 8px;color:#3b5145;font-size:13px;font-weight:650}.sidebar-heading>span:last-child{background:#e9eee8;padding:2px 7px;border-radius:4px;color:#65756a;font-size:12px}.sidebar .all-chats{text-align:left;background:transparent;color:#304b3e;border:1px solid transparent;border-radius:7px;padding:12px 10px;margin-top:3px}.all-chats>span{display:block;margin-top:4px;color:var(--muted);font-size:11px;font-weight:400}.sidebar .all-chats:hover{background:#f0f4ef}.sidebar .all-chats.active{background:#e7f1e9;border-color:#d4e3d7;color:#23573c}.chat-filters{display:grid;grid-template-columns:minmax(0,1fr) 104px;gap:7px;margin:6px 0 0;padding-top:15px;border-top:1px solid var(--line)}.chat-filters>label:first-child{grid-column:1/-1;color:var(--muted);font-size:11px}.chat-filters input,.chat-filters select{font-size:12px;padding:8px;min-height:36px;width:100%}.sidebar-result{font-size:11px;color:var(--muted);min-height:13px;padding:0 4px}.sidebar nav{display:flex;flex-direction:column;gap:3px;overflow-y:auto;min-height:0;scrollbar-width:thin;max-height:calc(100vh - 322px)}.sidebar nav>button{text-align:left;background:transparent;color:#3d5146;font-size:13px;font-weight:500;border-radius:6px;padding:10px;line-height:1.5;width:100%;flex:none}.sidebar nav>button:hover{background:#eef3ec}.sidebar nav>button.active{color:#204e38;background:#e5efe6;font-weight:650;box-shadow:inset 3px 0 #397757}.chat-label{display:block;overflow-wrap:anywhere;line-height:1.45}.chat-meta{display:block;font-size:11px;color:var(--muted);font-weight:400;margin-top:4px}.chat-pagination{display:flex;justify-content:space-between;align-items:center;gap:5px;padding-top:10px;border-top:1px solid var(--line);margin-top:6px;color:var(--muted);font-size:11px}.chat-pagination button{font-size:11px;min-height:32px;padding:5px 8px}.sidebar-bottom{margin:7px 6px 0;color:#7a8678;font-size:11px;line-height:1.7}.sidebar-empty{font-size:12px;color:var(--muted);padding:18px 8px;line-height:1.75}
main{min-width:0;padding:32px 32px 18px;max-width:1700px;width:100%;margin:0 auto}.page-heading{display:flex;align-items:center;justify-content:space-between;gap:24px;margin:0 0 24px}.page-heading>div:first-child{min-width:0}.eyebrow{font-size:11px;color:var(--muted);letter-spacing:.08em;margin-bottom:8px}.page-heading h2{font-size:27px;font-weight:680;line-height:1.4;margin:0;color:#223b2d;overflow-wrap:anywhere}.page-heading p{font-size:12px;color:var(--muted);margin:8px 0 0;line-height:1.8;max-width:820px}.stats{display:flex;align-self:stretch;align-items:center;gap:20px;flex-shrink:0}.stat{display:flex;flex-direction:column;gap:5px;padding-left:18px;border-left:1px solid var(--line)}.stat strong{font-size:22px;color:#304b37;font-weight:600;line-height:1.2}.stat span{font-size:11px;color:var(--muted);white-space:nowrap}.workspace-toolbar{display:flex;align-items:center;gap:12px;border-bottom:1px solid var(--line);padding-bottom:15px}.view-tabs{display:flex;gap:4px;flex-shrink:0}.view-tabs button{background:transparent;color:#647467;font-size:13px;padding:9px 12px;font-weight:500;min-height:40px;border:1px solid transparent;border-radius:6px}.view-tabs button:hover{background:#edf2ed}.view-tabs [aria-selected=true]{background:#fff;color:#205c3a;border-color:#ccdcd0;font-weight:650}.search{margin-left:auto;min-width:160px;max-width:340px;flex:1}.search input{width:100%;font-size:12px;height:40px;background:#fff}.filter-menu{position:relative;font-size:12px;color:#405e47}.filter-menu summary{cursor:pointer;padding:10px 12px;border:1px solid #cbd7cf;border-radius:6px;list-style:none;background:#fff;white-space:nowrap;min-height:40px}.filter-menu summary>span{margin-left:7px;background:#e6eee4;padding:1px 5px;border-radius:4px;font-size:11px}.filter-menu[open] summary{border-color:#79a083;background:#f2f7f1}.filter-popover{z-index:12;position:absolute;right:0;top:47px;background:#fff;box-shadow:0 8px 24px #21312e12;border:1px solid var(--line);border-radius:9px;padding:18px;display:grid;grid-template-columns:1fr 1fr;gap:14px;width:340px}.filter-popover label{display:flex;flex-direction:column;gap:6px;font-size:11px;color:#64735f;min-width:0}.filter-popover input,.filter-popover select{width:100%;font-size:12px}.filter-popover button{align-self:end;font-size:12px}.active-context{min-height:44px;display:flex;align-items:center;gap:12px;color:#6a7964;font-size:11px}.view-panel{animation:panel-in var(--motion) var(--ease)}.notice{font-size:12px;line-height:1.8;color:#6f592e;background:#fff8e9;border:1px solid #eadfbe;border-left:3px solid #b28b36;padding:12px 15px;border-radius:5px;margin:0 0 18px}.notice:empty{display:none}
.reading-heading{display:flex;justify-content:space-between;align-items:center;margin:20px 0 14px}.reading-heading h3{font-size:14px;font-weight:600;color:#495f4b;margin:0}.reading-heading button{font-size:12px}.card{background:#fff;border:1px solid #e0e7dc;border-radius:8px;margin:12px 0;padding:19px 22px;scroll-margin-top:20px;max-width:1140px}.card:focus{outline:3px solid #90af80;outline-offset:3px}.meta{display:flex;flex-wrap:wrap;gap:8px;align-items:center;font-size:11px;color:var(--muted);line-height:1.6}.name{color:#315d37;font-size:13px;font-weight:650}.type{font-size:10px;border-radius:3px;padding:2px 6px;background:#edf2e9;color:#68775e}.origin{background:#f3f6ef;border-radius:3px;padding:2px 6px}.card h3{font-size:17px;line-height:1.7;margin:12px 0 7px;color:#314b2e}.text{font-size:15px;line-height:1.9;white-space:pre-wrap;overflow-wrap:anywhere;margin:11px 0 0;color:#293e2b}.card>.secondary{font-size:11px;padding:5px 8px;margin:10px 8px 0 0;min-height:32px}.context{font-size:11px;color:#7a846f}.attachment{display:flex;align-items:center;gap:12px;border-top:1px solid #e9eee3;margin-top:15px;padding-top:15px;min-width:0}.attachment img{width:64px;height:56px;object-fit:cover;border-radius:5px;background:#f0f4eb}.file-info{flex:1;min-width:0}.filename{font-size:13px;font-weight:600;overflow-wrap:anywhere;color:#3c5937}.status{font-size:11px;line-height:1.8;margin-top:4px}.status-available{color:#2d6b3c}.status-preview{color:#527464}.status-pending{color:#6e765e}.status-failed{color:#984e37}.status-unavailable{color:#8a6d36}.attachment-reason{font-size:11px;line-height:1.75;color:#7d6c50;margin:6px 0 0;overflow-wrap:anywhere}.save-file{background:#e6f0e2;color:#32612f;font-size:12px;padding:8px 11px;font-weight:600}.save-file:hover{background:#d9e8d4}.attachment .secondary{font-size:12px;padding:8px 10px}.empty{padding:48px 20px;border:1px dashed #cbd7c2;border-radius:8px;text-align:center;font-size:13px;color:#64785a;line-height:1.8;grid-column:1/-1}
.files-heading{display:flex;align-items:center;justify-content:space-between;gap:20px;margin:18px 0}.files-heading h3{font-size:17px;line-height:1.5;margin:0 0 6px;color:#314b32}.files-heading p{font-size:12px;line-height:1.75;margin:0;color:var(--muted);max-width:650px}.file-filters{display:flex;gap:12px;flex-shrink:0}.file-filters label{display:flex;flex-direction:column;gap:5px;font-size:11px;color:var(--muted)}.file-filters select{min-height:36px;font-size:12px;min-width:120px}.file-summary{display:flex;flex-wrap:wrap;gap:7px;margin:15px 0}.status-chip{background:#f9fbf5;color:#5e7252;border:1px solid #d9e2d1;padding:6px 9px;font-size:11px;font-weight:500;min-height:32px;border-radius:5px}.status-chip:hover{background:#edf4e5}.status-chip[aria-pressed=true]{background:#e6f0dd;border-color:#8dab79;color:#345f26}.status-chip.status-failed{color:#984e37}.status-chip.status-unavailable{color:#8a6d36}.files-toolbar{display:flex;justify-content:space-between;align-items:center;gap:14px;padding:13px 0 18px;font-size:11px;color:#647458;border-top:1px solid #e0e7d9}.files-toolbar>div{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.files-toolbar button{font-size:11px;min-height:34px;padding:6px 9px}.file-selection-actions{flex:1}.file-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(245px,1fr));gap:16px}.file-card{background:#fff;border:1px solid #dce5d3;border-radius:8px;overflow:hidden;min-width:0;display:flex;flex-direction:column}.file-cover{height:168px;background:#eef3e6;display:flex;align-items:center;justify-content:center;position:relative}.file-cover img{width:100%;height:100%;object-fit:cover}.file-kind-label{font-size:20px;color:#829474;font-weight:500;letter-spacing:.06em}.file-badge{position:absolute;left:10px;top:10px;font-size:10px;font-weight:600;background:#fffffff2;border:1px solid #d4dfca;border-radius:4px;padding:4px 7px}.file-card-body{padding:16px;display:flex;flex-direction:column;flex:1}.file-card-body h3{font-size:13px;line-height:1.65;color:#38552c;margin:0 0 7px;overflow-wrap:anywhere}.file-origin{font-size:11px;color:var(--muted);margin:3px 0;line-height:1.7;overflow-wrap:anywhere}.file-card-actions{display:flex;align-items:center;gap:7px;margin-top:auto;padding-top:15px;flex-wrap:wrap}.file-card-actions input{accent-color:#3e7a40;min-height:18px;width:18px;height:18px;margin-right:3px}.file-card-actions button{font-size:11px;padding:6px 9px;min-height:32px}.file-card-actions .save-file{margin-left:auto}.file-card-actions .text-button{width:100%;text-align:left;padding-left:0;font-weight:500;color:#61804e}.file-unavailable{font-size:11px;color:#8b805f;padding:6px 0}.pagination{display:flex;justify-content:center;align-items:center;gap:18px;margin:24px 0;font-size:12px;color:#657b53}.pagination button{font-size:12px}
.map-workspace{display:grid;grid-template-columns:minmax(0,1fr) 302px;min-height:560px;border:1px solid #d5dfcc;background:#fff;border-radius:9px;overflow:hidden}.map-area{position:relative;min-width:0;background:#fbfcf8;display:flex;flex-direction:column}.map-caption{padding:18px 20px 5px;display:flex;justify-content:space-between;gap:14px;font-size:11px;line-height:1.7;color:#6f8061;z-index:1}.map-caption span:last-child{max-width:48%;text-align:right;color:#899479}.map-area>svg{width:100%;flex:1;min-height:450px;display:block;touch-action:pan-y}.map-bottom{padding:10px 16px 16px;display:flex;align-items:center;justify-content:space-between;gap:10px}.legend{display:flex;flex-wrap:wrap;gap:11px;font-size:10px;color:#6b805c}.legend span:before{content:"";display:inline-block;width:6px;height:6px;border-radius:50%;margin-right:5px}.legend .topic:before{background:#b08b49}.legend .person:before{background:#6c87a1}.legend .source:before{background:#648c60}.legend .file:before{background:#98856e}.map-controls{display:flex;align-items:center;background:#fff;border:1px solid #d5dfcb;border-radius:6px;overflow:hidden;flex-shrink:0}.icon-button{font-size:17px;border:0;border-radius:0;background:#fff;color:#557541;padding:5px 10px;min-width:34px}.icon-button:hover{background:#eef4e8}.map-controls .text-button{border-right:1px solid var(--line);border-radius:0}#zoom-level{font-size:10px;min-width:35px;text-align:center;color:#6f825b}.map-empty{position:absolute;inset:40% 8% auto;text-align:center;color:#718564;font-size:13px;line-height:1.8;background:#fbfcf8;padding:18px}.graph-edge{stroke:#d4ddca;stroke-width:1.2;transition:opacity var(--motion),stroke var(--motion)}.graph-edge.mentions{stroke-dasharray:3 5;stroke:#d7cbb5}.graph-edge.active{stroke:#77956b;stroke-width:2}.graph-edge.muted{opacity:.2}.graph-node{cursor:pointer;outline:none;transition:opacity var(--motion)}.graph-node text{fill:#567345;font-family:system-ui,"Microsoft JhengHei",sans-serif;font-size:15px;paint-order:stroke;stroke:#fbfcf8;stroke-width:5px;stroke-linejoin:round}.graph-node.chat text{font-size:17px;font-weight:650;fill:#375b30}.graph-node .node-halo{fill:transparent;stroke:transparent;stroke-width:2}.graph-node:hover .node-halo,.graph-node:focus .node-halo{fill:#d2dfc326;stroke:#87a573}.graph-node:focus-visible .node-halo{stroke:#3d7142;stroke-width:3}.graph-node.selected .node-halo{fill:#cddbba50;stroke:#789e65}.graph-node.selected text{fill:#2d5628;font-weight:650}.graph-node.muted{opacity:.27}.detail-panel{border-left:1px solid var(--line);padding:23px 20px;background:#fff;max-height:700px;overflow:auto}.detail-type{font-size:11px;color:#63794f;padding:4px 8px;background:#eff4e9;display:inline-block;border-radius:4px}.detail-panel h3{font-size:19px;line-height:1.6;color:#3f602f;margin:13px 0 7px;overflow-wrap:anywhere}.detail-subtitle{font-size:12px;line-height:1.8;color:#728262;margin:0 0 18px}.detail-entry{border-top:1px solid #e9eedf;padding:16px 0}.detail-entry .meta{font-size:10px;gap:6px}.detail-entry .text{font-size:13px;color:#5e764e;line-height:1.85;display:-webkit-box;-webkit-line-clamp:5;-webkit-box-orient:vertical;overflow:hidden;white-space:pre-wrap;margin:8px 0}.detail-entry h4{font-size:13px;line-height:1.6;margin:8px 0;color:#486c35}.detail-open{font-size:11px;border:0;background:transparent;padding:5px 0;color:#497d39;font-weight:550;min-height:32px}.detail-open:hover{background:transparent;color:#2d6026}.detail-panel>.secondary{width:100%;margin-top:14px;font-size:12px}.detail-count{font-size:11px;color:#6e835b;line-height:1.8;margin:17px 0 2px}.detail-panel .attachment{flex-wrap:wrap}.detail-panel .attachment .file-info{flex:1 1 130px;min-width:120px}.detail-panel .attachment img{width:52px;height:46px}.detail-panel .attachment button{font-size:11px;padding:6px 8px;min-height:32px}
.download-status{font-size:12px;color:#56724a;line-height:1.8;margin:10px 0 17px;display:flex;gap:12px;align-items:center;flex-wrap:wrap}.download-status:has(#download-notice:empty):has(#cancel-download[hidden]){display:none}.download-status button{font-size:12px}footer{display:flex;justify-content:space-between;gap:12px;margin:25px 0 0;padding:15px 0 0;border-top:1px solid #dce5d2;color:#81906c;font-size:10px;line-height:1.7}dialog{border:1px solid #cbdac0;border-radius:9px;max-width:92vw;max-height:90vh;padding:16px}dialog::backdrop{background:#1d312577}#preview-content img,#preview-content video{display:block;max-width:82vw;max-height:74vh;margin:14px auto}#preview-content audio{margin:20px}.skip{position:fixed;top:-100px;padding:12px;background:#fff;z-index:20;color:var(--green)}.skip:focus{top:0}
@keyframes panel-in{from{opacity:.4;transform:translateY(4px)}to{opacity:1;transform:translateY(0)}}
@media(min-width:1650px){main{padding:38px 42px 22px}.map-workspace{grid-template-columns:minmax(0,1fr) 330px}}
@media(max-width:1240px){.app-shell{grid-template-columns:240px minmax(0,1fr)}main{padding:26px 24px 16px}.stats{gap:12px}.stat{padding-left:12px}.page-heading{align-items:flex-start;gap:14px}.stat strong{font-size:19px}.workspace-toolbar{flex-wrap:wrap}.view-tabs{margin-right:auto}.search{margin-left:0;max-width:none;order:2;flex-basis:calc(100% - 75px)}.filter-menu{order:3}.files-heading{align-items:flex-start;flex-direction:column;gap:14px}.file-filters{width:100%}.file-filters label{flex:1}.file-filters select{width:100%}.files-toolbar{align-items:flex-start;flex-direction:column}.files-toolbar>div:last-child{align-self:flex-end}.map-workspace{grid-template-columns:minmax(0,1fr) 270px}.map-bottom{flex-wrap:wrap}.chat-filters{grid-template-columns:1fr}.chat-filters>label:first-child{grid-column:auto}.sidebar nav{max-height:calc(100vh - 370px)}}
@media(max-width:1000px){.page-heading{flex-direction:column;align-items:stretch}.stats{justify-content:flex-start;gap:22px}.stat:first-child{padding-left:0;border-left:0}.map-workspace{grid-template-columns:minmax(0,1fr)}.detail-panel{border-left:0;border-top:1px solid var(--line);max-height:480px}.map-caption span:last-child{max-width:45%}.file-grid{grid-template-columns:repeat(auto-fill,minmax(220px,1fr))}}
@media(max-width:720px){.app-header{height:70px;padding:0 17px}.wordmark svg{width:28px;height:28px}.wordmark h1{font-size:20px}.header-note{gap:10px}.header-note .offline{font-size:0;padding:8px}.offline i{width:7px;height:7px}.collection-info summary{font-size:11px}.info-popover{position:fixed;top:65px;right:12px;width:min(320px,calc(100vw - 24px))}.app-shell{display:block;min-height:calc(100vh - 70px)}.sidebar{position:static;max-height:none;padding:18px 16px 14px;border-right:0;border-bottom:1px solid var(--line);gap:6px}.sidebar-heading{padding:0}.sidebar .all-chats{padding:9px 10px;display:flex;justify-content:space-between;align-items:center;gap:10px;margin:0}.all-chats>span{margin:0;text-align:right;font-size:10px}.chat-filters{grid-template-columns:minmax(0,1fr) 120px;padding-top:8px;margin:0}.chat-filters>label:first-child{grid-column:1/-1}.sidebar nav{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));max-height:none;gap:5px}.sidebar nav>button{border:1px solid #e2e9db;padding:9px;font-size:12px}.chat-meta{font-size:10px}.chat-pagination{margin-top:3px;padding-top:7px}.sidebar-bottom{display:none}.sidebar-result:empty{display:none}main{padding:23px 17px 16px}.page-heading{gap:15px;margin-bottom:20px}.page-heading h2{font-size:23px}.page-heading p{font-size:11px}.stats{gap:20px}.stat strong{font-size:20px}.stat span{font-size:10px}.view-tabs{gap:2px;width:100%;justify-content:space-between}.view-tabs button{font-size:11px;padding:9px 7px;min-height:38px}.workspace-toolbar{gap:10px}.search{min-width:0;flex-basis:calc(100% - 70px)}.search input{font-size:12px}.filter-popover{position:fixed;right:12px;top:85px;width:min(340px,calc(100vw - 24px));max-height:calc(100vh - 110px);overflow:auto}.active-context{font-size:10px;min-height:42px}.notice{font-size:11px;padding:10px 12px}.reading-heading{margin-top:13px}.reading-heading h3{font-size:13px}.card{padding:16px;margin:10px 0}.text{font-size:14px;line-height:1.9}.attachment{flex-wrap:wrap}.attachment .file-info{flex:1 1 180px}.attachment button{min-height:36px}.files-heading{margin-top:14px}.files-heading h3{font-size:16px}.files-heading p{font-size:11px}.file-filters{gap:8px}.file-filters select{min-width:0}.file-summary{gap:5px}.status-chip{font-size:10px;padding:6px 7px}.file-grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.file-cover{height:120px}.file-kind-label{font-size:17px}.file-badge{left:6px;top:6px;font-size:9px;padding:3px 5px}.file-card-body{padding:11px}.file-card-body h3{font-size:11px}.file-origin,.attachment-reason{font-size:10px}.file-card-actions{gap:5px}.file-card-actions button{font-size:10px;padding:6px 7px}.file-card-actions .save-file{margin-left:0}.file-card-actions input{width:16px;height:16px}.files-toolbar>div:last-child{width:100%;justify-content:space-between}.files-toolbar button{font-size:10px;padding:6px 8px}.map-workspace{border-radius:8px}.map-area>svg{min-height:390px}.map-caption{padding:14px 13px 2px;font-size:10px}.map-bottom{padding:10px 12px 13px}.legend{gap:10px;font-size:9px}.map-controls{margin-left:auto}.detail-panel{padding:20px 17px}.detail-panel h3{font-size:18px}.graph-node text{font-size:14px}.graph-node.chat text{font-size:16px}.pagination{gap:12px;font-size:11px}.pagination button{padding:8px 10px;font-size:11px}footer{font-size:9px}dialog{padding:12px}}
@media(max-width:380px){.header-note{gap:7px}.wordmark{gap:8px}.wordmark svg{width:25px;height:25px}.file-grid{grid-template-columns:1fr}.file-cover{height:160px}.sidebar nav{grid-template-columns:1fr}.view-tabs button{font-size:10px;padding:8px 5px}.stats{gap:14px}.stat{padding-left:10px}.file-selection-actions{gap:5px!important}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{animation:none!important;transition:none!important;scroll-behavior:auto!important}button:active{transform:none}}
)ATLAS";
const char*web_script=R"ATLAS("use strict";
window.VaultGraphModel = (() => {
 const stop=new Set(['我們','你們','他們','這個','那個','可以','已經','目前','如果','所以','以及','還是','沒有','一下','一起','這是','範例','人工','內容','資料','來源','附件','訊息','圖片','檔案','保存','下載','更新','完整','使用','提供','其他','自己','一個','就是','只有','需要','進行','不會','本次','仍會','這份','相同','請問','時候','如何','什麼','這裡','今天','明天','昨天','謝謝']);
 const hash=s=>{let h=2166136261;for(const ch of s){h^=ch.codePointAt(0);h=Math.imul(h,16777619);}return (h>>>0).toString(36);};
 const technicalName=value=>typeof value==='string'&&/^(?:[ucrs][a-f0-9]{16,}|[a-f0-9]{24,})$/i.test(value.trim());
 const displayName=(value,fallback='名稱待確認',resolved=false)=>typeof value==='string'&&value.trim()&&(!technicalName(value)||resolved===true)?value.trim():fallback;
 function fileName(a,item={},index=0){
  const name=typeof a.name==='string'?a.name.trim():'';
  const base=name.replace(/\.[a-z0-9]{1,8}$/i,''),source=typeof item.source_id==='string'?item.source_id:'';
  const generated=!!source&&['image','video','audio','file','sticker'].some(kind=>base===kind+'-'+source);
  if(name&&a.generated_name!==true&&(a.name_is_original===true||!generated))return name;
  const kind={image:'圖片',video:'影片',audio:'語音',sticker:'貼圖',file:'檔案'}[a.kind]||'檔案';
  let stamp='未標示日期';
  if(item.sent_at&&Number.isFinite(Date.parse(item.sent_at))){
   const parts=new Intl.DateTimeFormat('en-CA',{timeZone:'Asia/Taipei',year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit',hourCycle:'h23'}).formatToParts(new Date(item.sent_at));
   const part=key=>parts.find(p=>p.type===key)?.value||'00';stamp=part('year')+'-'+part('month')+'-'+part('day')+'_'+part('hour')+'-'+part('minute')+'-'+part('second');
  }
  const extension=/\.[a-z0-9]{1,8}$/i.exec(typeof a.path==='string'?a.path:'')?.[0]||/\.[a-z0-9]{1,8}$/i.exec(name)?.[0]||'';
  return kind+'_'+stamp+'_'+String(index+1).padStart(2,'0')+extension;
 }
 const personKey=i=>'person:'+(i.sender_ref||i.chat_id+':name:'+i.sender_name);
 const label=i=>displayName(i.title,'',i.name_resolved)||(/^\[LINE 類型 \d+\]$/.test(i.text||'')?'系統訊息':i.text?.replace(/\s+/g,' ').slice(0,28))||({message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'}[i.kind]||'訊息');
 const tags=i=>{const values=[...(Array.isArray(i.tags)?i.tags:[]),...Array.from((i.text||'').matchAll(/#([\p{L}\p{N}_\-]{2,24})/gu),m=>m[1])];return [...new Set(values.filter(x=>typeof x==='string'&&x.length>=2&&x.length<=32))].slice(0,12);};
 function terms(rows){
  const counts=new Map(),explicit=new Set();let segmenter;
  try{segmenter=new Intl.Segmenter('zh-Hant',{granularity:'word'});}catch{}
  for(const row of rows){if(['recalled','deleted'].includes(row.state)||/^\[LINE 類型 \d+\]$/.test(row.text||''))continue;
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
  for(const [index,c]of chats.entries()){const ids=rows.filter(i=>i.chat_id===c.id).map(i=>i.id);if(ids.length||chats.length===1){const id='chat:'+c.id;roots.set(c.id,id);node(id,'chat',displayName(c.title,'未命名聊天室 '+(index+1),c.name_resolved),ids,{chatId:c.id});}}
  for(const c of chats)if(c.parent_id&&roots.has(c.parent_id)&&roots.has(c.id))edge(roots.get(c.parent_id),roots.get(c.id),'belongs','社群中的聊天室');
  if(types.has('topic'))for(const t of terms(rows)){
   const n=node('topic:'+t.term,'topic',t.term,[...t.ids],{explicit:t.explicit});if(!n)continue;
   for(const cid of new Set([...t.ids].map(id=>source.get(id)?.chat_id)))edge(roots.get(cid),n.id,'mentions',t.explicit?'使用標籤':'提到關鍵字');
  }
  if(types.has('person')){
   const people=new Map();
   for(const i of rows){if(!i.sender_name)continue;const pid=personKey(i);if(!people.has(pid))people.set(pid,{name:displayName(i.sender_name,'名稱待確認',i.sender_name_resolved),ids:[],chatIds:new Set(),identified:!!i.sender_ref});people.get(pid).ids.push(i.id);people.get(pid).chatIds.add(i.chat_id);}
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
  if(types.has('file'))for(const i of rows)for(const [index,a]of (i.attachments||[]).entries()){const n=node('file:'+a.id,'file',fileName(a,i,index),[i.id],{attachment:a,itemId:i.id});if(!n)continue;const parent=nodes.has('source:'+i.id)?'source:'+i.id:(i.parent_id&&nodes.has('source:'+i.parent_id)?'source:'+i.parent_id:roots.get(i.chat_id));edge(parent,n.id,'attachment','這裡保存的圖片或檔案');for(const t of nodes.values())if(t.type==='topic'&&t.items.includes(i.id))edge(t.id,n.id,'mentions','相關訊息的圖片或檔案');}
  // Every displayed non-room node has an observed containing room as a fallback.
  for(const n of nodes.values())if(n.type!=='chat'&&![...links.values()].some(e=>e.source===n.id||e.target===n.id)){const row=source.get(n.items[0]);if(row)edge(roots.get(row.chat_id),n.id,'belongs','聊天室中的內容');}
  return {nodes:[...nodes.values()],edges:[...links.values()],totalItems:totalRows,limited:totalRows!==rows.length||chosen.length<rows.length||nodes.size>=max,source};
 }
 return {build,label,terms,presentation:{technicalName,displayName,fileName}};
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
window.VaultDownloads = (catalog, notify, options={}) => {
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
   waiting.set(key,{cancel:()=>finish(new DOMException("Cancelled","AbortError")),receive:value=>{if(received||typeof value!=="string"||value.length>1048576||!/^[A-Za-z0-9+/]*={0,2}$/.test(value)){finish(Error("Invalid part"));return;}received=true;resolve(value);}});
   script.src=entry.path;script.onload=()=>finish(received?null:Error("Missing data"));script.onerror=()=>finish(Error("Missing file"));document.head.appendChild(script);
  });
  stop();const raw=atob(encoded),bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));
  if(bytes.length!==entry.size||!window.crypto?.subtle||hex(await window.crypto.subtle.digest("SHA-256",bytes))!==entry.sha256)throw Error("Integrity check failed");
  stop();return bytes;
 }
 async function saveFile(attachment){
  if(options.directFileDownloads===true&&!catalog[attachment.download]){
   if(!['saved','preview'].includes(attachment.status)||typeof attachment.path!=='string'||!/^assets\/[a-f0-9]{2}\/[a-f0-9]{64}\.(png|jpg|gif|webp|wav|ogg|mp3|mp4|bin)$/.test(attachment.path))throw Error('Invalid local file');
   stop();notify('正在開啟本機檔案：'+attachment.name);
   // Chromium ignores download names for file:// images. Keep the reader open
   // and accurately describe this fallback as opening, never verified saving.
   const link=document.createElement('a');link.href=attachment.path;link.download=attachment.name;link.target='_blank';link.rel='noopener';document.body.appendChild(link);link.click();link.remove();
   await new Promise(resolve=>setTimeout(resolve,0));stop();
   return;
  }
  const entry=catalog[attachment.download];
  if(!entry||entry.sha256!==attachment.download||!Array.isArray(entry.parts)||!entry.parts.length)throw Error("No download data");
  if(!Number.isSafeInteger(entry.size)||entry.size<0)throw Error("Invalid size");
  if(typeof window.crypto?.subtle?.digest!=="function"){const error=Error();error.cryptoRequired=true;throw error;}
  let output=null;const chunks=[];let size=0;
  if(entry.size>64*1024*1024){
   if(typeof window.showSaveFilePicker!=="function"){const error=Error();error.browserRequired=true;throw error;}
   const handle=await window.showSaveFilePicker({suggestedName:attachment.name});
   stop();output=await handle.createWritable();
  }
  try{
   for(let n=0;n<entry.parts.length;n++){
    stop();notify(`正在準備 ${attachment.name}（${n+1} / ${entry.parts.length}）`);
    const bytes=await part(entry.sha256,n,entry.parts[n]);size+=bytes.length;if(size>entry.size||(!output&&size>64*1024*1024))throw Error("Wrong size");
    if(output)await output.write(bytes);else chunks.push(bytes);
   }
   stop();if(size!==entry.size)throw Error("Wrong size");
   if(output){await output.close();output=null;}
   else{
    const blob=new Blob(chunks,{type:"application/octet-stream"});
    if(hex(await window.crypto.subtle.digest("SHA-256",await blob.arrayBuffer()))!==entry.sha256)throw Error("Wrong checksum");
    stop();const url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download=attachment.name;
    document.body.appendChild(link);link.click();link.remove();urls.push(url);
    while(urls.length>4)URL.revokeObjectURL(urls.shift());
    setTimeout(()=>{URL.revokeObjectURL(url);urls=urls.filter(x=>x!==url);},60000);
   }
  }catch(error){if(output)await output.abort().catch(()=>{});throw error;}
 }
 return {
  busy:()=>busy,
  cancel:()=>{if(!busy)return;aborted=true;for(const item of [...waiting.values()])item.cancel();notify("正在停止另存，已完成的檔案仍會保留。");},
  save:async attachments=>{
   if(busy){notify("正在另存檔案，請等目前操作完成。");return;}
   if(attachments.length>1&&attachments.some(a=>catalog[a.download]?.size>64*1024*1024)){notify("選取內容含大檔案，請分別按各檔案的「另存檔案」，或用桌面程式製作 ZIP。");return;}
   busy=true;aborted=false;let count=0;
   try{for(const a of attachments){stop();await saveFile(a);count++;}notify(options.directFileDownloads===true&&attachments.some(a=>!catalog[a.download])?`已開啟 ${count} 個本機檔案。若要用畫面顯示的檔名另存，請回桌面程式更新閱讀頁；本次未重新計算原檔校驗值。`:`已準備 ${count} 個檔案，請查看瀏覽器的下載項目或選定位置。`);}
   catch(error){notify(error.name==="AbortError"?"另存已取消，已完成的檔案仍會保留。":error.cryptoRequired?"這個瀏覽器無法驗證檔案，請用新版 Edge 或 Chrome，或在桌面程式檢查後取用原檔。":error.browserRequired?"這份檔案較大，請用新版 Edge 或 Chrome 開啟此頁後另存；原檔仍在保存資料夾。":"無法完成另存，請確認整個保存資料夾都在，並在桌面程式執行檔案檢查。");}
   finally{busy=false;}
  }
 };
};

"use strict";
(() => {
 const manifest=JSON.parse(document.getElementById('manifest').textContent),$=id=>document.getElementById(id),presentation=window.VaultGraphModel.presentation;
 const kinds={message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'};
 const chatKinds={direct:'個人對話',group:'群組',community:'社群',subchat:'社群聊天室'};
 const types={chat:'聊天室',topic:'關鍵字',person:'人物',source:'訊息',file:'圖片或檔案'};
 const states={available:'已保存原檔',preview:'僅有預覽',pending:'等待保存',failed:'保存失敗',unavailable:'未取得'};
 const fileKinds={image:'圖片',video:'影片',audio:'語音',file:'一般檔案',sticker:'貼圖'};
 manifest.chats=manifest.chats.map((c,index)=>{const fallback='未命名'+(c.kind==='direct'?'對話':'聊天室')+' '+(index+1),title=presentation.displayName(c.title,fallback,c.name_resolved),unresolved=c.name_resolved===false||title===fallback||/^未命名(?:對話|群組|聊天室|社群)/.test(title);return {...c,title:title+(unresolved?'（名稱待確認）':'')};});
 const data=new Map(),loading=new Map(),selected=new Map(),chatTitles=new Map(manifest.chats.map(c=>[c.id,c.title]));
 const chatPageSize=8,readingPageSize=60,filePageSize=24;
 let chat=manifest.chats[0]?.id||null,page=0,filePage=0,chatPage=0,version=0,current=[],filtered=[],view='reading',focusNode=null,mapModel=null,mapDirty=true,contextKey='',visibleFiles=[];
 const el=(tag,text,cls)=>{const n=document.createElement(tag);if(text!==undefined)n.textContent=text;if(cls)n.className=cls;return n;};
 const localPath=p=>typeof p==='string'&&/^assets\/[a-f0-9]{2}\/[a-f0-9]{64}\.(png|jpg|gif|webp|wav|ogg|mp3|mp4|bin)$/.test(p)?p:null;
 const date=t=>t&&Number.isFinite(Date.parse(t))?new Date(t).toLocaleString('zh-TW',{timeZone:'Asia/Taipei',year:'numeric',month:'numeric',day:'numeric',hour:'2-digit',minute:'2-digit'}):'未標示時間';
 const size=n=>n==null?'大小未提供':n>=1048576?(n/1048576).toFixed(1)+' MB':n>=1024?(n/1024).toFixed(1)+' KB':n+' B';
 // Capture presentation helpers: plain text only, not an assertion of full LINE coverage.
 const unfinished=new Set(['running','stopped','failed','interrupted','complete_with_issues']);
 function captureSummary(chats){
  if(chats.length!==1){const pending=chats.filter(c=>!c.last_job||unfinished.has(c.last_job.status)).length,unknown=chats.filter(c=>!c.last_job||!['local_only','visible_only','source_range'].includes(c.last_job.coverage)).length;return chats.length+' 個聊天室'+(pending?' · '+pending+' 個尚未完成或有需處理項目':'')+(unknown?' · '+unknown+' 個來源範圍未確認':' · 範圍依各聊天室紀錄');}
  const job=chats[0]?.last_job;if(!job)return '尚無整理紀錄，已保存訊息仍可閱讀。';
  const statuses={complete:'上次整理完成',complete_with_issues:'上次整理有需處理項目',stopped:'上次整理已停止，已保存內容保留',failed:'上次整理未完成',interrupted:'上次整理曾中斷',running:'上次整理尚未完成'};
  const ranges={local_only:'僅限本機來源提供的內容',visible_only:'僅限當時可讀範圍',source_range:'依所選來源範圍，不代表完整歷史'};
  const parts=[statuses[job.status]||'上次整理狀態待確認',ranges[job.coverage]||'來源範圍尚未確認'];
  if(job.finished_at&&Number.isFinite(Date.parse(job.finished_at)))parts.push(date(job.finished_at));
  const counts=job.counts||{};for(const [key,label]of [['downloaded','新增檔案'],['reused','沿用檔案'],['failed','待重試'],['unavailable','未取得']])if(Number.isSafeInteger(counts[key])&&counts[key]>=0)parts.push(label+' '+counts[key]);
  return parts.join(' · ');
 }
 function attachmentReason(a){if(a.status==='saved')return '';return typeof a.reason==='string'&&a.reason.trim()?a.reason.trim().replaceAll('附件','檔案').slice(0,300):a.status==='preview'?'目前只有預覽版本，原檔尚未取得。':a.status==='unavailable'?'此次未取得原檔，可回程式重新嘗試。':a.status==='failed'?'保存時發生錯誤，可回程式重試。':'此檔案尚未完成保存，可繼續整理。';}
 // End capture presentation helpers.
 const direct=manifest.direct_file_downloads===true;
 const downloads=window.VaultDownloads(manifest.downloads||{},message=>{$('download-notice').textContent=message;$('cancel-download').hidden=!downloads.busy();},{directFileDownloads:direct});
 const graph=window.VaultGraphView($('knowledge-map'),showNode);
 if(manifest.artificial_data){$('sample-label').hidden=false;document.title='聊藏 · 人工聊天記錄範例';}
 if(direct)$('files-description').textContent='已保存的本機檔案可在新分頁開啟。若要用畫面顯示的檔名另存，請回桌面程式更新閱讀頁。';
 function presentRows(rows,id){return rows.map(i=>{const resolved=i.sender_name_resolved??i.metadata?.sender_name_resolved,name=presentation.displayName(i.sender_name,i.kind==='message'||i.kind==='thread'?'名稱待確認':kinds[i.kind]||'系統訊息',resolved);return {...i,chat_id:i.chat_id||id,sender_ref:i.sender_ref||(presentation.technicalName(i.sender_name)?'unresolved:'+id+':'+i.sender_name:undefined),sender_name:name+(name!=='名稱待確認'&&(resolved===false||/^未知成員[（(]/.test(name))?'（名稱待確認）':''),title:presentation.displayName(i.title,'',i.name_resolved),attachments:(i.attachments||[]).map((a,index)=>({...a,name:presentation.fileName(a,i,index),generated_name:false,name_is_original:true}))};});}
 window.VaultData={add:(id,rows)=>{if(!data.has(id))data.set(id,[]);data.get(id).push(...presentRows(rows,id));}};
 function load(id){
  if(loading.has(id))return loading.get(id);const c=manifest.chats.find(c=>c.id===id);if(!c)return Promise.reject(Error('Unknown chat'));data.set(id,[]);
  const job=(async()=>{for(const src of c.chunks){if(!/^reader\/[a-f0-9]{32}\/[a-f0-9]{64}-\d+\.js$/.test(src))throw Error('Invalid local data');await new Promise((ok,fail)=>{const s=el('script');s.src=src;s.onload=ok;s.onerror=fail;document.head.appendChild(s);});}return data.get(id);})();loading.set(id,job);return job;
 }
 function setView(next){
  view=next;for(const [id,section,key]of [['view-reading','reading-section','reading'],['view-files','files-section','files'],['view-map','map-section','map']]){$(id).setAttribute('aria-selected',String(key===view));$(id).tabIndex=key===view?0:-1;$(section).hidden=key!==view;}
  if(view==='reading')drawReading();if(view==='files')drawFiles();if(view==='map')drawMap();
 }
 function nav(){
  const n=$('chats');n.replaceChildren();$('chat-count').textContent=manifest.chats.length;
  const query=$('chat-search').value.trim().toLocaleLowerCase(),kind=$('chat-kind').value;
  const chats=manifest.chats.filter(c=>(!query||c.title.toLocaleLowerCase().includes(query))&&(kind==='all'||c.kind===kind));
  const pages=Math.max(1,Math.ceil(chats.length/chatPageSize));chatPage=Math.min(Math.max(0,chatPage),pages-1);
  $('chat-results').textContent=query||kind!=='all'?'符合 '+chats.length+' 個，共 '+manifest.chats.length+' 個':'';
  for(const c of chats.slice(chatPage*chatPageSize,(chatPage+1)*chatPageSize)){
   const b=el('button');b.append(el('span',c.title,'chat-label'),el('span',c.count+' 筆紀錄 · '+(chatKinds[c.kind]||'聊天室'),'chat-meta'));b.className=c.id===chat&&$('scope').value!=='all'?'active':'';b.setAttribute('aria-pressed',String(c.id===chat&&$('scope').value!=='all'));
   b.onclick=()=>{chat=c.id;$('scope').value='chat';focusNode=null;page=0;filePage=0;setView('reading');nav();render();};n.append(b);
  }
  if(!chats.length)n.append(el('p','沒有符合條件的聊天室。','sidebar-empty'));
  $('chat-page').textContent=(chatPage+1)+' / '+pages;$('chat-previous').disabled=chatPage===0;$('chat-next').disabled=chatPage+1>=pages;
  const all=$('scope').value==='all';$('all-chats').classList.toggle('active',all);$('all-chats').setAttribute('aria-pressed',String(all));
 }
 function downloadable(a){return !!localPath(a.path)&&['saved','preview'].includes(a.status)&&!!manifest.downloads?.[a.download];}
 function locallyOpenable(a){return direct&&!!localPath(a.path)&&['saved','preview'].includes(a.status)&&!downloadable(a);}
 const fileAction=a=>locallyOpenable(a)?a.status==='preview'?'開啟本機預覽':'開啟本機原檔':a.status==='preview'?'另存預覽':'另存檔案';
 function fileStatus(a){if(a.status==='saved')return downloadable(a)||locallyOpenable(a)?'available':'failed';if(a.status==='preview')return localPath(a.path)?'preview':'failed';return ['pending','failed','unavailable'].includes(a.status)?a.status:'pending';}
 function canPreview(a){const p=localPath(a.path);return p&&['saved','preview'].includes(a.status)&&((['image','sticker'].includes(a.kind)&&/\.(png|jpg|gif|webp)$/.test(p))||(a.kind==='video'&&p.endsWith('.mp4'))||(a.kind==='audio'&&/\.(wav|ogg|mp3)$/.test(p)));}
 function preview(a){const box=$('preview-content');box.replaceChildren();const p=localPath(a.path);if(!canPreview(a))return;let media;if(['image','sticker'].includes(a.kind)){media=el('img');media.alt=a.name;}else{media=el(a.kind==='video'?'video':'audio');media.controls=true;}media.src=p;media.onerror=()=>box.replaceChildren(el('p','無法預覽此檔案，請嘗試另存或回程式檢查。'));box.append(media);$('preview').showModal();}
 function counts(){$('selected-count').textContent=selected.size?'已選 '+selected.size+' 個檔案':'尚未選取檔案';$('save-selected').disabled=!selected.size;$('clear-file-selection').disabled=!selected.size;}
 async function save(a){await downloads.save([a]);$('cancel-download').hidden=true;}
 function checkbox(a){const cb=el('input');cb.type='checkbox';cb.checked=selected.has(a.id);cb.setAttribute('aria-label','選取 '+a.name);cb.onchange=()=>{if(cb.checked)selected.set(a.id,a);else selected.delete(a.id);counts();};return cb;}
 function displayReason(a){return a.status==='saved'&&fileStatus(a)==='failed'?'保存紀錄存在，但本機檔案或另存資料需檢查。請回程式檢查或重新產生閱讀頁。':attachmentReason(a);}
 function attachment(a){
  const row=el('div',undefined,'attachment');if(canPreview(a)&&['image','sticker'].includes(a.kind)){const img=el('img');img.src=localPath(a.path);img.alt=a.name;img.loading='lazy';img.onerror=()=>img.remove();row.append(img);}
  const info=el('div',undefined,'file-info'),status=fileStatus(a);info.append(el('div',a.name,'filename'),el('div',(a.saved_before?'先前保存 · ':'')+(states[status]||'待確認')+' · '+size(a.size),'status status-'+status));const reason=displayReason(a);if(reason)info.append(el('p',reason,'attachment-reason'));row.append(info);
  if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);row.append(b);}if(downloadable(a)||locallyOpenable(a)){const b=el('button',fileAction(a),'save-file');b.onclick=()=>save(a);row.append(b);}return row;
 }
 function meta(i){const line=el('div',undefined,'meta');line.append(el('span',i.sender_name||kinds[i.kind]||'系統訊息','name'),el('span',kinds[i.kind]||'訊息','type'));if(i.sent_at)line.append(el('time',date(i.sent_at)));if($('scope').value==='all'&&chatTitles.has(i.chat_id))line.append(el('span',chatTitles.get(i.chat_id),'origin'));return line;}
 const bodyText=i=>i.state==='recalled'?'此訊息已收回':i.state==='deleted'?'此內容已刪除':i.content_omitted===true?'未保存對話文字':/^\[LINE 類型 18\]$/.test(i.text||'')?'系統訊息（詳細文字尚未取得）':/^\[LINE 類型 \d+\]$/.test(i.text||'')?'此訊息的文字內容尚未取得。':i.text;
 function card(i){
  const c=el('article',undefined,'card');c.id='item-'+i.id;c.append(meta(i));if(i.title)c.append(el('h3',i.title));
  if(i.parent_id){const p=current.find(r=>r.id===i.parent_id);if(p){const b=el('button','查看所屬'+(kinds[p.kind]||'訊息'),'secondary');b.onclick=()=>jump(p.id);c.append(b);}else c.append(el('p','所屬訊息尚未保存。','context'));}
  if(i.reply_source_id){const r=current.find(r=>r.chat_id===i.chat_id&&r.source_id===i.reply_source_id);if(r){const b=el('button','查看引用訊息','secondary');b.onclick=()=>jump(r.id);c.append(b);}else c.append(el('p','引用的原訊息尚未保存。','context'));}
  c.append(el('p',bodyText(i),'text'));for(const a of i.attachments)c.append(attachment(a));return c;
 }
 function showNode(node,overview=false){
  if(!node){const first=mapModel?.nodes.find(n=>n.type==='chat');if(first)return showNode(first,true);graph.select(null);return;}
  graph.select(overview?null:node.id);const panel=$('node-detail');panel.replaceChildren();panel.append(el('span',types[node.type]||'訊息','detail-type'),el('h3',node.title));
  const subtitle=node.type==='topic'?'含有此關鍵字的已保存訊息。':node.type==='person'?'來自這位成員的已保存訊息。':node.type==='file'?'這個檔案的保存狀態與相關訊息。':'每個節點都能回到已保存的原文。';panel.append(el('p',subtitle,'detail-subtitle'));
  if(node.attachment)panel.append(attachment(node.attachment));const related=node.items.map(id=>current.find(i=>i.id===id)).filter(Boolean);panel.append(el('div',related.length+' 則相關訊息','detail-count'));
  for(const i of related.slice(0,3)){const entry=el('div',undefined,'detail-entry');entry.append(meta(i));if(i.title)entry.append(el('h4',i.title));entry.append(el('p',bodyText(i),'text'));const b=el('button','閱讀原文 ↗','detail-open');b.onclick=()=>jump(i.id);entry.append(b);panel.append(entry);}
  if(related.length){const b=el('button','閱讀全部相關內容','secondary');b.onclick=()=>{focusNode={title:node.title,ids:new Set(node.items)};page=0;setView('reading');};panel.append(b);}
 }
 function drawMap(){
  if(!mapDirty)return;mapDirty=false;const chats=manifest.chats.filter(c=>$('scope').value==='all'||c.id===chat);mapModel=window.VaultGraphModel.build(filtered,chats);graph.set(mapModel);$('map-size').textContent=mapModel.limited?'地圖僅顯示部分節點；訊息紀錄保留全部已保存內容':'';$('map-empty').hidden=filtered.length>0;
  const first=mapModel.nodes.find(n=>n.type==='chat');if(first)showNode(first,true);else $('node-detail').replaceChildren(el('p','清除篩選或選擇其他聊天室。','detail-subtitle'));
 }
 function drawReading(){
  const rows=focusNode?filtered.filter(i=>focusNode.ids.has(i.id)):filtered,pages=Math.max(1,Math.ceil(rows.length/readingPageSize));page=Math.max(0,Math.min(page,pages-1));$('reading-label').textContent=focusNode?'關於「'+focusNode.title+'」':'已保存的完整訊息';$('clear-node').hidden=!focusNode;
  const box=$('items');box.replaceChildren();for(const i of rows.slice(page*readingPageSize,(page+1)*readingPageSize))box.append(card(i));if(!rows.length)box.append(el('div','沒有符合條件的訊息，請調整日期或清除篩選。','empty'));$('page').textContent='第 '+(page+1)+' / '+pages+' 頁 · 共 '+rows.length+' 筆';$('previous').disabled=page===0;$('next').disabled=page+1>=pages;
 }
 function scopedFiles(){return filtered.flatMap(i=>i.attachments.map(a=>({i,a})));}
 function matchingFiles(){const status=$('file-status').value,kind=$('file-kind').value;return scopedFiles().filter(({a})=>(status==='all'||fileStatus(a)===status)&&(kind==='all'||a.kind===kind));}
 function drawFileSummary(files){
  const summary=$('file-summary');summary.replaceChildren();for(const [key,label]of [['all','全部檔案'],...Object.entries(states)]){const count=key==='all'?files.length:files.filter(({a})=>fileStatus(a)===key).length,b=el('button',label+' '+count,'status-chip status-'+key);b.setAttribute('aria-pressed',String($('file-status').value===key));b.onclick=()=>{$('file-status').value=key;filePage=0;drawFiles();};summary.append(b);}
 }
 function drawFiles(){
  const box=$('file-grid');box.replaceChildren();drawFileSummary(scopedFiles());visibleFiles=matchingFiles();$('file-selection-toolbar').hidden=direct&&!scopedFiles().some(({a})=>downloadable(a));const pages=Math.max(1,Math.ceil(visibleFiles.length/filePageSize));filePage=Math.max(0,Math.min(filePage,pages-1));
  for(const {i,a}of visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize)){
   const c=el('article',undefined,'file-card'),cover=el('div',undefined,'file-cover'),status=fileStatus(a);c.dataset.status=status;
   if(canPreview(a)&&['image','sticker'].includes(a.kind)){const img=el('img');img.src=localPath(a.path);img.alt=a.name;img.loading='lazy';cover.append(img);cover.onclick=()=>preview(a);}else cover.append(el('span',fileKinds[a.kind]||'檔案','file-kind-label'));
   cover.append(el('span',states[status]||'待確認','file-badge status-'+status));
   const body=el('div',undefined,'file-card-body');body.append(el('h3',a.name),el('p',date(i.sent_at)+' · '+size(a.size),'file-origin'),el('p',(chatTitles.get(i.chat_id)||'聊天室')+' · '+i.sender_name,'file-origin'));const reason=displayReason(a);if(reason)body.append(el('p',reason,'attachment-reason'));
   const actions=el('div',undefined,'file-card-actions');if(downloadable(a))actions.append(checkbox(a));if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);actions.append(b);}if(downloadable(a)||locallyOpenable(a)){const b=el('button',fileAction(a),'save-file');b.onclick=()=>save(a);actions.append(b);}else actions.append(el('span','原檔尚不可另存','file-unavailable'));const message=el('button','查看訊息','text-button');message.onclick=()=>jump(i.id);actions.append(message);body.append(actions);c.append(cover,body);box.append(c);
  }
  if(!visibleFiles.length)box.append(el('div','沒有符合此狀態與類型的檔案。可切回「全部狀態」查看。','empty'));$('file-page').textContent='第 '+(filePage+1)+' / '+pages+' 頁 · 共 '+visibleFiles.length+' 個';$('file-previous').disabled=filePage===0;$('file-next').disabled=filePage+1>=pages;$('select-file-page').disabled=!visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize).some(({a})=>downloadable(a));$('select-file-filtered').disabled=!visibleFiles.some(({a})=>downloadable(a));counts();
 }
 function isKind(i,kind){if(kind==='all')return true;if(kind==='files')return i.attachments.length>0;if(kind==='issues')return i.attachments.some(a=>!['available','preview'].includes(fileStatus(a)));let p=i,seen=new Set();while(p&&!seen.has(p.id)){seen.add(p.id);if(p.kind===kind)return true;p=current.find(x=>x.id===p.parent_id);}return false;}
 function drawStats(){
  const files=scopedFiles(),counts=[[(filtered.filter(i=>i.kind==='message'||i.kind==='thread').length),' 則訊息'],[files.filter(({a})=>['available','preview'].includes(fileStatus(a))).length,' 個本機檔案'],[files.filter(({a})=>!['available','preview'].includes(fileStatus(a))).length,' 個需處理']];$('stats').replaceChildren();for(const [count,label]of counts){const item=el('div',undefined,'stat');item.append(el('strong',String(count)),el('span',label));$('stats').append(item);}
 }
 function drawNotice(chats){
  const files=scopedFiles(),issues=files.filter(({a})=>!['available','preview'].includes(fileStatus(a))).length,stopped=chats.some(c=>['stopped','interrupted','running'].includes(c.last_job?.status));
  let notice=stopped?'上次整理已停止或中斷，已保存的訊息與本機檔案保留在這裡。可回桌面程式繼續整理。':'';
  if(issues)notice+=(notice?' ':'')+issues+' 個檔案需要處理；請到「圖片、影片與檔案」查看名稱、狀態與原因。';
  else if(!notice&&chats.some(c=>c.last_job?.status==='failed'))notice='上次整理未完成，本頁顯示當時已保存的訊息。可回桌面程式重試。';
  else if(!notice&&chats.some(c=>c.last_job?.status==='complete_with_issues'))notice='上次整理紀錄有需處理項目；本頁顯示目前已保存的訊息與檔案。可回桌面程式查看整理紀錄。';
  if(direct)notice+=(notice?' ':'')+'已保存的本機原檔仍可開啟；若要用顯示檔名另存，請回桌面程式更新閱讀頁。';$('notice').textContent=notice;
 }
 async function render(){
  const turn=++version;$('notice').textContent='';$('content').setAttribute('aria-busy','true');$('filter-summary').textContent='正在載入訊息…';
  const ids=$('scope').value==='all'?manifest.chats.map(c=>c.id):chat?[chat]:[],key=ids.join('|'),all=$('scope').value==='all',c=manifest.chats.find(c=>c.id===chat);
  $('title').textContent=all?'全部聊天室':c?.title||'沒有聊天室';$('section-label').textContent=all?'聊天總覽':'聊天記錄';
  if(key!==contextKey){contextKey=key;current=[];filtered=[];mapDirty=true;drawStats();$('coverage').textContent=captureSummary(manifest.chats.filter(c=>ids.includes(c.id)));$('items').replaceChildren(el('p','正在載入此範圍的已保存訊息…','empty'));$('file-grid').replaceChildren();if(view==='map')drawMap();}
  try{
   for(let offset=0;offset<ids.length;offset+=4){await Promise.all(ids.slice(offset,offset+4).map(load));if(turn!==version)return;}
   current=ids.flatMap(id=>data.get(id)||[]);const q=$('search').value.trim().toLocaleLowerCase(),sender=$('sender').value.trim().toLocaleLowerCase(),kind=$('kind').value;
   filtered=current.filter(i=>{const text=[i.text,i.title,i.sender_name,...i.attachments.map(a=>a.name)].join(' ').toLocaleLowerCase(),day=i.sent_at&&Number.isFinite(Date.parse(i.sent_at))?new Date(i.sent_at).toLocaleDateString('en-CA',{timeZone:'Asia/Taipei'}):'';return (!q||text.includes(q))&&(!sender||i.sender_name.toLocaleLowerCase().includes(sender))&&isKind(i,kind)&&(!$('since').value||day&&day>=$('since').value)&&(!$('until').value||day&&day<=$('until').value);});
   const chats=manifest.chats.filter(c=>ids.includes(c.id));$('coverage').textContent=captureSummary(chats);drawStats();drawNotice(chats);mapDirty=true;drawReading();drawFiles();if(view==='map')drawMap();nav();
  }catch{if(turn===version){current=[];filtered=[];mapDirty=true;drawStats();drawReading();drawFiles();if(view==='map')drawMap();$('notice').textContent='部分訊息檔案讀不到。請確認整份保存資料夾都在，或回桌面程式重新產生閱讀頁。';}}
  finally{if(turn===version){$('content').setAttribute('aria-busy','false');updateFilterSummary();}}
 }
 async function jump(id){const target=current.find(i=>i.id===id);if(!target)return;for(const key of ['search','sender','since','until'])$(key).value='';$('kind').value='all';focusNode=null;await render();page=Math.floor(filtered.findIndex(i=>i.id===id)/readingPageSize);setView('reading');const n=document.getElementById('item-'+id);n?.setAttribute('tabindex','-1');n?.scrollIntoView({block:'center'});n?.focus({preventScroll:true});}
 function updateFilterSummary(){const count=['search','sender','since','until'].filter(id=>$(id).value.trim()).length+Number($('kind').value!=='all');$('filter-count').textContent=count;$('filter-count').hidden=!count;$('reset-filters').hidden=!count;const scope=$('scope').value==='all'?'全部聊天室':'目前聊天室';$('filter-summary').textContent=count?scope+' · '+count+' 項篩選 · '+filtered.length+' 筆結果':scope+' · 全部已保存訊息';}
 function clearFilters(){for(const id of ['search','sender','since','until'])$(id).value='';$('kind').value='all';focusNode=null;page=0;filePage=0;render();}
 const tabs=[$('view-reading'),$('view-files'),$('view-map')],views=['reading','files','map'];
 for(const [index,tab]of tabs.entries())tab.addEventListener('keydown',event=>{let next=index;if(event.key==='ArrowRight')next=(index+1)%tabs.length;else if(event.key==='ArrowLeft')next=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')next=0;else if(event.key==='End')next=tabs.length-1;else return;event.preventDefault();setView(views[next]);tabs[next].focus();});
 document.addEventListener('keydown',event=>{const editing=/INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName||'');if(event.key==='/'&&!editing&&!event.ctrlKey&&!event.metaKey&&!event.altKey){event.preventDefault();$('search').focus();}if(event.key==='Escape'){const open=document.querySelector('details[open]');if(open){open.open=false;open.querySelector('summary')?.focus();}}});
 document.addEventListener('click',event=>{for(const details of document.querySelectorAll('details[open]'))if(!details.contains(event.target))details.open=false;});
 $('reset-filters').onclick=clearFilters;$('clear-map-selection').onclick=()=>{showNode(null);$('knowledge-map').querySelector('[tabindex="0"]')?.focus();};$('preview').addEventListener('close',()=>{$('preview-content').replaceChildren();});
 for(const [id,key]of [['view-map','map'],['view-reading','reading'],['view-files','files']])$(id).onclick=()=>setView(key);
 for(const id of ['search','sender','since','until','kind','scope'])$(id).addEventListener('input',()=>{page=0;filePage=0;focusNode=null;render();});
 for(const id of ['chat-search','chat-kind'])$(id).addEventListener('input',()=>{chatPage=0;nav();});
 for(const id of ['file-status','file-kind'])$(id).addEventListener('input',()=>{filePage=0;drawFiles();});
 $('chat-previous').onclick=()=>{chatPage--;nav();};$('chat-next').onclick=()=>{chatPage++;nav();};
 $('all-chats').onclick=()=>{$('scope').value='all';focusNode=null;page=0;filePage=0;setView('map');render();};$('clear').onclick=clearFilters;$('clear-node').onclick=()=>{focusNode=null;page=0;drawReading();};$('previous').onclick=()=>{page--;drawReading();};$('next').onclick=()=>{page++;drawReading();};$('file-previous').onclick=()=>{filePage--;drawFiles();};$('file-next').onclick=()=>{filePage++;drawFiles();};
 $('select-file-page').onclick=()=>{for(const {a}of visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize))if(downloadable(a))selected.set(a.id,a);drawFiles();};$('select-file-filtered').onclick=()=>{for(const {a}of visibleFiles)if(downloadable(a))selected.set(a.id,a);drawFiles();};$('clear-file-selection').onclick=()=>{selected.clear();drawFiles();};
 $('zoom-in').onclick=()=>graph.zoom(1.2);$('zoom-out').onclick=()=>graph.zoom(1/1.2);$('map-center').onclick=()=>graph.center();$('close-preview').onclick=()=>{$('preview').close();$('preview-content').replaceChildren();};$('cancel-download').onclick=()=>downloads.cancel();$('save-selected').onclick=async()=>{await downloads.save([...selected.values()]);$('cancel-download').hidden=true;};nav();setView('reading');render();
})();
)ATLAS";
const char*worker_script=R"ATLAS($ErrorActionPreference = 'Stop'
# Use redirected streams directly: no console window/code-page API is required.
# Reuse the bootstrap reader so a queued first request is never discarded.
if ($null -eq $ChatAtlasInput) {
 $ChatAtlasInput=[IO.StreamReader]::new([Console]::OpenStandardInput(),[Text.UTF8Encoding]::new($false,$true),$true)
}
[Console]::SetIn($ChatAtlasInput)
$ChatAtlasOutput=[IO.StreamWriter]::new([Console]::OpenStandardOutput(),[Text.UTF8Encoding]::new($false))
$ChatAtlasOutput.AutoFlush=$true
[Console]::SetOut($ChatAtlasOutput)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VaultNative {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint command);
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
# Only a new foreground confirmation owned by this exact Save As window is ours.
# Unknown dialogs and any window predating the download are never closed.
function Assert-OwnedSaveConfirmation($confirmation,$dialog,$dialogKey,$dialogHandle,$requireForeground=$true) {
 Assert-Window
 $saved=$dialog.Current;$c=$confirmation.Current
 if ((Node-Key $dialog) -cne $dialogKey -or $saved.NativeWindowHandle -ne $dialogHandle -or $saved.ProcessId -ne $script:ProcessId -or $saved.ClassName -cne '#32770' -or $saved.Name -cnotin $script:SaveDialogNames) { throw 'save_dialog_unsupported' }
 if ($c.ProcessId -ne $script:ProcessId -or $c.ClassName -cne '#32770' -or $c.NativeWindowHandle -eq 0 -or $c.NativeWindowHandle -eq $dialogHandle -or !$c.IsEnabled -or $c.IsOffscreen -or $c.Name -cnotin @('Confirm Save As','Confirm Save as','確認另存新檔','確認另存為','確認另存为','确认另存为','名前を付けて保存の確認')) { throw 'save_dialog_unsupported' }
 if ([VaultNative]::GetWindow([IntPtr]$c.NativeWindowHandle,4) -ne [IntPtr]$dialogHandle) { throw 'focus_changed' }
 if ($requireForeground -and [VaultNative]::GetForegroundWindow() -ne [IntPtr]$c.NativeWindowHandle) { throw 'focus_changed' }
}
# Closing a modal window does not synchronously restore focus to its owner.
# Poll only the already tracked pair; never activate a window or accept overwrite.
function Close-TrackedSaveAfterConfirmation($dialog,$valuePattern,$dest,$dialogKey,$dialogHandle,$dialogOwner,$confirmationKey,$confirmationHandle) {
 $elapsed=[Diagnostics.Stopwatch]::StartNew()
 while ($elapsed.ElapsedMilliseconds -lt 500) {
  Assert-Window
  $saved=$dialog.Current
  if ((Node-Key $dialog) -cne $dialogKey -or $saved.NativeWindowHandle -ne $dialogHandle -or $saved.ProcessId -ne $script:ProcessId -or $saved.ClassName -cne '#32770' -or $saved.Name -cnotin $script:SaveDialogNames -or [VaultNative]::GetWindow([IntPtr]$dialogHandle,4) -ne $dialogOwner) { throw 'save_dialog_unsupported' }
  if ($valuePattern.Current.Value -cne $dest) { throw 'invalid_destination' }
  $windows=@(Get-LineDialogs)
  $remaining=@($windows | Where-Object { (Node-Key $_) -ceq $dialogKey -and $_.Current.NativeWindowHandle -eq $dialogHandle })
  $extra=@($windows | Where-Object { (Node-Key $_) -cne $dialogKey -or $_.Current.NativeWindowHandle -ne $dialogHandle })
  if ($remaining.Count -ne 1 -or $extra.Count -gt 1) { throw 'save_dialog_unsupported' }
  if ($extra.Count -eq 1) {
   if ((Node-Key $extra[0]) -cne $confirmationKey -or $extra[0].Current.NativeWindowHandle -ne $confirmationHandle) { throw 'save_dialog_unsupported' }
   Assert-OwnedSaveConfirmation $extra[0] $dialog $dialogKey $dialogHandle $false
  }
  $foreground=[VaultNative]::GetForegroundWindow()
  # Zero is a permitted transient focus gap, not permission to close anything.
  if ($foreground -ne [IntPtr]::Zero -and $foreground -ne [IntPtr]$dialogHandle -and $foreground -ne [IntPtr]$confirmationHandle) { throw 'focus_changed' }
  if ($extra.Count -eq 0 -and $foreground -eq [IntPtr]$dialogHandle) {
   $closeSave=$dialog.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
   Assert-Window
   Assert-SaveDialog $dialog
   if ((Node-Key $dialog) -cne $dialogKey -or $dialog.Current.NativeWindowHandle -ne $dialogHandle -or [VaultNative]::GetWindow([IntPtr]$dialogHandle,4) -ne $dialogOwner -or $valuePattern.Current.Value -cne $dest) { throw 'save_dialog_unsupported' }
   try { $closeSave.Close() } catch { throw 'save_conflict_cleanup_required' }
   return
  }
  Start-Sleep -Milliseconds 20
 }
 throw 'save_conflict_cleanup_required'
}
function Wait-SaveCompletion($dialog,$valuePattern,$dest,$dialogKey,$dialogHandle) {
 $deadline=[DateTime]::UtcNow.AddSeconds(15)
 while ([DateTime]::UtcNow -lt $deadline) {
  Assert-Window
  $windows=@(Get-LineDialogs)
  $remaining=@($windows | Where-Object { (Node-Key $_) -ceq $dialogKey -and $_.Current.NativeWindowHandle -eq $dialogHandle })
  $extra=@($windows | Where-Object { (Node-Key $_) -cne $dialogKey -or $_.Current.NativeWindowHandle -ne $dialogHandle })
  if ($extra.Count -ne 0) {
   if ($extra.Count -ne 1 -or $remaining.Count -ne 1) { throw 'save_dialog_unsupported' }
   $confirmation=$extra[0]
   Assert-OwnedSaveConfirmation $confirmation $dialog $dialogKey $dialogHandle
   if ($valuePattern.Current.Value -cne $dest) { throw 'invalid_destination' }
   $close=$confirmation.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
   Assert-OwnedSaveConfirmation $confirmation $dialog $dialogKey $dialogHandle
   $confirmationKey=Node-Key $confirmation
   $confirmationHandle=$confirmation.Current.NativeWindowHandle
   $dialogOwner=[VaultNative]::GetWindow([IntPtr]$dialogHandle,4)
   if ($valuePattern.Current.Value -cne $dest) { throw 'invalid_destination' }
   # Close means cancel; never invoke Yes/Replace, remove or replace a target file.
   try { $close.Close() } catch { throw 'save_conflict_cleanup_required' }
   Close-TrackedSaveAfterConfirmation $dialog $valuePattern $dest $dialogKey $dialogHandle $dialogOwner $confirmationKey $confirmationHandle
   throw 'save_conflict'
  }
  # Native caller separately waits for file stability and checks source size/hash.
  if ($remaining.Count -eq 0) { return }
  Start-Sleep -Milliseconds 100
 }
 # A timeout does not authorize closing an unrecognized or user-changed window.
 throw 'save_incomplete'
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

# A running process can have no main window (for example, system tray only).
# Keep selection separate from publisher verification so both failure modes are precise.
function Get-LineMainProcess {
 $processes=@(Get-Process -Name LINE -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 })
 if ($processes.Count -eq 0) { throw 'line_window_missing' }
 if ($processes.Count -gt 1) { throw 'line_window_ambiguous' }
 return $processes[0]
}

while ($null -ne ($line=[Console]::ReadLine())) {
 try {
  if ($line.Length -gt 131072) { throw 'request_limit' }
  $r=ConvertFrom-Json $line
  $result=$null
  switch ($r.action) {
   'connect' {
    $p=Get-LineMainProcess
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
    $dialogKey=Node-Key $dialog;$dialogHandle=$dialog.Current.NativeWindowHandle
    $invoke.Invoke()
    Wait-SaveCompletion $dialog $valuePattern $dest $dialogKey $dialogHandle
    $result=$true
   }
   'close' { exit 0 }
   default { throw 'unknown_action' }
  }
  @{ok=$true;result=$result} | ConvertTo-Json -Depth 20 -Compress | ForEach-Object { [Console]::WriteLine($_) }
 } catch {
  # Never return exception text, which can contain private paths or UI contents.
  $allowed=@('not_connected','wrong_process','wrong_window','focus_changed','stale_node','tree_limit','request_limit','line_window_missing','line_window_ambiguous','publisher_unverified','selection_unsupported','scroll_unsupported','navigation_not_allowed','download_action_not_allowed','invalid_destination','linked_destination','save_dialog_missing','save_dialog_unsupported','save_button_unsupported','save_conflict','save_conflict_cleanup_required','save_incomplete','unknown_action')
  $code=if($_.Exception.Message -in $allowed){$_.Exception.Message}else{'uia_operation_failed'}
  @{ok=$false;code=$code} | ConvertTo-Json -Compress | ForEach-Object { [Console]::WriteLine($_) }
 }
}
)ATLAS";
}
