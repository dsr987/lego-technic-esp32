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
            <g id="c-fr" class="wheel" transform="translate(224,125) rotate(0)">
              <use id="w-fr" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-fr" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>
            <g id="c-rl" transform="translate(28,315)">
              <use id="w-rl" href="#wheel-art" x="-29" y="-51.5"/>
              <rect id="tint-rl" class="wheel-tint" x="-29" y="-51.5" width="58" height="103" rx="12" fill="#10b981" opacity="0" mask="url(#wheel-alpha-mask)" pointer-events="none"/>
            </g>
            <g id="c-rr" transform="translate(224,315)">
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
    if(fl) fl.setAttribute('transform', 'translate(35,125) rotate('+ang+')');
    if(fr) fr.setAttribute('transform', 'translate(217,125) rotate('+ang+')');
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
