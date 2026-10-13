#include "web_page.h"

const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<title>Lego Control Center</title>
<style>
:root{
  --bg:#0d0f12; --panel:#141820; --panel2:#1b2029; --border:#2a3140;
  --blue:#3b82f6; --blue-dk:#2563eb; --emerald:#10b981; --rose:#f43f5e; --amber:#f59e0b; --cyan:#22d3ee;
  --text:#e2e8f0; --muted:#94a3b8;
  --track:#1b2029; --svg-fill:#1b2029; --svg-stroke:#2a3140; --svg-deep:#0d0f12;
  --sh:180px; --T:64px;
}
[data-theme="light"]{
  --bg:#e8eef5; --panel:#ffffff; --panel2:#eef2f7; --border:#c5d0de;
  --blue:#2563eb; --blue-dk:#1d4ed8; --emerald:#059669; --rose:#e11d48; --amber:#d97706; --cyan:#0891b2;
  --text:#0f172a; --muted:#64748b;
  --track:#dbe3ee; --svg-fill:#dbe3ee; --svg-stroke:#94a3b8; --svg-deep:#cbd5e1;
}
*{box-sizing:border-box}
html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none;touch-action:none;
  background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  user-select:none;-webkit-user-select:none}
body{padding:max(env(safe-area-inset-top,0px),6px) 10px max(env(safe-area-inset-bottom,0px),6px);
  display:flex;justify-content:center}
.wrap{width:100%;max-width:1000px;height:100%;display:flex;flex-direction:column;gap:6px}

.header{flex:none;display:flex;justify-content:space-between;align-items:center;gap:8px;
  height:50px;min-height:50px;max-height:50px;box-sizing:border-box;
  background:var(--panel);border:1px solid var(--border);padding:6px 10px;border-radius:14px}
.brand-info{height:36px;display:flex;flex-direction:column;justify-content:center;box-sizing:border-box}
.brand-monitor{height:36px;min-height:0;max-height:none;box-sizing:border-box;flex:0 1 min(230px,27vw)}
.brand{display:flex;align-items:center;gap:8px;min-width:0}
.brand-icon{width:28px;height:28px;border-radius:8px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;color:var(--blue);flex-shrink:0;padding:0;cursor:pointer}
.brand-title{font-size:11px;font-weight:800;line-height:1;letter-spacing:.025em;white-space:nowrap}
.brand-info{min-width:0}
.brand-monitor{max-width:min(230px,27vw);min-width:125px;display:flex;align-items:center;
  overflow:hidden;background:var(--panel2);border:1px solid var(--border);border-radius:8px;padding:4px 7px;}
.brand-monitor code{display:block;max-width:100%;font:9px/1.25 ui-monospace,SFMono-Regular,Consolas,monospace;color:var(--cyan);white-space:normal;overflow-wrap:anywhere;}
.brand-sub{display:flex;align-items:center;gap:5px;font-size:9px;color:var(--muted);margin-top:2px}
.brand-ver{font-size:9px;color:var(--muted);opacity:.7;margin-top:1px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--emerald);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.mode-switch{display:flex;gap:3px;background:var(--panel2);border:1px solid var(--border);border-radius:11px;padding:3px;flex-shrink:0}
.mode-btn{border:none;background:transparent;color:var(--muted);font-size:11px;font-weight:600;
  padding:6px 10px;border-radius:8px;cursor:pointer;white-space:nowrap}
.mode-btn.active{background:var(--blue-dk);color:#fff;box-shadow:0 0 12px rgba(59,130,246,.45)}
.badge{font-size:9px;font-weight:700;padding:3px 7px;border-radius:7px;white-space:nowrap;
  background:rgba(16,185,129,.12);color:var(--emerald);border:1px solid rgba(16,185,129,.3)}
.icon-btn{width:30px;height:30px;border-radius:9px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);display:flex;align-items:center;justify-content:center;cursor:pointer;font-size:13px;flex-shrink:0}
.hstats{display:flex;align-items:center;gap:10px}
.stat{display:flex;align-items:center;gap:5px;font-size:10px;font-weight:600;white-space:nowrap}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px}
.bars i{display:block;width:3px;border-radius:1px;background:var(--border)}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}
.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:14px}
.bars.w1 i.on{background:var(--rose)}.bars.w2 i.on{background:var(--amber)}
.bars.w3 i.on,.bars.w4 i.on{background:var(--emerald)}
.bat{position:relative;width:24px;height:12px;border:2px solid var(--muted);border-radius:3px;padding:1px}
.bat::after{content:"";position:absolute;right:-4px;top:2px;width:2px;height:5px;background:var(--muted);border-radius:0 1px 1px 0}
.bat-fill{height:100%;width:70%;background:var(--emerald);border-radius:1px}
.bat.warn .bat-fill{background:var(--amber)}.bat.crit .bat-fill{background:var(--rose)}

.panel{flex:1;min-height:0;display:flex;align-items:center;justify-content:center;
  background:var(--panel);border:1px solid var(--border);border-radius:18px;padding:10px;overflow:hidden}
.cluster{display:flex;align-items:center;justify-content:center;gap:clamp(14px,4vw,40px);height:100%;width:100%}
.col{display:flex;flex-direction:column;align-items:center;gap:3px;height:100%;justify-content:center}
.lbl{font-size:9px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--muted);white-space:nowrap}
.val{font-size:12px;font-weight:700;font-family:ui-monospace,monospace;color:var(--blue)}
.val.g{color:var(--emerald)}.val.c{color:var(--cyan)}.val.a{color:var(--amber)}

input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;touch-action:pan-y;margin:0}
input[type=range]::-webkit-slider-runnable-track{
  height:22px;cursor:pointer;background:#1b2029;border-radius:12px;border:1px solid var(--border)}
[data-theme="light"] input[type=range]::-webkit-slider-runnable-track{background:#dbe3ee}
input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;width:32px;height:60px;margin-top:-19px;
  border-radius:12px;background:var(--blue);cursor:pointer;border:2px solid #fff;
  box-shadow:0 0 14px rgba(59,130,246,.8)}
input[type=range]:disabled::-webkit-slider-thumb{background:#475569;box-shadow:none;border-color:#64748b}
input[type=range]:disabled::-webkit-slider-runnable-track{opacity:.4}
input.slim::-webkit-slider-runnable-track{height:14px}
input.slim::-webkit-slider-thumb{width:22px;height:40px;margin-top:-13px;border-radius:10px}

.slider-v-container{height:var(--sh);width:var(--T);display:flex;align-items:center;justify-content:center;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
[data-theme="light"] .slider-v-container{background:#e2e8f0;box-shadow:inset 0 2px 6px rgba(0,0,0,.08)}
.slider-v{flex:none;width:var(--sh)!important;height:var(--T)!important;transform:rotate(-90deg);transform-origin:center}
.slider-h-container{width:100%;height:var(--T);display:flex;align-items:center;padding:0 8px;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
[data-theme="light"] .slider-h-container{background:#e2e8f0;box-shadow:inset 0 2px 6px rgba(0,0,0,.08)}
.slider-h-container input{width:100%}

.slider-v-container input.slider-v::-webkit-slider-thumb{
  width:36px;height:90px;margin-top:-34px;border-radius:14px}
.slider-v-container .slider-v{height:110px!important}

.slim-container { position: relative; min-height: 52px; margin-top: 4px; }
.slim-wrap {
  height: 52px; display: flex; align-items: center; padding: 0 8px;
  background: rgba(2,6,15,.7); border-radius: 12px; border: 1px solid var(--border);
  box-shadow: inset 0 2px 8px rgba(0,0,0,.4);
}
[data-theme="light"] .slim-wrap { background:#e2e8f0; box-shadow: inset 0 2px 6px rgba(0,0,0,.08); }
.slim-wrap input { width: 100%; }
.slim-container > .slim-wrap {
  position: absolute; top: 0; left: 0; right: 0; height: 52px;
  opacity: 1; transform: scale(1);
  transition: opacity 0.35s ease, transform 0.35s ease; pointer-events: auto;
}
.slim-container > .slim-wrap.hidden { opacity: 0; transform: scale(0.95); pointer-events: none; }
.eng-label {
  position: absolute; top: 0; left: 0; right: 0; height: 52px;
  display: flex; align-items: center; justify-content: center;
  font-size: 13px; font-weight: 800; letter-spacing: .04em; text-transform: uppercase;
  color: var(--amber); text-align: center;
  background: rgba(245,158,11,.08); border-radius: 12px; border: 1px solid rgba(245,158,11,.35);
  opacity: 0; transform: scale(0.95);
  transition: opacity 0.35s ease, transform 0.35s ease; pointer-events: none;
}
.eng-label.visible { opacity: 1; transform: scale(1); pointer-events: auto; }
#classic-panel:not(.motor-count-4) #eng-label{writing-mode:horizontal-tb;width:100%;height:52px;letter-spacing:.04em;font-size:13px;}

.tank-svg{height:calc(var(--sh) - 8px);width:auto;aspect-ratio:240/280;max-height:100%}
.car-svg{height:min(calc(var(--sh) + 56px), 246px);width:auto;aspect-ratio:256/427;max-height:100%;flex-shrink:1;display:block}
.layered-car-svg .motor-overlay{opacity:0;transition:opacity .22s ease}
.layered-car-svg .vehicle-light{transition:opacity .15s ease,filter .15s ease}
.layered-car-svg use{transition:filter .12s ease}
.layered-car-svg .wheel-tint{mix-blend-mode:screen;transition:opacity .12s ease,fill .12s ease;pointer-events:none}
.car-wrap{position:relative;display:flex;flex-direction:column;align-items:center}
.btn-row{display:flex;gap:8px;margin-top:6px;flex-shrink:0}

#tank-panel .cluster{justify-content:space-evenly;width:100%;padding:0 8px}
#tank-panel .slider-v-container{width:168px}
#classic-panel .cluster{display:grid;grid-template-columns:1.15fr .85fr 1.15fr;align-items:center;width:100%}
#classic-panel .col:first-child{justify-self:center}
#classic-panel .slider-v-container{width:160px}
#classic-panel .side{width:min(340px,36vw)}
/* Конфигуратор количества моторов — меняет только раскладку классического режима. */
#classic-panel.motor-count-1 .side-top, #classic-panel.motor-count-1 #eng-btn, #classic-panel.motor-count-1 #aux-mode-btn{display:none!important}
#classic-panel.motor-count-1 .cluster{grid-template-columns:1.15fr .85fr 1.15fr}
/* При одном моторе приборы остаются на прежнем месте и показывают состояние A. */
#classic-panel.motor-count-1 .dash{display:flex!important}
#classic-panel.motor-count-4 .slim-container{height:var(--sh);min-height:0;width:100%;margin-top:2px;box-sizing:border-box}
#classic-panel.motor-count-2 #field-C, #classic-panel.motor-count-2 #field-D,
#classic-panel.motor-count-3 #field-D{display:none!important}
#classic-panel.motor-count-2 #eng-btn, #classic-panel.motor-count-2 #aux-mode-btn,
#classic-panel.motor-count-3 #eng-btn, #classic-panel.motor-count-3 #aux-mode-btn,
#classic-panel.motor-count-4 #eng-btn, #classic-panel.motor-count-4 #aux-mode-btn{display:flex}
#classic-panel.motor-count-2 .side-top.open{justify-content:center}
#classic-panel.motor-count-2 #field-B{flex:1;max-width:100%}
#classic-panel.motor-count-4 .side-top.open{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));align-items:stretch;gap:6px;overflow:hidden;padding:0 2px;box-sizing:border-box}
#classic-panel.motor-count-4 .side-top .field{min-width:0;width:100%;flex:none;align-items:stretch;gap:2px;padding:5px 3px;box-sizing:border-box;overflow:hidden}
#classic-panel.motor-count-4 .side-top .row{width:100%;min-width:0;justify-content:space-between;gap:2px;font-size:9px}
#classic-panel.motor-count-4 .side-top .row label{font-size:9px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#classic-panel.motor-count-4 .slim-container{margin-top:2px}
#classic-panel.motor-count-4 .val{font-size:9px}
#classic-panel.motor-count-4 .slim-wrap{width:100%;height:var(--sh);padding:0;justify-content:center;overflow:hidden;box-sizing:border-box}
#classic-panel.motor-count-4 .slim-wrap input{width:calc(var(--sh) - 12px)!important;height:var(--T)!important;max-width:calc(var(--sh) - 12px);transform:rotate(-90deg);transform-origin:center;flex:none}
#classic-panel.motor-count-4 #eng-label{width:100%;height:var(--sh);writing-mode:vertical-rl;text-orientation:mixed;white-space:nowrap;transform:none;letter-spacing:0;font-size:12px;line-height:1;box-sizing:border-box;padding:3px;overflow:hidden}
#classic-panel.motor-count-4 #eng-label.visible{transform:none}
#classic-panel.motor-count-4 #field-B .slim-container{width:100%;height:var(--sh)!important;min-height:var(--sh)!important;max-height:var(--sh)!important;flex:0 0 var(--sh);margin-top:0!important;box-sizing:border-box}
#classic-panel.motor-count-4 #field-B .slim-container>.slim-wrap,
#classic-panel.motor-count-4 #field-B #eng-label{top:0;bottom:0;height:100%!important;min-height:0;max-height:100%;box-sizing:border-box}
#classic-panel.motor-count-4 #field-C .slim-wrap,
#classic-panel.motor-count-4 #field-D .slim-wrap{height:var(--sh)!important;min-height:var(--sh)!important;max-height:var(--sh)!important;flex:0 0 var(--sh);margin:0;box-sizing:border-box}
#classic-panel.motor-count-4 #field-B .slim-wrap.hidden{display:flex!important;opacity:0}
#classic-panel.motor-count-4 #field-B .eng-label{background:rgba(245,158,11,.18);border-color:#f59e0b;color:#fbbf24}
/* В четырёхмоторном режиме не меняем сетку классической панели:
   газ A, машина и правая колонка остаются на тех же координатах. */
#classic-panel.motor-count-4 .side-stack{flex:1}
#classic-panel.motor-count-4 .side-top.open{opacity:1;transform:none;pointer-events:auto}
#classic-panel.motor-count-4 .dash.hide{opacity:0}
#classic-panel.motor-count-4 .val{font-size:10px}
/* У B/C/D одинаковые габариты вертикальных слайдеров; короткий запас
   не даёт треку и ползунку выходить за рамку контейнера. */
#classic-panel.motor-count-4 #field-B .slim-wrap,
#classic-panel.motor-count-4 #field-C .slim-wrap,
#classic-panel.motor-count-4 #field-D .slim-wrap{width:100%;height:var(--sh)!important;min-height:var(--sh)!important;max-height:var(--sh)!important;padding:0;overflow:hidden;box-sizing:border-box}
#classic-panel.motor-count-4 #field-B .slim-wrap input,
#classic-panel.motor-count-4 #field-C .slim-wrap input,
#classic-panel.motor-count-4 #field-D .slim-wrap input{width:calc(var(--sh) - 12px)!important;height:var(--T)!important;max-width:calc(var(--sh) - 12px);transform:rotate(-90deg) scale(.72,.765);transform-origin:center;flex:none}
/* В режиме A+B убираем только оформление карточки B, не сам контейнер
   и не слой анимации приборной панели. */
#classic-panel.motor-count-2 #field-B{background:transparent;border-color:transparent;box-shadow:none}
/* Кнопки опускаются к уровню горизонтального слайдера серво; машинка немного крупнее. */
#classic-panel .cluster > .col:nth-child(2){justify-content:space-between;min-height:0}
#classic-panel .car-wrap{flex:1;min-height:0;width:100%;justify-content:center}
#classic-panel .car-svg{height:calc(var(--sh) + 12px);max-height:100%;}
#classic-panel .btn-row{margin-top:0;margin-bottom:18px;flex-shrink:0}
#classic-panel.motor-count-4 .btn-row{margin-bottom:18px}
#test-panel .motors-group{flex-wrap:nowrap}
#test-panel .motors-group .col{min-width:48px}
#test-panel .motors-group .col[data-motor="D"]{display:flex}
/* Высота основного слайдера A не уменьшается отдельно в конфигурации из четырёх моторов. */

#test-panel .cluster{display:flex;align-items:center;justify-content:space-around;width:100%;height:100%;gap:clamp(16px,4vw,40px)}
#test-panel .motors-group{display:flex;align-items:center;justify-content:center;gap:clamp(24px,5vw,48px)}
#test-panel .servo-group{display:flex;flex-direction:column;align-items:center;justify-content:center;width:min(380px,40vw);gap:16px}

.side-stack { position: relative; flex: 1 1 auto; min-height: 0; width: 100%; overflow: hidden; }
.dash{
  position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; gap: 10px;
  opacity: 1; transform: translateY(0);
  transition: transform 0.42s cubic-bezier(0.22, 0.9, 0.3, 1), opacity 0.35s ease; pointer-events: auto;
}
.dash.hide{ opacity: 0; transform: translateY(-110%); pointer-events: none; }
.side-top{
  position: absolute; inset: 0; display: flex; flex-direction: column; gap: 6px; justify-content: flex-start;
  opacity: 0; transform: translateY(110%);
  transition: transform 0.42s cubic-bezier(0.22, 0.9, 0.3, 1), opacity 0.35s ease; pointer-events: none;
}
.side-top.open{ opacity: 1; transform: translateY(0); pointer-events: auto; }
.side-top .field{flex:1;display:flex;flex-direction:column;justify-content:center}

.gauge{display:flex;flex-direction:column;align-items:center;width:46%}
.gauge svg{width:100%;height:auto;max-height:118px}
.dead-btn{opacity:.38;pointer-events:none;font-weight:800;font-size:14px;color:var(--muted)}

.aux-btn{
  width:44px;height:44px;border-radius:14px;
  background:rgba(20,24,32,.85);border:1.5px solid #2a3140;
  color:#64748b;cursor:pointer;
  display:flex;align-items:center;justify-content:center;
  transition:border-color .28s ease, box-shadow .28s ease, color .28s ease, background .28s ease, transform .18s ease;
  font-size:20px;line-height:1;padding:0;
}
.aux-btn:active{transform:scale(.92)}
[data-theme="light"] .aux-btn{background:#eef2f7;border-color:#c5d0de;color:#94a3b8}
.aux-btn .ico{font-size:20px;line-height:1;filter:grayscale(.3) opacity(.75)}
.aux-btn.active .ico{filter:none;opacity:1}

.led-btn[data-ch="F"].active{ border-color:#eab308;color:#eab308; box-shadow:0 0 14px rgba(234,179,8,.5), inset 0 0 8px rgba(234,179,8,.1); }
.led-btn[data-ch="R"].active{ border-color:#f43f5e;color:#f43f5e; box-shadow:0 0 14px rgba(244,63,94,.5), inset 0 0 8px rgba(244,63,94,.1); }
.reset-btn:active{ border-color:var(--rose);color:var(--rose); box-shadow:0 0 14px rgba(244,63,94,.5); }
.eng-btn.active{ border-color:#f59e0b;color:#f59e0b; box-shadow:0 0 12px rgba(245,158,11,.45), inset 0 0 8px rgba(245,158,11,.08); }
.aux-mode-btn.active{ border-color:#22d3ee;color:#22d3ee; box-shadow:0 0 12px rgba(34,211,238,.4), inset 0 0 8px rgba(34,211,238,.08); }

.rev-btn{margin-top:4px;padding:4px 8px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--muted);font-size:9px;font-weight:700;cursor:pointer;white-space:nowrap}
.rev-btn.active{background:rgba(59,130,246,.15);border-color:var(--blue);color:var(--blue)}

.hold-btn{padding:4px 8px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--muted);font-size:9px;font-weight:700;cursor:pointer;white-space:nowrap}
.hold-btn.active{background:rgba(16,185,129,.15);border-color:var(--emerald);color:var(--emerald)}

.side{ width:min(260px,28vw);align-self:stretch; display:flex;flex-direction:column;gap:6px; min-height:0; }
.side-bottom{flex:0 0 auto;margin-top:auto}
.field{background:var(--panel2);padding:8px;border-radius:12px;border:1px solid var(--border)}
.field label{font-size:11px;font-weight:600;color:var(--text)}
.field .row{display:flex;align-items:center;justify-content:space-between;margin-bottom:4px}

.mini-btn{width:28px;height:28px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);font-size:14px;cursor:pointer;padding:0}
.sheet .mini-btn{width:52px;height:48px;font-size:22px;border-radius:12px}
.sheet .rev-btn, .sheet .hold-btn{min-width:64px;min-height:44px;font-size:14px;padding:8px 12px;border-radius:12px}
.sheet .num{width:72px;height:44px;font-size:16px}
.num{width:48px;height:28px;background:var(--panel);color:var(--text);border:1px solid var(--border);border-radius:7px;padding:2px;text-align:center;font-weight:700;font-size:12px}

.sheet select {
  width: 100%; padding: 8px; background: var(--panel); color: var(--text);
  border: 1px solid var(--border); border-radius: 8px; font-size: 13px; font-weight: 600; outline: none;
}

.hidden{display:none!important}
.wheel{transform-box:fill-box;transform-origin:center;transition:transform .08s ease-out}

#t-left-body, #t-right-body, #t-left-f, #t-left-r, #t-right-f, #t-right-r, #w-rl, #w-rr, #c-fwd, #c-rev { transition: stroke 0.36s ease, fill 0.36s ease, filter 0.36s ease; }
#t-left-body.active-light, #t-right-body.active-light, #t-left-f.active-light, #t-left-r.active-light, #t-right-f.active-light, #t-right-r.active-light, #w-rl.active-light, #w-rr.active-light, #c-fwd.active-light, #c-rev.active-light { transition: stroke 0.18s ease, fill 0.18s ease, filter 0.18s ease; }

#car-body { transition: stroke 1.00s cubic-bezier(.22,.9,.3,1), stroke-width 1.00s cubic-bezier(.22,.9,.3,1), filter 1.00s cubic-bezier(.22,.9,.3,1); }
#car-body.eng-glow { transition: stroke 0.50s cubic-bezier(.22,.9,.3,1), stroke-width 0.50s cubic-bezier(.22,.9,.3,1), filter 0.50s cubic-bezier(.22,.9,.3,1); }

.sparks { pointer-events:none; opacity: 0; transition: opacity 1.00s ease; }
.sparks.on { opacity: 1; transition: opacity 0.50s ease; }

#motors-icon { transition: opacity 0.40s ease; }
#motor-b-group, #motor-c-group, #motor-d-group {
  transition: opacity 0.40s ease, transform 0.40s cubic-bezier(0.22, 0.9, 0.3, 1);
  transform-box: view-box;
  transform-origin: 120px 100px;
}

@keyframes boltPulse{0%,100%{opacity:.55}50%{opacity:1}}
.bolt-glow{filter:drop-shadow(0 0 5px #22d3ee)}
.bolt-pulse{animation:boltPulse 1s ease-in-out infinite}
@keyframes sparkFly{0%{transform:translate(0,0) scale(1);opacity:1}100%{transform:translate(var(--sx),var(--sy)) scale(.2);opacity:0}}
.spark{position:absolute;width:3px;height:3px;border-radius:50%;background:var(--amber);
  box-shadow:0 0 6px 2px rgba(245,158,11,.8);animation:sparkFly .9s ease-out infinite}

.footer{flex:none;background:var(--panel);border:1px solid var(--border);border-radius:12px;
  padding:5px 10px;display:flex;justify-content:space-between;align-items:center}
.footer span:first-child{font-size:9px;color:var(--muted)}
.footer code{font-size:10px;color:var(--cyan);background:var(--panel2);padding:2px 7px;border-radius:5px;border:1px solid var(--border)}

.overlay{display:none;position:fixed;inset:0;z-index:50;background:var(--bg);
  flex-direction:column;align-items:center;justify-content:center;padding:24px;text-align:center}
@media (orientation:portrait){.overlay{display:flex}}
.overlay-icon{width:64px;height:64px;margin-bottom:16px;border-radius:16px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;font-size:28px}
.overlay h2{font-size:16px;margin:0 0 6px}
.overlay p{font-size:12px;color:var(--muted);max-width:260px;margin:0 0 18px;line-height:1.45}
.fs-btn{padding:12px 20px;border-radius:14px;background:var(--blue-dk);color:#fff;border:none;
  font-weight:700;font-size:13px;cursor:pointer;box-shadow:0 0 16px rgba(59,130,246,.4)}

.sbg{position:fixed;inset:0;z-index:40;background:rgba(0,0,0,.34);backdrop-filter:blur(0);-webkit-backdrop-filter:blur(0);opacity:0;pointer-events:none;transition:opacity .34s ease,backdrop-filter .34s ease,-webkit-backdrop-filter .34s ease}
.sbg.open{opacity:1;pointer-events:auto;backdrop-filter:blur(5px);-webkit-backdrop-filter:blur(5px)}
.sheet{position:fixed;top:0;right:0;bottom:0;z-index:41;width:min(300px,85vw);
  background:var(--panel);border-left:1px solid var(--border); transform:translateX(105%);
  transition:transform .34s cubic-bezier(.22,.9,.3,1); display:flex;flex-direction:column;overflow:hidden;touch-action:pan-y;
  will-change:transform;box-shadow:-12px 0 32px rgba(0,0,0,.35)}
.sheet.open{transform:translateX(0)}
.sheet-h{flex:0 0 auto;display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border-bottom:1px solid var(--border)}
.sheet-b{flex:1 1 auto;min-height:0;overflow-y:auto;-webkit-overflow-scrolling:touch;padding:10px 12px 24px;
  display:flex;flex-direction:column;gap:8px;touch-action:pan-y}
.block{background:var(--panel2);border:1px solid var(--border);border-radius:10px;padding:10px;flex-shrink:0}
.block h3{font-size:10px;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin:0 0 8px}
.line{display:flex;align-items:center;gap:6px;margin-bottom:6px;font-size:11px;font-weight:600;color:var(--muted)}
.line:last-child{margin-bottom:0}
.grow{flex:1}
.svg-body{fill:var(--svg-fill);stroke:var(--svg-stroke)}
.svg-deep{fill:var(--svg-deep);stroke:var(--svg-stroke)}
/* Корпуса дополнительных моторов повторяют контур машины; каналы различаются только молниями. */
#b-motor-body,#c-motor-body,#d-motor-body{fill:none;stroke:var(--svg-stroke);stroke-width:3px}

/* Скрываем внешние рамки карточек для конфигураций с тремя и четырьмя моторами. */
#classic-panel.motor-count-3 #field-B,
#classic-panel.motor-count-3 #field-C,
#classic-panel.motor-count-4 #field-B,
#classic-panel.motor-count-4 #field-C,
#classic-panel.motor-count-4 #field-D {
  background: transparent !important;
  border-color: transparent !important;
  box-shadow: none !important;
}

/* Тёмные рамки слайдеров B/C/D в конфигурации из четырёх моторов:
   на 15% уже и на 20% короче, с сохранением исходной геометрии колонок. */
#classic-panel.motor-count-4 #field-B .slim-container > .slim-wrap,
#classic-panel.motor-count-4 #field-C .slim-wrap,
#classic-panel.motor-count-4 #field-D .slim-wrap {
  width: 85% !important;
  height: calc(var(--sh) * .8) !important;
  min-height: calc(var(--sh) * .8) !important;
  max-height: calc(var(--sh) * .8) !important;
  padding: 0;
  box-sizing: border-box;
  align-self: center;
}

/* B находится в абсолютном контейнере; центрируем рамку внутри него. */
#classic-panel.motor-count-4 #field-B .slim-container > .slim-wrap {
  top: 10% !important;
  bottom: auto !important;
  left: 7.5% !important;
  right: 7.5% !important;
  width: auto !important;
  margin: 0 !important;
}

/* Баннер имитации сохраняет размер и положение рамки B, чтобы переключение
   между слайдером и имитацией не меняло геометрию панели. */
#classic-panel.motor-count-4 #field-B #eng-label {
  top: 10% !important;
  bottom: auto !important;
  left: 7.5% !important;
  right: 7.5% !important;
  width: auto !important;
  height: 80% !important;
  min-height: 0 !important;
  max-height: 80% !important;
  box-sizing: border-box;
}

/* Ручки увеличены на 10% только в четырёхмоторном режиме. */
#classic-panel.motor-count-4 #field-B input.slim::-webkit-slider-thumb,
#classic-panel.motor-count-4 #field-C input.slim::-webkit-slider-thumb,
#classic-panel.motor-count-4 #field-D input.slim::-webkit-slider-thumb {
  width: 30.25px;
  height: 44px;
  margin-top: -15px;
  border-radius: 11px;
}

/* Контейнер B согласован по высоте с C/D, чтобы подпись и процент
   оставались над укороченной тёмной рамкой. */
#classic-panel.motor-count-4 #field-B .slim-container {
  height: calc(var(--sh) * .8) !important;
  min-height: calc(var(--sh) * .8) !important;
  max-height: calc(var(--sh) * .8) !important;
  flex: 0 0 calc(var(--sh) * .8) !important;
  margin-top: 0 !important;
  box-sizing: border-box;
}
#classic-panel.motor-count-4 #field-B .slim-container > .slim-wrap,
#classic-panel.motor-count-4 #field-B #eng-label {
  top: 0 !important;
  bottom: 0 !important;
  height: 100% !important;
  min-height: 0 !important;
  max-height: 100% !important;
  box-sizing: border-box;
}

</style>
</head>
<body>

<div class="overlay" id="rotate-overlay">
  <div class="overlay-icon">📱</div>
  <h2>Поверните устройство</h2>
  <p>Управление доступно только в горизонтальной ориентации. Поверните устройство.</p>
  <button class="fs-btn" onclick="toggleFullscreen()">⛶ Включить полный экран</button>
</div>

<div class="wrap">
  <div class="header">
    <div class="brand">
      <button class="brand-icon" id="brand-toggle" type="button" title="Показать монитор порта" aria-label="Переключить заголовок и монитор порта" aria-expanded="false" aria-controls="brand-monitor">
        <svg viewBox="0 0 32 32" width="24" height="24" role="img" aria-label="LEGO Technic ESP32">
          <g fill="none" stroke-linecap="round" stroke-linejoin="round">
            <path d="M9 3H23L29 9V23L23 29H9L3 23V9Z" stroke="#E2E8F0" stroke-width="2"/>
            <path d="M10 9H22V23H10Z" stroke="#E2E8F0" stroke-width="1.8"/>
            <path d="M7 7L10 10M25 7L22 10M7 25L10 22M25 25L22 22" stroke="#EF4444" stroke-width="2.4"/>
            <path d="M12 14.5Q16 10.5 20 14.5M13.8 17Q16 14.8 18.2 17" stroke="#38BDF8" stroke-width="2"/>
          </g>
          <circle cx="16" cy="19.5" r="1.5" fill="#38BDF8"/>
        </svg>
      </button>
      <div class="brand-info" id="brand-info">
        <div class="brand-title">LEGO TECHNIC ESP32</div>
        <div class="brand-sub"><span class="dot"></span><span id="ip-addr">192.168.4.1</span></div>
        <div class="brand-ver">0.5.27 · beta</div>
      </div>
      <div class="brand-monitor hidden" id="brand-monitor" aria-live="polite">
        <code id="telemetry">{"ch":"—","val":0}</code>
      </div>
    </div>
    <div class="mode-switch">
      <button id="btn-tank" class="mode-btn active" type="button">🛡 Танковый</button>
      <button id="btn-classic" class="mode-btn" type="button">🚗 Классический</button>
      <button id="btn-test" class="mode-btn" type="button">🔧 Тестовый</button>
    </div>
    <div style="display:flex;align-items:center;gap:8px;flex-shrink:0">
      <div class="hstats">
        <div class="stat">
          <div class="bars w0" id="wifi-bars"><i></i><i></i><i></i><i></i></div>
          <span id="wifi-txt">--</span>
        </div>
        <div class="stat">
          <div class="bat" id="bat-ic"><div class="bat-fill" id="bat-fill" style="width:0%"></div></div>
          <span id="batt-txt">--% · --V</span>
        </div>
      </div>
      <button class="icon-btn" type="button" onclick="toggleFullscreen()" title="Полный экран">⛶</button>
      <button class="icon-btn" type="button" id="themeBtn" title="Тема">🌙</button>
      <button class="icon-btn" type="button" id="setBtn" title="Настройки">⚙</button>
      <span class="badge" id="ws-status">ПОДКЛЮЧЕНИЕ</span>
    </div>
  </div>

  <main id="tank-panel" class="panel">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Левая гусеница</span>
        <span class="val" id="val-left">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-left" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
      <div class="col">
        <svg class="tank-svg" viewBox="0 0 240 280">
          <rect x="64" y="30" width="112" height="220" rx="20" class="svg-body" stroke-width="3"/>
          <rect x="80" y="52" width="80" height="176" rx="12" class="svg-deep" stroke-width="2"/>
          <rect x="112" y="24" width="16" height="96" rx="5" fill="var(--svg-stroke)" stroke="var(--blue)" stroke-width="1.5" opacity=".7"/>
          <circle cx="120" cy="150" r="34" class="svg-body" stroke="var(--blue)" stroke-width="2.5" opacity=".9"/>
          <g id="t-left-group">
            <rect id="t-left-body" x="6" y="14" width="62" height="252" rx="14" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="3"/>
            <path d="M 6 32 H 68 M 6 52 H 68 M 6 72 H 68 M 6 92 H 68 M 6 112 H 68 M 6 132 H 68 M 6 152 H 68 M 6 172 H 68 M 6 192 H 68 M 6 212 H 68 M 6 232 H 68 M 6 248 H 68" stroke="var(--svg-stroke)" stroke-width="2" stroke-linecap="round"/>
            <polygon id="t-left-f" points="37,42 18,72 56,72" fill="var(--svg-stroke)"/>
            <polygon id="t-left-r" points="37,238 18,208 56,208" fill="var(--svg-stroke)"/>
          </g>
          <g id="t-right-group">
            <rect id="t-right-body" x="172" y="14" width="62" height="252" rx="14" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="3"/>
            <path d="M 172 32 H 234 M 172 52 H 234 M 172 72 H 234 M 172 92 H 234 M 172 112 H 234 M 172 132 H 234 M 172 152 H 234 M 172 172 H 234 M 172 192 H 234 M 172 212 H 234 M 172 232 H 234 M 172 248 H 234" stroke="var(--svg-stroke)" stroke-width="2" stroke-linecap="round"/>
            <polygon id="t-right-f" points="203,42 184,72 222,72" fill="var(--svg-stroke)"/>
            <polygon id="t-right-r" points="203,238 184,208 222,208" fill="var(--svg-stroke)"/>
          </g>
        </svg>
        <div class="btn-row">
          <button class="aux-btn dead-btn" type="button" disabled>1</button>
          <button class="aux-btn dead-btn" type="button" disabled>2</button>
          <button class="aux-btn dead-btn" type="button" disabled>3</button>
          <button class="aux-btn dead-btn" type="button" disabled>4</button>
        </div>
      </div>
      <div class="col">
        <span class="lbl">Правая гусеница</span>
        <span class="val" id="val-right">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-right" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
    </div>
  </main>

  <main id="classic-panel" class="panel hidden">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Газ (A)</span>
        <span class="val g" id="val-drive">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-drive" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
      <div class="col">
        <div class="car-wrap">
          <div class="sparks" id="sparks" style="position:absolute;inset:0">
            <i class="spark" style="left:30%;top:40%;--sx:-18px;--sy:-22px;animation-delay:0s"></i>
            <i class="spark" style="left:70%;top:35%;--sx:16px;--sy:-20px;animation-delay:.15s"></i>
            <i class="spark" style="left:45%;top:55%;--sx:-12px;--sy:18px;animation-delay:.3s"></i>
            <i class="spark" style="left:60%;top:50%;--sx:14px;--sy:16px;animation-delay:.45s"></i>
            <i class="spark" style="left:35%;top:28%;--sx:-8px;--sy:-24px;animation-delay:.6s"></i>
            <i class="spark" style="left:55%;top:70%;--sx:10px;--sy:20px;animation-delay:.75s"></i>
          </div>
          <svg class="car-svg layered-car-svg" viewBox="-2 0 256 427" xmlns="http://www.w3.org/2000/svg">
            <defs>
              <image id="wheel-art" href="data:image/webp;base64,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" x="0" y="0" width="58" height="103" preserveAspectRatio="none"/>
              <mask id="wheel-alpha-mask" maskUnits="userSpaceOnUse" x="-29" y="-51.5" width="58" height="103" style="mask-type:alpha">
                <use href="#wheel-art" x="-29" y="-51.5"/>
              </mask>
            </defs>

            <!-- Колёса находятся под шасси; наружные края чуть выступают по бокам. -->
            <g id="c-fl" class="wheel" transform="translate(28,125) rotate(0)">
              <use id="w-fl" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-fl" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>
            <g id="c-fr" class="wheel" transform="translate(224,125) rotate(0) scale(-1,1)">
              <use id="w-fr" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-fr" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>
            <g id="c-rl" transform="translate(28,315)">
              <use id="w-rl" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-rl" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>
            <g id="c-rr" transform="translate(224,315) scale(-1,1)>
              <use id="w-rr" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-rr" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>

            <!-- Кузов поверх колёс, чтобы колёса визуально уходили под шасси. -->
            <image id="car-body" href="data:image/webp;base64,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" x="0" y="0" width="252" height="427" preserveAspectRatio="none"/>

            <!-- Прозрачные контурные пиктограммы моторов, без залитого фона. -->
            <g id="motors-icon" pointer-events="none">
              <g id="motor-b-group" class="motor-overlay" opacity="0" transform="translate(126 88)">
                <polygon id="b-al" points="-62,0 -46,-18 -46,18" fill="var(--svg-stroke)"/>
                <rect id="b-motor-body" x="-38" y="-38" width="76" height="76" rx="10" class="motor-body" stroke-width="3" fill="none"/>
                <polygon id="b-bolt" points="10,-28 -14,2 -3,2 -10,28 17,-7 4,-7" fill="var(--svg-stroke)"/>
                <polygon id="b-ar" points="62,0 46,-18 46,18" fill="var(--svg-stroke)"/>
              </g>
              <g id="motor-c-group" class="motor-overlay" opacity="0" transform="translate(126 212)">
                <polygon id="c-al" points="-62,0 -46,-18 -46,18" fill="var(--svg-stroke)"/>
                <rect id="c-motor-body" x="-38" y="-38" width="76" height="76" rx="10" class="motor-body" stroke-width="3" fill="none"/>
                <polygon id="c-bolt" points="10,-28 -14,2 -3,2 -10,28 17,-7 4,-7" fill="var(--svg-stroke)"/>
                <polygon id="c-ar" points="62,0 46,-18 46,18" fill="var(--svg-stroke)"/>
              </g>
              <g id="motor-d-group" class="motor-overlay" opacity="0" transform="translate(126 340)">
                <polygon id="d-al" points="-62,0 -46,-18 -46,18" fill="var(--svg-stroke)"/>
                <rect id="d-motor-body" x="-38" y="-38" width="76" height="76" rx="10" class="motor-body" stroke-width="3" fill="none"/>
                <polygon id="d-bolt" points="10,-28 -14,2 -3,2 -10,28 17,-7 4,-7" fill="var(--svg-stroke)"/>
                <polygon id="d-ar" points="62,0 46,-18 46,18" fill="var(--svg-stroke)"/>
              </g>
            </g>

            <circle id="hl-fl" class="vehicle-light" cx="72" cy="26" r="7" fill="#3a4252" opacity="0"/>
            <circle id="hl-fr" class="vehicle-light" cx="180" cy="26" r="7" fill="#3a4252" opacity="0"/>
            <circle id="hl-rl" class="vehicle-light" cx="42" cy="404" r="8" fill="#3a4252" opacity="0"/>
            <circle id="hl-rr" class="vehicle-light" cx="210" cy="404" r="8" fill="#3a4252" opacity="0"/>
          </svg>
        </div>
        <div class="btn-row">
          <button class="aux-btn led-btn" data-ch="F" type="button" title="Передние фары"><span class="ico">💡</span></button>
          <button class="aux-btn led-btn" data-ch="R" type="button" title="Задние фонари"><span class="ico">🛑</span></button>
          <button class="aux-btn eng-btn" id="eng-btn" type="button" title="Имитация ДВС → мотор B"><span class="ico">⚙️</span></button>
          <button class="aux-btn aux-mode-btn" id="aux-mode-btn" type="button" title="Доп. моторы B/C/D"><span class="ico">🎛️</span></button>
        </div>
      </div>
      <div class="side">
        <div class="side-stack">
          <div class="dash" id="dash">
            <div class="gauge">
              <svg viewBox="0 0 120 100">
                <path d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="var(--border)" stroke-width="8" stroke-linecap="round"/>
                <path id="rpm-arc" d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="#f59e0b" stroke-width="8" stroke-linecap="round" stroke-dasharray="0 200"/>
                <g id="rpm-ticks"></g>
                <line id="rpm-needle" x1="60" y1="78" x2="60" y2="40" stroke="var(--text)" stroke-width="2.5" stroke-linecap="round"/>
                <circle cx="60" cy="78" r="4" fill="#f59e0b"/>
                <text x="60" y="94" text-anchor="middle" fill="var(--muted)" font-size="9" font-weight="700">×1000</text>
              </svg>
            </div>
            <div class="gauge">
              <svg viewBox="0 0 120 100">
                <path d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="var(--border)" stroke-width="8" stroke-linecap="round"/>
                <path id="spd-arc" d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="#22d3ee" stroke-width="8" stroke-linecap="round" stroke-dasharray="0 200"/>
                <line id="spd-needle" x1="60" y1="78" x2="60" y2="40" stroke="var(--text)" stroke-width="2.5" stroke-linecap="round"/>
                <circle cx="60" cy="78" r="4" fill="#22d3ee"/>
                <text x="60" y="94" text-anchor="middle" fill="var(--muted)" font-size="9" font-weight="700">м/с</text>
              </svg>
            </div>
          </div>
          <div class="side-top" id="aux-panel">
            <div class="field" id="field-B">
              <div class="row"><label>Мотор B</label><span class="val c" id="val-aux">0%</span></div>
              <div class="slim-container">
                <div class="slim-wrap" id="wrap-aux">
                  <input type="range" class="slim" id="slider-aux" min="-100" max="100" value="0" style="width:100%">
                </div>
                <div class="eng-label" id="eng-label">Имитация ДВС</div>
              </div>
            </div>
            <div class="field" id="field-C">
              <div class="row"><label>Мотор C</label><span class="val a" id="val-C">0%</span></div>
              <div class="slim-wrap">
                <input type="range" class="slim" id="slider-C" min="-100" max="100" value="0" style="width:100%">
              </div>
            </div>
            <div class="field" id="field-D">
              <div class="row"><label>Мотор D</label><span class="val" id="val-D">0%</span></div>
              <div class="slim-wrap">
                <input type="range" class="slim" id="slider-D" min="-100" max="100" value="0" style="width:100%">
              </div>
            </div>
          </div>
        </div>
        <div class="side-bottom">
          <div class="row" style="display:flex;justify-content:space-between;margin-bottom:4px">
            <span class="lbl">Руль (Servo)</span><span class="val" id="val-steer">0%</span>
          </div>
          <div class="slider-h-container">
            <input type="range" id="slider-steer" min="-100" max="100" value="0">
          </div>
        </div>
      </div>
    </div>
  </main>

  <main id="test-panel" class="panel hidden">
    <div class="cluster">
      <div class="motors-group">
        <div class="col">
          <span class="lbl">Мотор A</span>
          <span class="val g" id="val-test-A">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-A" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
        <div class="col">
          <span class="lbl">Мотор B</span>
          <span class="val c" id="val-test-B">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-B" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
        <div class="col">
          <span class="lbl">Мотор C</span>
          <span class="val a" id="val-test-C">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-C" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
        <div class="col" data-motor="D">
          <span class="lbl">Мотор D</span>
          <span class="val" id="val-test-D">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-D" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
      </div>
      <div class="servo-group">
        <div class="btn-row" style="margin-top:0; justify-content:center; gap:12px">
          <button class="aux-btn led-btn" data-ch="F" type="button" title="Передние фары"><span class="ico">💡</span></button>
          <button class="aux-btn led-btn" data-ch="R" type="button" title="Задние фонари"><span class="ico">🛑</span></button>
          <button class="aux-btn reset-btn" id="test-reset-btn" type="button" title="Сбросить всё в 0"><span class="ico">🔄</span></button>
        </div>
        <div style="width:100%">
          <div class="row" style="display:flex;justify-content:space-between;margin-bottom:4px">
            <span class="lbl">Серво (Servo)</span><span class="val" id="val-test-S">0%</span>
          </div>
          <div class="slider-h-container">
            <input type="range" id="slider-test-S" min="-100" max="100" value="0">
          </div>
        </div>
      </div>
    </div>
  </main>

</div>

<div class="sbg" id="sbg"></div>
<aside class="sheet" id="sheet">
  <div class="sheet-h">
    <b style="font-size:13px">Настройки</b>
    <button class="icon-btn" type="button" id="sheetX">✕</button>
  </div>
  <div class="sheet-b" id="sheetBody">
    <div class="block">
      <h3>Классический режим</h3>
      <div class="line" style="flex-direction:column;align-items:stretch;gap:8px">
        <label for="motor-count-select" style="font-size:12px;color:var(--text)">Количество моторов</label>
        <select id="motor-count-select">
          <option value="1">1 мотор — A</option>
          <option value="2">2 мотора — A + B</option>
          <option value="3" selected>3 мотора — A + B + C</option>
          <option value="4">4 мотора — A + B + C + D</option>
        </select>
        <div style="font-size:10px;line-height:1.45">Настройка влияет на классический интерфейс. В тестовом режиме доступны все каналы, танковый режим не меняется.</div>
      </div>
    </div>
    <div class="block">
      <h3>Виброотклик (Haptic)</h3>
      <div class="line" style="flex-direction:column; align-items:stretch; gap:8px;">
        <select id="haptic-select">
          <option value="all">Вся вибрация (кнопки + джойстики)</option>
          <option value="buttons">Только кнопки</option>
          <option value="none">Выключена</option>
        </select>
      </div>
    </div>
    <div class="block">
      <h3>Удержание (Cruise Control)</h3>
      <div class="line"><span>Мотор A</span><span class="grow"></span><button class="hold-btn" data-ch="A" type="button">ВЫКЛ</button></div>
      <div class="line"><span>Мотор B</span><span class="grow"></span><button class="hold-btn" data-ch="B" type="button">ВЫКЛ</button></div>
      <div class="line"><span>Мотор C</span><span class="grow"></span><button class="hold-btn" data-ch="C" type="button">ВЫКЛ</button></div>
      <div class="line"><span>Мотор D</span><span class="grow"></span><button class="hold-btn" data-ch="D" type="button">ВЫКЛ</button></div>
    </div>
    <div class="block">
      <h3>Калибровка руля</h3>
      <div class="line">
        <span>Центр</span><span class="grow"></span>
        <button class="mini-btn" type="button" id="trimM">−</button>
        <span class="val" id="val-trim">0</span>
        <button class="mini-btn" type="button" id="trimP">+</button>
      </div>
      <div class="line">
        <span>Макс.°</span><span class="grow"></span>
        <input class="num" id="maxdeg-input" type="number" min="5" max="90" value="45">
      </div>
    </div>
    <div class="block">
      <h3>Реверс моторов</h3>
      <div class="line"><span>Канал A</span><span class="grow"></span><button class="rev-btn" data-ch="A" type="button">⇄</button></div>
      <div class="line"><span>Канал B</span><span class="grow"></span><button class="rev-btn" data-ch="B" type="button">⇄</button></div>
      <div class="line"><span>Канал C</span><span class="grow"></span><button class="rev-btn" data-ch="C" type="button">⇄</button></div>
      <div class="line"><span>Канал D</span><span class="grow"></span><button class="rev-btn" data-ch="D" type="button">⇄</button></div>
    </div>
    <div class="block">
      <h3>Кнопки</h3>
      <div style="font-size:13px;color:var(--text);line-height:1.85">
        <div>💡 <b style="color:#eab308">Свет</b></div>
        <div>🛑 <b style="color:#f43f5e">Стоп</b></div>
        <div>⚙️ <b style="color:var(--amber)">Имитация ДВС</b></div>
        <div>🎛️ <b style="color:var(--cyan)">Доп. моторы B/C/D</b></div>
      </div>
    </div>
  </div>
</aside>

<script>
(function(){
  function $(id){ return document.getElementById(id); }
  var ws = null;

  // Нажатие на кубик переключает шапку между названием устройства и последними TX-данными.
  var brandToggle = $('brand-toggle');
  if (brandToggle) {
    brandToggle.addEventListener('click', function(){
      var monitor = $('brand-monitor');
      var info = $('brand-info');
      if (!monitor || !info) return;
      var showMonitor = monitor.classList.contains('hidden');
      monitor.classList.toggle('hidden', !showMonitor);
      info.classList.toggle('hidden', showMonitor);
      brandToggle.setAttribute('aria-expanded', String(showMonitor));
      brandToggle.title = showMonitor ? 'Вернуться к заголовку' : 'Показать монитор порта';
    });
  }

  // --- Единый источник правды для heartbeat: последние отправленные значения A/B/C/S ---
  var lastSent = {A:0, B:0, C:0, D:0, S:0};

  function txWS(obj){
    $('telemetry').textContent = JSON.stringify(obj);
    if (obj.ch && lastSent.hasOwnProperty(obj.ch)) lastSent[obj.ch] = obj.val;
    if(ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
  }

  var mode=0, rev={A:0,B:0,C:0,D:0}, led={F:0,R:0};
  var eng=false, auxMode=false, trim=0;
  var drive=0, auxB=0, auxC=0, auxD=0, steer=0;
  var motorCount = +(localStorage.getItem("lcc_motor_count") || 3);
  
  var holdConfig = { A: false, B: false, C: false, D: false };
  var holdActive = { A: false, B: false, C: false, D: false };
  var holdTimer = { A: null, B: null, C: null, D: null };
  var holdStartVal = { A: 0, B: 0, C: 0, D: 0 };

  var hapticMode = localStorage.getItem('lcc_haptic') || 'all';
  if($('haptic-select')) $('haptic-select').value = hapticMode;

  function vibrate(pattern) {
    if (!navigator.vibrate) return;
    if (hapticMode === 'none') { navigator.vibrate(0); return; }
    navigator.vibrate(pattern);
  }
  function vibrateClick() { vibrate(15); }
  function vibrateEdge() { vibrate(30); }
  function vibrateCruise() { vibrate([40, 30, 40]); }

  function setHapticMode(val) {
    hapticMode = (val === 'all' || val === 'buttons' || val === 'none') ? val : 'all';
    localStorage.setItem('lcc_haptic', hapticMode);
    // Cancel any vibration already in progress when settings change.
    if (navigator.vibrate) navigator.vibrate(0);
    // A short confirmation is allowed for enabled modes, never for "off".
    if (hapticMode !== 'none') vibrateClick();
  }
  var hapticSelect = $('haptic-select');
  if (hapticSelect) {
    hapticSelect.addEventListener('change', function(){ setHapticMode(this.value); });
  }

  function initWS(){
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = function(){
      $('ws-status').textContent='ESP32';
      $('ws-status').style.color='var(--emerald)';

      // После любого переподключения не восстанавливаем прежние команды моторов.
      // Сначала сбрасываем интерфейс и отправляем нулевые значения каналов.
      eng = false;
      updateEngUI();
      applyEngGate();
      zeroAll();
      txWS({eng:0});
    };
    ws.onclose = function(){ 
      $('ws-status').textContent='НЕТ СВЯЗИ'; 
      $('ws-status').style.color='var(--rose)'; 
      updateWifi(0);
      setTimeout(initWS,1000); 
    };
    ws.onmessage = function(e){
      try{
        var d = JSON.parse(e.data);
        if(d.st){
          $('batt-txt').textContent = d.p + '% · ' + d.v.toFixed(1) + 'V';
          $('bat-fill').style.width = d.p + '%';
          updateWifi(d.r);
          
          if(typeof d.ra !== 'undefined') { rev.A = !!d.ra; updateRevUI('A'); }
          if(typeof d.rb !== 'undefined') { rev.B = !!d.rb; updateRevUI('B'); }
          if(typeof d.rc !== 'undefined') { rev.C = !!d.rc; updateRevUI('C'); }
          if(typeof d.rd !== 'undefined') { rev.D = !!d.rd; updateRevUI('D'); }
          if(typeof d.lf !== 'undefined') { led.F = !!d.lf; updateLedUI('F'); }
          if(typeof d.lr !== 'undefined') { led.R = !!d.lr; updateLedUI('R'); }
          if(typeof d.es !== 'undefined') { eng = !!d.es; updateEngUI(); applyEngGate(); }
          if(typeof d.tr !== 'undefined') { trim = d.tr; $('val-trim').textContent = trim; }
          if(typeof d.md !== 'undefined') { $('maxdeg-input').value = d.md; }
        }
      }catch(err){}
    };
  }

  // Heartbeat поддерживает связь с прошивкой, но не повторяет команды моторов.
  // Иначе после тайм-аута или переподключения старое значение слайдера могло бы
  // неожиданно снова запустить мотор.
  setInterval(function(){
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({hb:1}));
  }, 150);

  function updateWifi(r){
    var n = 0, txt = '--';
    if (r < 0){
      n = r > -55 ? 4 : (r > -65 ? 3 : (r > -75 ? 2 : 1));
      txt = r + ' dBm';
    }
    var bars = document.getElementById('wifi-bars');
    if(bars){
      bars.className = 'bars w' + n;
      for (var i = 0; i < 4; i++) bars.children[i].classList.toggle('on', i < n);
    }
    var wtxt = document.getElementById('wifi-txt');
    if(wtxt) wtxt.textContent = txt;
  }

  function updateRevUI(ch){
    document.querySelectorAll('.rev-btn[data-ch="'+ch+'"]').forEach(function(b){
      b.classList.toggle('active', rev[ch]);
    });
  }

  function updateLedUI(ch){
    document.querySelectorAll('.led-btn[data-ch="'+ch+'"]').forEach(function(b){
      b.classList.toggle('active', led[ch]);
    });
    paintLights();
  }

  var th=localStorage.getItem('lcc_th')||'dark';
  function setTh(t){
    th=t; document.documentElement.setAttribute('data-theme', t==='light'?'light':'dark');
    $('themeBtn').textContent=t==='light'?'☀️':'🌙';
    localStorage.setItem('lcc_th',t);
  }
  setTh(th);
  $('themeBtn').onclick=function(){ setTh(th==='dark'?'light':'dark'); };

  window.toggleFullscreen=async function(){
    try{
      if(!document.fullscreenElement){
        await document.documentElement.requestFullscreen();
        if(screen.orientation&&screen.orientation.lock) await screen.orientation.lock('landscape').catch(function(){});
      } else document.exitFullscreen();
    }catch(e){}
  };

  function fitSliders(){
    var p=document.querySelector('.panel:not(.hidden)');
    if(!p) return;
    // Высота главного слайдера A одинакова при любом количестве моторов.
    var extra = 88;
    var maxH = 220;
    document.documentElement.style.setProperty('--sh', Math.max(88, Math.min(maxH, p.clientHeight-extra))+'px');
  }
  window.addEventListener('resize', fitSliders);
  window.addEventListener('load', fitSliders);

  function applyMotorCount(count){
    motorCount = Math.max(1, Math.min(4, parseInt(count, 10) || 3));
    localStorage.setItem('lcc_motor_count', String(motorCount));
    $('motor-count-select').value = String(motorCount);
    var panel = $('classic-panel');
    [1,2,3,4].forEach(function(n){ panel.classList.toggle('motor-count-'+n, n===motorCount); });
    // При уменьшении числа моторов лишние каналы сбрасываем, чтобы старое значение не осталось активным.
    if(motorCount < 4){ auxD=0; if($('slider-D')) $('slider-D').value=0; if($('val-D')) $('val-D').textContent='0%'; txWS({ch:'D',val:0}); }
    if(motorCount < 3){ auxC=0; if($('slider-C')) $('slider-C').value=0; if($('val-C')) $('val-C').textContent='0%'; txWS({ch:'C',val:0}); }
    if(motorCount < 2){
      eng=false; auxMode=false; auxB=0;
      if($('slider-aux')) $('slider-aux').value=0;
      if($('val-aux')) $('val-aux').textContent='0%';
      txWS({eng:0});
      txWS({ch:'B',val:0});
    }
    updateEngUI(); applyEngGate(); updateAuxModeUI();
    setTimeout(fitSliders, 30);
  }
  $('motor-count-select').value = String(motorCount);
  $('motor-count-select').onchange = function(){ vibrateClick(); applyMotorCount(this.value); };

  function switchMode(m){
    vibrateClick();
    mode=m;
    $('btn-tank').classList.toggle('active', m===0);
    $('btn-classic').classList.toggle('active', m===1);
    $('btn-test').classList.toggle('active', m===2);
    
    $('tank-panel').classList.toggle('hidden', m!==0);
    $('classic-panel').classList.toggle('hidden', m!==1);
    $('test-panel').classList.toggle('hidden', m!==2);

    if(m!==1 && eng){
      eng=false;
      updateEngUI();
      applyEngGate();
      txWS({eng:0});
    }
    if(m===0){ eng=false; updateEngUI(); applyEngGate(); }
    txWS({mode: m});
    zeroAll(); setTimeout(fitSliders,30);
  }
  $('btn-tank').onclick=function(){ switchMode(0); };
  $('btn-classic').onclick=function(){ switchMode(1); };
  $('btn-test').onclick=function(){ switchMode(2); };

  function zeroAll(){
    drive=auxB=auxC=auxD=steer=0;
    ['A','B','C','D'].forEach(function(ch){ clearHoldState(ch); });
    ['slider-left','slider-right','slider-drive','slider-aux','slider-C','slider-D','slider-steer',
     'slider-test-A','slider-test-B','slider-test-C','slider-test-D','slider-test-S'].forEach(function(id){ var e=$(id); if(e) e.value=0; });
    ['val-left','val-right','val-drive','val-aux','val-C','val-D','val-steer',
     'val-test-A','val-test-B','val-test-C','val-test-D','val-test-S'].forEach(function(id){ var e=$(id); if(e) e.textContent='0%'; });

    // Фары намеренно не трогаем — привязаны к физическому выходу, не к режиму.
    txWS({ch:'A', val:0}); txWS({ch:'B', val:0}); txWS({ch:'C', val:0}); txWS({ch:'D', val:0}); txWS({ch:'S', val:0});
    paintTank(0,0); paintCar();
  }

  function toggleLightState(el, active){
    if(!el) return;
    el.classList.toggle('active-light', active);
  }

  function paintTank(L,R){
    var green='#10b981', red='#f43f5e', off='var(--svg-stroke)';
    var lf=$('t-left-f'),lr=$('t-left-r'),rf=$('t-right-f'),rr=$('t-right-r');
    if(lf){ 
      toggleLightState(lf, L>8);
      toggleLightState(lr, L<-8);
      lf.setAttribute('fill', L>8 ? green : off); 
      lr.setAttribute('fill', L<-8 ? red : off); 
    }
    if(rf){ 
      toggleLightState(rf, R>8);
      toggleLightState(rr, R<-8);
      rf.setAttribute('fill', R>8 ? green : off); 
      rr.setAttribute('fill', R<-8 ? red : off); 
    }
    var lb=$('t-left-body'),rb=$('t-right-body');
    if(lb){
      toggleLightState(lb, Math.abs(L)>8);
      lb.setAttribute('stroke', L>8 ? green : (L<-8 ? red : 'var(--svg-stroke)'));
      lb.style.filter = L>8 ? 'drop-shadow(0 0 6px #10b981)' : (L<-8 ? 'drop-shadow(0 0 6px #f43f5e)' : '');
    }
    if(rb){
      toggleLightState(rb, Math.abs(R)>8);
      rb.setAttribute('stroke', R>8 ? green : (R<-8 ? red : 'var(--svg-stroke)'));
      rb.style.filter = R>8 ? 'drop-shadow(0 0 6px #10b981)' : (R<-8 ? 'drop-shadow(0 0 6px #f43f5e)' : '');
    }
  }

  function paintCar(){
    var A=drive, S=steer, B=auxB, C=auxC;
    // Шасси является неподвижной растровой подложкой; передние колёса вращаются отдельно.
    var ang = S * 0.28;
    var fl=$('c-fl'), fr=$('c-fr');
    if(fl) fl.setAttribute('transform', 'translate(28,125) rotate('+ang+')');
    if(fr) fr.setAttribute('transform', 'translate(224,125) rotate('+ang+') scale(-1,1)');
    var wfl=$('w-fl'), wfr=$('w-fr');
    var wrl=$('w-rl'), wrr=$('w-rr');
    var tintFL=$('tint-fl'), tintFR=$('tint-fr');
    var tintRL=$('tint-rl'), tintRR=$('tint-rr');
    var activeDrive = Math.abs(A) > 8;
    var driveColor = A>8 ? '#10b981' : (A<-8 ? '#f43f5e' : 'var(--svg-stroke)');
    var driveFilter = A>8 ? 'drop-shadow(0 0 6px #10b981)' : (A<-8 ? 'drop-shadow(0 0 6px #f43f5e)' : '');

    // Колёса - изображения: stroke их не перекрашивает, поэтому используется
    // прозрачный tint, ограниченный альфа-маской самого колеса.
    [tintFL,tintFR,tintRL,tintRR].forEach(function(tint){
      if(!tint) return;
      tint.setAttribute('fill', driveColor);
      tint.setAttribute('opacity', activeDrive ? '0.42' : '0');
      tint.style.filter = driveFilter;
    });
    [wfl,wfr,wrl,wrr].forEach(function(wheelImage){
      if(wheelImage) wheelImage.style.filter = driveFilter;
    });

    var bGroup = $('motor-b-group');
    var cGroup = $('motor-c-group');
    var dGroup = $('motor-d-group');

    /* A отображается приборами.
       B связан с имитацией ДВС и скрывается при eng=true.
       Координаты слоёв B, C и D постоянны и не зависят от motorCount. */
    var showB = auxMode && motorCount >= 2 && !eng;
    var showC = auxMode && motorCount >= 3;
    var showD = auxMode && motorCount >= 4;
    // B расположен над капотом, C в районе кабины, D в багажнике.
    // Конфигурация меняет только видимость слоёв, не их координаты.
    if(bGroup) bGroup.style.opacity = showB ? '1' : '0';
    if(cGroup) cGroup.style.opacity = showC ? '1' : '0';
    if(dGroup) dGroup.style.opacity = showD ? '1' : '0';

    setMotorViz('b', eng ? engineBFromDrive() : B);
    setMotorViz('c', C);
    setMotorViz('d', auxD);
  }

  function setMotorViz(prefix, val){
    var al=$(prefix+'-al'), ar=$(prefix+'-ar'), bolt=$(prefix+'-bolt');
    if(!al) return;
    // Корпус и стрелки всех моторов повторяют стиль машины; канал окрашивается только молнией.
    var channelColor = prefix==='b' ? '#f59e0b' : (prefix==='c' ? '#3b82f6' : '#a78bfa');
    var active = Math.abs(val) > 8;
    // Направление определяется знаком команды: отрицательное — левая стрелка,
    // положительное — правая. В нейтрали обе стрелки выключены.
    [[al, val < -8], [ar, val > 8]].forEach(function(item){
      var arrow = item[0], arrowOn = active && item[1];
      if(!arrow) return;
      arrow.setAttribute('fill', arrowOn ? channelColor : 'var(--svg-stroke)');
      arrow.setAttribute('class', arrowOn ? 'bolt-glow bolt-pulse' : '');
      arrow.style.filter = arrowOn ? 'drop-shadow(0 0 5px '+channelColor+')' : '';
    });
    if(bolt){
      bolt.setAttribute('fill', active ? channelColor : 'var(--svg-stroke)');
      bolt.setAttribute('class', active ? 'bolt-glow bolt-pulse' : '');
      bolt.style.filter = active ? 'drop-shadow(0 0 5px '+channelColor+')' : '';
    }
  }

  function paintLights(){
    var yOn='#eab308', rOn='#f43f5e';
    var fl=$('hl-fl'),fr=$('hl-fr'),rl=$('hl-rl'),rr=$('hl-rr');
    if(fl){
      [fl,fr].forEach(function(lamp){
        lamp.setAttribute('fill', led.F ? yOn : 'transparent');
        lamp.setAttribute('opacity', led.F ? '0.95' : '0');
        lamp.style.filter = led.F ? 'drop-shadow(0 0 7px #eab308)' : '';
      });
    }
    if(rl){
      [rl,rr].forEach(function(lamp){
        lamp.setAttribute('fill', led.R ? rOn : 'transparent');
        lamp.setAttribute('opacity', led.R ? '0.95' : '0');
        lamp.style.filter = led.R ? 'drop-shadow(0 0 7px #f43f5e)' : '';
      });
    }
  }

  function engineBFromDrive(){ return Math.round(33+67*Math.abs(drive)/100); }

  function updateEngUI(){
    var b=$('eng-btn'); if(b) b.classList.toggle('active', eng);
    var s=$('sparks'); if(s) s.classList.toggle('on', eng);
    paintCar();
  }
  
  function applyEngGate(){
    var slB=$('slider-aux'), wrap=$('wrap-aux'), lab=$('eng-label');
    if(!slB || !wrap || !lab) return;
    
    if(eng){
      wrap.classList.add('hidden');
      lab.classList.add('visible');
      auxB=engineBFromDrive();
      slB.value=auxB; $('val-aux').textContent=auxB+'%';
    } else {
      wrap.classList.remove('hidden');
      lab.classList.remove('visible');
      slB.disabled=false;
      auxB=0; slB.value=0; $('val-aux').textContent='0%';
    }
    paintCar();
  }
  
  function updateAuxModeUI(){
    var b=$('aux-mode-btn'); if(b) b.classList.toggle('active', auxMode);
    var pan=$('aux-panel');
    if(!pan) return;
    var dash=$('dash');
    if(auxMode){
      if(dash) dash.classList.add('hide');
      pan.classList.add('open');
    } else {
      if(dash) dash.classList.remove('hide');
      pan.classList.remove('open');
      if(!eng){ auxB=0; var slB=$('slider-aux'); if(slB) slB.value=0; $('val-aux').textContent='0%'; txWS({ch:'B',val:0}); }
      auxC=0; var slC=$('slider-C'); if(slC) slC.value=0; $('val-C').textContent='0%'; txWS({ch:'C',val:0});
      auxD=0; var slD=$('slider-D'); if(slD) slD.value=0; $('val-D').textContent='0%'; txWS({ch:'D',val:0});
    }
    paintCar();
  }

  $('eng-btn').onclick=function(){
    if(motorCount<2 || mode!==1) return;
    vibrateClick();
    eng=!eng;
    updateEngUI();
    applyEngGate();
    txWS({eng: eng?1:0});
  };
  $('aux-mode-btn').onclick=function(){ vibrateClick(); auxMode=!auxMode; updateAuxModeUI(); applyEngGate(); };

  var springs = {};
  function springTo(el, target, onFrame){
    if(!el) return;
    if(springs[el.id]) cancelAnimationFrame(springs[el.id]);
    var from = +el.value;
    var t0 = performance.now();
    var dur = 220;
    function step(now){
      var k = Math.min(1, (now-t0)/dur);
      var e = 1 - Math.pow(1-k, 3);
      var v = Math.round(from + (target-from)*e);
      el.value = v;
      onFrame(v, k>=1);
      if(k<1) springs[el.id] = requestAnimationFrame(step);
      else delete springs[el.id];
    }
    springs[el.id] = requestAnimationFrame(step);
  }
  function cancelSpring(el){
    if(el && springs[el.id]){ cancelAnimationFrame(springs[el.id]); delete springs[el.id]; }
  }

  function clearHoldState(ch){
    if(holdTimer[ch]){ clearTimeout(holdTimer[ch]); holdTimer[ch] = null; }
    holdActive[ch] = false;
  }

  function handleInputHold(ch, val){
    if(!holdConfig[ch]) return;
    if(holdActive[ch]){
      holdActive[ch] = false;
    }
    if(Math.abs(val) > 50){
      if(!holdTimer[ch] || Math.abs(val - holdStartVal[ch]) > 5){
        if(holdTimer[ch]) clearTimeout(holdTimer[ch]);
        holdStartVal[ch] = val;
        holdTimer[ch] = setTimeout(function(){
          holdActive[ch] = true;
          holdTimer[ch] = null;
        }, 1500);
      }
    } else {
      if(holdTimer[ch]){ clearTimeout(holdTimer[ch]); holdTimer[ch] = null; }
    }
  }

  function bindSpring(el, ch, onFrame){
    function start(){ 
      cancelSpring(el); 
      if(ch) clearHoldState(ch);
    }
    el.addEventListener('mousedown', start);
    el.addEventListener('touchstart', start, {passive:true});
    function release(){
      if(el.disabled || mode===2) return;
      var target = (ch && holdActive[ch]) ? +el.value : 0;
      springTo(el, target, onFrame);
    }
    el.addEventListener('mouseup', release);
    el.addEventListener('touchend', release);
    el.addEventListener('touchcancel', release);
  }

  $('slider-left').oninput=function(){
    cancelSpring(this);
    var v=+this.value; 
    handleInputHold('A', v);
    $('val-left').textContent=v+'%'; txWS({ch:'A',val:v}); 
    paintTank(v,+$('slider-right').value);
    if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-left'), 'A', function(v){
    $('val-left').textContent=v+'%'; txWS({ch:'A',val:v}); 
    paintTank(v,+$('slider-right').value);
  });

  $('slider-right').oninput=function(){
    cancelSpring(this);
    var v=+this.value; 
    if(mode===0) handleInputHold('B', v);
    $('val-right').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintTank(+$('slider-left').value,v);
    if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-right'), 'B', function(v){
    $('val-right').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintTank(+$('slider-left').value,v);
  });

  $('slider-drive').oninput=function(){
    cancelSpring(this);
    drive=+this.value; 
    handleInputHold('A', drive);
    $('val-drive').textContent=drive+'%'; txWS({ch:'A',val:drive});
    if(eng){ auxB=engineBFromDrive(); $('slider-aux').value=auxB; $('val-aux').textContent=auxB+'%'; }
    paintCar();
    if ((drive === 100 || drive === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-drive'), 'A', function(v){
    drive=v; $('val-drive').textContent=v+'%'; txWS({ch:'A',val:v});
    if(eng){ auxB=engineBFromDrive(); $('slider-aux').value=auxB; $('val-aux').textContent=auxB+'%'; }
    paintCar();
  });

  $('slider-aux').oninput=function(){
    if(eng) return;
    cancelSpring(this);
    auxB=+this.value; 
    handleInputHold('B', auxB);
    $('val-aux').textContent=auxB+'%'; txWS({ch:'B',val:auxB}); 
    paintCar();
    if ((auxB === 100 || auxB === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-aux'), 'B', function(v){
    if(eng) return;
    auxB=v; $('val-aux').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintCar();
  });

  $('slider-C').oninput=function(){
    cancelSpring(this);
    auxC=+this.value; 
    handleInputHold('C', auxC);
    $('val-C').textContent=auxC+'%'; txWS({ch:'C',val:auxC}); 
    paintCar();
    if ((auxC === 100 || auxC === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-C'), 'C', function(v){
    auxC=v; $('val-C').textContent=v+'%'; txWS({ch:'C',val:v}); 
    paintCar();
  });

  $('slider-D').oninput=function(){
    cancelSpring(this);
    auxD=+this.value;
    handleInputHold('D', auxD);
    $('val-D').textContent=auxD+'%'; txWS({ch:'D',val:auxD}); paintCar();
    if ((auxD === 100 || auxD === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-D'), 'D', function(v){
    auxD=v; $('val-D').textContent=v+'%'; txWS({ch:'D',val:v}); paintCar();
  });

  $('slider-steer').oninput=function(){
    cancelSpring(this);
    steer=+this.value; $('val-steer').textContent=steer+'%'; txWS({ch:'S',val:steer}); paintCar();
    if ((steer === 100 || steer === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-steer'), null, function(v){
    steer=v; $('val-steer').textContent=v+'%'; txWS({ch:'S',val:v}); paintCar();
  });

  $('slider-test-A').oninput=function(){ var v=+this.value; $('val-test-A').textContent=v+'%'; txWS({ch:'A',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-B').oninput=function(){ var v=+this.value; $('val-test-B').textContent=v+'%'; txWS({ch:'B',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-C').oninput=function(){ var v=+this.value; $('val-test-C').textContent=v+'%'; txWS({ch:'C',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-D').oninput=function(){ var v=+this.value; $('val-test-D').textContent=v+'%'; txWS({ch:'D',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-S').oninput=function(){ var v=+this.value; $('val-test-S').textContent=v+'%'; txWS({ch:'S',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  
  $('test-reset-btn').onclick=function(){ vibrateClick(); zeroAll(); };

  document.querySelectorAll('.rev-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var ch=b.getAttribute('data-ch'); if(!ch) return;
      rev[ch]=!rev[ch];
      updateRevUI(ch);
      var obj = {}; obj['rev' + ch] = rev[ch] ? 1 : 0;
      txWS(obj);
    });
  });

  document.querySelectorAll('.hold-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var ch=b.getAttribute('data-ch'); if(!ch) return;
      holdConfig[ch] = !holdConfig[ch];
      clearHoldState(ch);
      b.classList.toggle('active', holdConfig[ch]);
      b.textContent = holdConfig[ch] ? 'ВКЛ' : 'ВЫКЛ';
      if (holdConfig[ch]) vibrateCruise();
    });
  });

  document.querySelectorAll('.led-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var c=b.getAttribute('data-ch');
      led[c]=!led[c];
      updateLedUI(c);
      var obj = {}; obj['led' + c] = led[c] ? 1 : 0;
      txWS(obj);
    });
  });

  function openS(v){ 
    vibrateClick();
    $('sheet').classList.toggle('open',v); 
    $('sbg').classList.toggle('open',v); 
  }
  $('setBtn').onclick=function(){ openS(true); };
  $('sheetX').onclick=function(){ openS(false); };
  $('sbg').onclick=function(){ openS(false); };
  $('sheetBody').addEventListener('touchmove', function(e){ e.stopPropagation(); }, {passive:true});

  $('trimM').onclick=function(){ vibrateClick(); trim=Math.max(-400,trim-50); $('val-trim').textContent=trim; txWS({trim:trim}); };
  $('trimP').onclick=function(){ vibrateClick(); trim=Math.min(400,trim+50); $('val-trim').textContent=trim; txWS({trim:trim}); };
  $('maxdeg-input').onchange=function(){ vibrateClick(); txWS({maxdeg: +this.value||45}); };

  var speedMs = 0, rpmShown = 0, moveDir = 0;
  function setNeedle(id, t){
    var el=$(id); if(!el) return;
    var ang = -90 + Math.max(0, Math.min(1, t))*180;
    el.setAttribute('transform', 'rotate('+ang+' 60 78)');
  }
  function setArc(id, t){
    var el=$(id); if(!el) return;
    var len = 132;
    var v = Math.max(0, Math.min(1, t))*len;
    el.setAttribute('stroke-dasharray', v+' '+(200-v));
  }
  setInterval(function(){
    var dir = drive>6 ? 1 : (drive<-6 ? -1 : 0);
    var braking = (dir !== 0 && moveDir !== 0 && dir !== moveDir) || (dir === 0 && (speedMs>0.15 || rpmShown>40));
    if(braking){
      speedMs += (0 - speedMs) * 0.22;
      rpmShown += (0 - rpmShown) * 0.22;
      if(speedMs < 0.2 && rpmShown < 30){ speedMs = 0; rpmShown = 0; moveDir = dir; }
    } else {
      if(dir) moveDir = dir;
      // Тахометр всегда привязан к A; имитация ДВС влияет на мотор B, а не на прибор.
      var targetRpm = 980 + (6000-980)*(Math.abs(drive)/100);
      rpmShown += (targetRpm - rpmShown) * 0.045;
      var targetSpd = (Math.abs(drive)/100)*10;
      speedMs += (targetSpd - speedMs) * 0.04;
      if(!eng && rpmShown < 8) rpmShown = 0;
      if(targetSpd===0 && speedMs<0.05) speedMs = 0;
    }
    setNeedle('rpm-needle', rpmShown/7000);
    setArc('rpm-arc', rpmShown/7000);
    setNeedle('spd-needle', speedMs/10);
    setArc('spd-arc', speedMs/10);
  }, 50);

  document.addEventListener('touchmove', function(e){
    if(e.target.closest && e.target.closest('.sheet-b')) return;
    if(e.target.tagName!=='INPUT') e.preventDefault();
  }, {passive:false});

  initWS();
  updateEngUI(); applyEngGate(); paintLights();
  applyMotorCount(motorCount);
  switchMode(0); fitSliders();
})();
</script>
</body>
</html>
)HTML";
