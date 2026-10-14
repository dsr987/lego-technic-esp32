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
.layered-car-svg{overflow:visible}
.layered-car-svg #wheel-light-overlays use{mix-blend-mode:screen;transition:opacity .12s ease;pointer-events:none}
.car-wrap{position:relative;display:flex;flex-direction:column;align-items:center}
.btn-row{display:flex;gap:8px;margin-top:6px;flex-shrink:0}

#tank-panel .cluster{justify-content:space-evenly;width:100%;padding:0 8px}
#tank-panel .slider-v-container{width:168px}
#classic-panel .cluster{display:grid;grid-template-columns:1.15fr .85fr 1.15fr;align-items:center;width:100%;overflow:visible}
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

.engine-smoke { position:absolute;inset:0;z-index:2;pointer-events:none;opacity:0;transition:opacity .45s ease; }
.engine-smoke.on { opacity:1; }
@keyframes exhaustSmoke{0%{transform:translate(0,0) scale(.38);opacity:0}12%{opacity:.72}100%{transform:translate(var(--dx),22px) scale(2.15);opacity:0}}
.smoke-puff{position:absolute;width:11px;height:9px;border-radius:50%;background:radial-gradient(ellipse at 42% 42%,rgba(148,163,184,1) 0%,rgba(148,163,184,.6) 40%,rgba(100,116,139,0) 78%);filter:blur(1px);animation:exhaustSmoke 1.8s ease-out infinite;animation-play-state:paused;will-change:transform,opacity}
.engine-smoke.on .smoke-puff{animation-play-state:running}

#motors-icon { transition: opacity 0.40s ease; }
#motor-b-group, #motor-c-group, #motor-d-group {
  transition: opacity 0.40s ease, transform 0.40s cubic-bezier(0.22, 0.9, 0.3, 1);
  transform-box: view-box;
  transform-origin: 120px 100px;
}

@keyframes boltPulse{0%,100%{opacity:.55}50%{opacity:1}}
.bolt-glow{filter:drop-shadow(0 0 5px #22d3ee)}
.bolt-pulse{animation:boltPulse 1s ease-in-out infinite}

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
#b-motor-body,#c-motor-body,#d-motor-body{fill:none;stroke:#f1f5f9;stroke-width:3.5px}

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
        <svg class="tank-svg" viewBox="0 0 240 280" role="img" aria-label="Гусеничная модель LEGO Technic">
          <image id="crawler-body" href="data:image/webp;base64,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" x="44" y="5" width="152" height="270" preserveAspectRatio="none"/>
          <g id="t-left-group">
            <image id="t-left-body" href="data:image/webp;base64,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" x="0" y="15" width="80" height="250" preserveAspectRatio="none"/>
            <polygon id="t-left-f" points="40,238 21,208 59,208" fill="var(--svg-stroke)"/>
            <polygon id="t-left-r" points="40,42 21,72 59,72" fill="var(--svg-stroke)"/>
          </g>
          <g id="t-right-group">
            <image id="t-right-body" href="data:image/webp;base64,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" x="0" y="15" width="80" height="250" transform="translate(240 0) scale(-1 1)" preserveAspectRatio="none"/>
            <polygon id="t-right-f" points="200,238 181,208 219,208" fill="var(--svg-stroke)"/>
            <polygon id="t-right-r" points="200,42 181,72 219,72" fill="var(--svg-stroke)"/>
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
          <div class="engine-smoke" id="engine-smoke" aria-hidden="true">
            <i class="smoke-puff" style="left:48%;top:95%;--dx:-18px;animation-delay:0s"></i>
            <i class="smoke-puff" style="left:49%;top:96%;--dx:-8px;animation-delay:.36s"></i>
            <i class="smoke-puff" style="left:50%;top:95%;--dx:10px;animation-delay:.72s"></i>
            <i class="smoke-puff" style="left:51%;top:96%;--dx:21px;animation-delay:1.08s"></i>
            <i class="smoke-puff" style="left:49%;top:95%;--dx:4px;animation-delay:1.44s"></i>
          </div>
          <svg class="car-svg layered-car-svg" viewBox="-2 0 256 427" xmlns="http://www.w3.org/2000/svg">
            <defs>
              <image id="wheel-art" href="data:image/webp;base64,UklGRjAJAABXRUJQVlA4WAoAAAAQAAAAPgAAbwAAQUxQSB8EAAANoHRre9pIel/JnGI6zczMtJrfzszM0MzdcQUM0rewkur2N7OPiAnAf68x9A62KGJXjsWIKWK4SS00EP9WGOfF4uLCwkJy95vhsaubcfPwmS2e+2Nrpvrxi+39pwbVeDSdjMtJK29i+cjhjYXYGhEZffPoxF5p7UJGEVe1sK8+eHrrbE5CfDN6/PPf452t3jmRVKNJ47x3+88u/D3yEu/acm7dOC/JyaPR8LXLiiLLi8W0/vLL4U6iaydfOxFBd+HKrlYEMABIEWD3O1tDJyBposGukxuf/DjdwcErz7wgTEYGswkANKuZCIIEiysrnz6WuQZ3pq3gbRLzc3Dr/p/VXMeObQv6zEMrfwznSa69dv1Cfu7v536OteNDQa/Js+O79RzHTdMzYPfuP1/NSs8/EPSc6ZV/HrkZmwfK3oFnqrvVjKNs+ofVlcfjkD3+wqP3jHY9n5HsfSr9g9n9eiSBYrnUwLXxNLSYTDVg4KrQsjTQmLvWB9ZqpyJDJR1ujLwGZqh8x6yXogFJMnGdaGmkI06nbSdeKnVEg3HTSQdKbDEN5PEEKplXrjNArSRpA4VvdSAN5U4J08YLgKx1OpA4h47zSmLfYd7o8QIgb0SL8wCQtkoYBZg0ShB7LwBjpyUSh06rJfbKIi+iysIBoHVaDEQAWK+FEAAglHlSi6ArhFYKANKrAaUjoqZLCJQS0rFOlAihC8KO8VqEAGD0+ADpoVUIAEZECSEADL2WMCn4fxYQgHgD6gh7kFBKAUAItAo79GqCAmoRsCNGGahGAgSVeJiON6AOR4YInS0tAO+sUWMCEbVYApAmMkocTaeNqKSdYY2SxrJTx1paExinkZLGdvw0UWM6qCKrQ5oIIUMVqAJSxRYqpQ6gNpEOzKgiQxUyo7YWOqo40NBSRx2qGOvwTRSYmESHzKgkMSp8EwemkhiqaENVXVjoiAL1pIhVOBdqywUdjcQBPywSFS1sqMyVSBSQMoupoTEmgO04+R+S1thQaTOjABNrQq/9oqWCZwVD5WQ5Rf/l3grC0yebef+kfbgxo/l7Y4W9w+jVwgz5ZWErZu8eungG7r48tIS+++/XOWv0w6ENsl8y+mENs92Pgz0ZevZPmcyBe4+PLoN9kvbzXZxn/PnuAyl6LHj4+wLmlW+fnN807I2g/nTRzIVX72YX1w3BkADgXBIgAOk0v/61gPnl14/s8f1LqSFI+MbFEQFxoBgSMp2gSAER31Tli38f7OIO0HzzxctoYWlhUAwW0hc/DHddWIvw8oc6qffuS93rr//1B/Yn1WS0vV3WTbRM7Li998vdYeU9YQwNxKaWzcQJolSkNSkmtQA0UZZFxBt1VTksx+NRWSfEg++fOazGrHn/bm1yA/x7tybx9kW8A+AnVSvo1pWgK4I3CQBWUDgg6gQAABAYAJ0BKj8AcAA+YSaNRaQiIRrevqhABgS0gGl32YeI2MdtRWnPsHUknRoQRu/6f1bP9bwo/YM/TT/P8BH+rxDF7Pkgree9lcG6pZlr820G3acD0r03FkV1P+0KmHO/3pVh6+jak+dRD0Tn42w2nStpmSVVo/Tc/nXNt23XSeIVPxHvKmexEmTE6KrRAhjCyFpfLq4SiQ2lmN8xUZgf54ZjU8JWRH1LsKTf9pTz6KZzeChkyzEXVf7KoD3PCaPers99hSQVfxRG4AD+/sz9+6FfyTB9nPEAvmJu/HVi1psUCDsZYXDyUZq+P3JctVcjLQy44tdHd6fH//A/HH92vyivxlfAgqHPyXXgwR/doio3abbT5FnNX7M5L9v5o3kugI6iDcMEJ/+KfxbbLyYbUYGpYZk2wIwoV4HFoMXSx4EbpQweQtdvNWf7v4yS17h9beNXic6ReHHcZimv89xxP+n1iCeU5ufMxSEMuFbRB5X7AAaicLisWCt9fOWucDzAQF7Zmn9x5+Jy7fQGHULnQ1sSqEkujmNPBU/UIOU7B9XUjI9mVyixk+ZLeVu3cZCTLp9hG7fqvyDHKNYV6cKhJNzYSvQ/KGulgB6QDodZ3cgo4AXEsEGev0WOZJVRXrvNJtw00JpqRFEd+4NR+pX/LAT9PtzsUVtqLY2G3CLx/NZbOVNq0sXzPFt1dob7qwvv+O35hUEGezhknNQ2p3dtFepF13nOrxin1d61VP4YxKvcWQz4Z/vXKnbT/Eio0McgqhwSKzQtXyXG1/jRzp90mBLHK/NrKLal4+bAo5pl/2Q4ITbXuXprMBo82bZVT0Hk72UwnC2609R0oVLapmjpbNCqMQ04ZeURMyDRqkb1Nx/l71xtsnjH+CjLhoiVGgBeXPStaoZQaVtFDld8/4cpXDhwDw7gr8332ini0g9soPKY1jMuzMn+Ji0af91A8HVaXhdsKHld2MTNZ6KzLSN6JrPhnQtiMirQgz6Zz3qbVp5OtM6GbzRG1v4CzdlsCRMwZ99yrcN4p6Be/wt0FY+fj2NVTzgQuG3IFfF6hFWdpfcoqklMDrHVrW7FQnqixe/ck10vaJvYz2evqI9eZIhubQrdpHaACGFbXGbOGZTZfpN9G6mcATHNXoeAra79YRVaSqjkUN39LjqcloGPspxjFdVMkST301+mruSY09EtRSy2Z5KpWZBKGHlLc/tYR2M81fScrLt4zcyKXd1TJ08HTEUG8NpXlr+6uaidb90OBAbSpzZbdTQSLZke122pkl9aVr7EtGqF+KxRRci5MwgKlD5QG+rK+yCRkSHotAh3FBTbI97i8d9woT+S8Y+BCC/GzIWQb0zxEOPirZr4AN14zxfFGHICoYmc/QUwdXNwYLFB/MEsSwC8t2kGXxQpYGscNgOJgcyBG4Tb9wLqwMT4A9Ldf+n2yfZDUzq7fXRXPTXjpCzxtZz4nvW7YzhSdyePFE17aI7xlO/DLNUMKkw/J1BIDfupp5KaiTOughp8edbixMmhRxlTcXQa8oSCaoqtLnkUZQVqCcjEkEtErkocSbE+50Z6ReHGG03j/1+LKHPx8TC+m4PYGHfhRN5aF3y2vd/iWKmJX+IU+aV2yv966mZSCdhmmez2uF2TlY+YA0cKv5Jg65sI3GPUWakfJ50H9ysi97tH+5EsQAA=" x="0" y="0" width="58" height="103" preserveAspectRatio="none"/>
              <filter id="wheel-tint-filter" x="-45%" y="-35%" width="190%" height="170%" color-interpolation-filters="sRGB">
                <feFlood id="wheel-tint-flood" flood-color="#10b981" flood-opacity="1" result="wheel-tint-color"/>
                <feComposite in="wheel-tint-color" in2="SourceAlpha" operator="in" result="wheel-tint-shape"/>
                <feGaussianBlur in="wheel-tint-shape" stdDeviation="4" result="wheel-tint-blur"/>
                <feMerge>
                  <feMergeNode in="wheel-tint-blur"/>
                  <feMergeNode in="wheel-tint-shape"/>
                </feMerge>
              </filter>
            </defs>

            <!-- Колёса находятся под шасси; наружные края чуть выступают по бокам. -->
            <g id="c-fl" class="wheel" transform="translate(24,125) rotate(0)">
              <use id="w-fl" href="#wheel-art" x="-29" y="-51.5"/>
            </g>
            <g id="c-fr" class="wheel" transform="translate(228,125) rotate(0) scale(-1,1)">
              <use id="w-fr" href="#wheel-art" x="-29" y="-51.5"/>
            </g>
            <g id="c-rl" transform="translate(24,315)">
              <use id="w-rl" href="#wheel-art" x="-29" y="-51.5"/>
            </g>
            <g id="c-rr" transform="translate(228,315) scale(-1,1)">
              <use id="w-rr" href="#wheel-art" x="-29" y="-51.5"/>
            </g>

            <!-- Кузов поверх колёс, чтобы колёса визуально уходили под шасси. -->
            <image id="car-body" href="data:image/webp;base64,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" x="0" y="0" width="252" height="427" preserveAspectRatio="none"/>

            <!-- Световые слои рисуются поверх кузова; цвет формируется только из альфа-силуэта шины. -->
            <g id="wheel-light-overlays" pointer-events="none">
              <g id="c-light-fl" transform="translate(24,125) rotate(0)">
                <use id="glow-fl" class="wheel-light-overlay" href="#wheel-art" x="-29" y="-51.5" filter="url(#wheel-tint-filter)" opacity="0"/>
              </g>
              <g id="c-light-fr" transform="translate(228,125) rotate(0) scale(-1,1)">
                <use id="glow-fr" class="wheel-light-overlay" href="#wheel-art" x="-29" y="-51.5" filter="url(#wheel-tint-filter)" opacity="0"/>
              </g>
              <g id="c-light-rl" transform="translate(24,315)">
                <use id="glow-rl" class="wheel-light-overlay" href="#wheel-art" x="-29" y="-51.5" filter="url(#wheel-tint-filter)" opacity="0"/>
              </g>
              <g id="c-light-rr" transform="translate(228,315) scale(-1,1)">
                <use id="glow-rr" class="wheel-light-overlay" href="#wheel-art" x="-29" y="-51.5" filter="url(#wheel-tint-filter)" opacity="0"/>
              </g>
            </g>

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

            <!-- Свет фар: широкое радиальное свечение без отдельной точки в центре. Координаты сохранены. -->
            <defs>
              <radialGradient id="front-headlight-gradient">
                <stop offset="0" stop-color="#ffffff" stop-opacity="0.98"/>
                <stop offset="0.22" stop-color="#ffffff" stop-opacity="0.88"/>
                <stop offset="0.55" stop-color="#ffffff" stop-opacity="0.38"/>
                <stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
              </radialGradient>
              <radialGradient id="rear-headlight-gradient">
                <stop offset="0" stop-color="#ff334f" stop-opacity="0.98"/>
                <stop offset="0.24" stop-color="#ff334f" stop-opacity="0.86"/>
                <stop offset="0.58" stop-color="#f43f5e" stop-opacity="0.4"/>
                <stop offset="1" stop-color="#f43f5e" stop-opacity="0"/>
              </radialGradient>
            </defs>
            <ellipse id="hl-fl" class="vehicle-light" cx="72" cy="26" rx="18" ry="13" fill="url(#front-headlight-gradient)" opacity="0"/>
            <ellipse id="hl-fr" class="vehicle-light" cx="180" cy="26" rx="18" ry="13" fill="url(#front-headlight-gradient)" opacity="0"/>
            <ellipse id="hl-rl" class="vehicle-light" cx="42" cy="404" rx="17" ry="12" fill="url(#rear-headlight-gradient)" opacity="0"/>
            <ellipse id="hl-rr" class="vehicle-light" cx="210" cy="404" rx="17" ry="12" fill="url(#rear-headlight-gradient)" opacity="0"/>
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
        <label for="drive-axle-select" style="font-size:12px;color:var(--text)">Ведущая ось для подсветки</label>
        <select id="drive-axle-select">
          <option value="rear">Задняя (RWD)</option>
          <option value="front">Передняя (FWD)</option>
          <option value="awd">Полный привод (AWD)</option>
        </select>
        <div style="font-size:10px;line-height:1.45">Меняет только свечение ведущих колёс. Управление моторами не меняется.</div>
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
  // Это настройка только визуальной подсветки, не маршрутизация команд мотора A.
  var savedDrivenAxle = localStorage.getItem('lcc_driven_axle');
  var drivenAxle = (savedDrivenAxle === 'front' || savedDrivenAxle === 'awd') ? savedDrivenAxle : 'rear';
  
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

  var driveAxleSelect = $('drive-axle-select');
  if (driveAxleSelect) {
    driveAxleSelect.value = drivenAxle;
    driveAxleSelect.addEventListener('change', function(){
      drivenAxle = (this.value === 'front' || this.value === 'awd') ? this.value : 'rear';
      localStorage.setItem('lcc_driven_axle', drivenAxle);
      vibrateClick();
      paintCar();
    });
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
      lb.style.filter = L>8
        ? 'drop-shadow(0 0 7px #10b981) brightness(1.12)'
        : (L<-8 ? 'drop-shadow(0 0 7px #f43f5e) brightness(1.12)' : '');
    }
    if(rb){
      toggleLightState(rb, Math.abs(R)>8);
      rb.style.filter = R>8
        ? 'drop-shadow(0 0 7px #10b981) brightness(1.12)'
        : (R<-8 ? 'drop-shadow(0 0 7px #f43f5e) brightness(1.12)' : '');
    }
  }

  function paintCar(){
    var A=drive, S=steer, B=auxB, C=auxC;
    // Шасси является неподвижной растровой подложкой; передние колёса вращаются отдельно.
    var ang = S * 0.28;
    var fl=$('c-fl'), fr=$('c-fr');
    if(fl) fl.setAttribute('transform', 'translate(24,125) rotate('+ang+')');
    if(fr) fr.setAttribute('transform', 'translate(228,125) rotate('+ang+') scale(-1,1)');
    // Световые копии шины лежат над кузовом, а не прячутся под ним.
    var lightFLGroup=$('c-light-fl'), lightFRGroup=$('c-light-fr');
    var glowFL=$('glow-fl'), glowFR=$('glow-fr');
    var glowRL=$('glow-rl'), glowRR=$('glow-rr');
    if(lightFLGroup) lightFLGroup.setAttribute('transform', 'translate(24,125) rotate('+ang+')');
    if(lightFRGroup) lightFRGroup.setAttribute('transform', 'translate(228,125) rotate('+ang+') scale(-1,1)');

    var activeDrive = Math.abs(A) > 8;
    // Выбранная ведущая ось управляет только визуальной подсветкой; команды моторам не меняются.
    var frontAxleGlows = activeDrive && (drivenAxle === 'front' || drivenAxle === 'awd');
    var rearAxleGlows = activeDrive && (drivenAxle === 'rear' || drivenAxle === 'awd');

    var driveColor = A>8 ? '#10b981' : (A<-8 ? '#f43f5e' : 'var(--svg-stroke)');
    var tintFlood = $('wheel-tint-flood');
    if(tintFlood) tintFlood.setAttribute('flood-color', driveColor);

    // Подсветка включается только на ведущей оси; передние колёса не светятся лишь от движения A.
    [[glowFL,frontAxleGlows],[glowFR,frontAxleGlows],
     [glowRL,rearAxleGlows],[glowRR,rearAxleGlows]].forEach(function(item){
      if(item[0]) item[0].setAttribute('opacity', item[1] ? '0.58' : '0');
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
      arrow.setAttribute('fill', arrowOn ? channelColor : '#f1f5f9');
      arrow.setAttribute('class', arrowOn ? 'bolt-glow bolt-pulse' : '');
      arrow.style.filter = arrowOn ? 'drop-shadow(0 0 5px '+channelColor+')' : '';
    });
    if(bolt){
      bolt.setAttribute('fill', active ? channelColor : '#f1f5f9');
      bolt.setAttribute('class', active ? 'bolt-glow bolt-pulse' : '');
      bolt.style.filter = active ? 'drop-shadow(0 0 5px '+channelColor+')' : '';
    }
  }

  function paintLights(){
    var fl=$('hl-fl'),fr=$('hl-fr'),rl=$('hl-rl'),rr=$('hl-rr');
    // Цвет и плавное затухание задаются radialGradient; не рисуем отдельные светящиеся точки.
    [fl,fr].forEach(function(lamp){
      if(!lamp) return;
      lamp.setAttribute('opacity', led.F ? '1' : '0');
      lamp.style.filter = led.F ? 'brightness(1.25) drop-shadow(0 0 6.25px rgba(255,255,255,.9))' : '';
    });
    [rl,rr].forEach(function(lamp){
      if(!lamp) return;
      lamp.setAttribute('opacity', led.R ? '1' : '0');
      lamp.style.filter = led.R ? 'brightness(1.25) drop-shadow(0 0 6.25px rgba(244,63,94,.9))' : '';
    });
  }

  function engineBFromDrive(){ return Math.round(33+67*Math.abs(drive)/100); }

  function updateEngUI(){
    var b=$('eng-btn'); if(b) b.classList.toggle('active', eng);
    var smoke=$('engine-smoke'); if(smoke) smoke.classList.toggle('on', eng);
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
