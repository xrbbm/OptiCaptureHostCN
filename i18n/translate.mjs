import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const MODEL = 'Xenova/opus-mt-en-zh';
const REVISION = '046f55aec303cdee3e0318604406d4df20f1e8ea';
const CACHE_VERSION = 1;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const outputDir = path.resolve(process.argv[2] ?? path.join(root, 'generated'));
const sourceFile = path.join(root, 'src/ui_strings.inc');
const cacheFile = path.join(root, 'i18n/zh-CN.json');
const cacheDir = path.resolve(process.env.MODEL_CACHE_DIR ?? path.join(root, '.cache/transformers'));

function readCatalog(text) {
  const entries = new Map();
  for (const line of text.split(/\r?\n/u)) {
    if (!line || line.startsWith('//')) continue;
    const match = /^OC_UI\(([A-Za-z][A-Za-z0-9_]*), L("(?:\\.|[^"\\])*")\)$/u.exec(line);
    if (!match) throw new Error(`Invalid catalog row: ${line}`);
    const [, id, encoded] = match;
    if (entries.has(id)) throw new Error(`Duplicate UI string ID: ${id}`);
    entries.set(id, JSON.parse(encoded));
  }
  if (entries.size === 0) throw new Error('UI catalog is empty');
  return entries;
}

// Prefer PC-gaming terms and natural wording for recurring UI copy; otherwise use the model.
const glossary = new Map(Object.entries({
  'apply': '应用', 'auto': '自动', 'auto apply': '自动应用', 'async': '异步',
  'balanced': '均衡', 'bilinear': '双线性', 'borderless': '无边框', 'cancel': '取消',
  'choose a game': '选择游戏', 'depth': '深度', 'focus': '焦点', 'frame generation': '帧生成',
  'frame limiter': '限帧', 'front edge sync': '前沿同步', 'back edge sync': '后沿同步',
  'by': '作者：', 'hud fix': '修复 HUD', 'mod': '模组', 'n/a': '不适用',
  'game': '游戏', 'host': '采集端', 'live status': '实时状态', 'native': '原生',
  'no fg': '关闭 FG', 'off': '关闭', 'on': '开启', 'on/off': '开/关', 'output scaling': '输出缩放',
  'overlay': '悬浮窗', 'performance': '性能', 'present delay': '呈现延迟',
  'present is waiting': '等待呈现', 'game looks capped': '游戏疑似被限帧',
  'present mode': '呈现模式', 'quality': '画质', 'refresh rate': '刷新率', 'reset all': '全部重置',
  'save': '保存', 'load': '加载', 'sharpness': '锐度', 'shortcuts': '快捷键',
  'start': '启动', 'start capture': '开始捕获', 'stop': '停止', 'stop capture': '停止捕获',
  'capturing': '捕获中',
  'sync interval': '同步间隔', 'to': '至', 'wait': '等待',
  'apply on start': '启动时应用', 'archive': '归档', 'enable': '启用', 'global': '全局',
  'hold ctrl, shift or alt together with the key (so it does not clash with the game).': '按住 Ctrl、Shift 或 Alt 的同时按下该按键（避免与游戏快捷键冲突）。',
  'installed': '已安装', 'no fg mod': '关闭 FG 模组', 'opti upscaler': 'Opti 超分',
  'run the upscaler on the picture': '启用画面超分',
  'source code': '源代码',
  'waiting': '等待', 'waiting for start': '等待启动',
  'the limiter will be applied every time you press start.': '每次点击“启动”都会应用限帧设置。',
  'the limiter is applied only with the apply button.': '仅在点击“应用”时更新限帧设置。',
  'tick opticapturedepth on in reshade\'s menu.': '在 ReShade 菜单中勾选 OptiCaptureDepth。',
  'download nvngx_dlssnr.dll': '下载 nvngx_dlssnr.dll',
  'could not start opticapturehost.exe (error {0}).': '无法启动 OptiCaptureHost.exe（错误 {0}）。',
  'profile {0} saved for {1} ({2}).': '已为 {1} 保存配置 {0}（{2}）。',
  'profile {0} of {1} is empty. press save to store the current settings in it.': '{1} 的配置 {0} 为空。点击“保存”以存入当前设置。',
  'profile {0} of {1} applied (optiscaler.ini replaced).': '已应用 {1} 的配置 {0}，并替换 OptiScaler.ini。',
  'sets optiscaler forcevsync. auto leaves it to optiscaler and gives the lowest latency.': '设置 OptiScaler 的 ForceVsync。选择“自动”时交由 OptiScaler 控制，以获得最低延迟。',
  'the frame limiter uses rivatuner statistics server (rtss), and it was not found on this pc.': '限帧功能需要 RivaTuner Statistics Server（RTSS），本机未找到 RTSS。',
  'install rtss (it comes with msi afterburner, or from guru3d.com), start it, then press apply again.': '安装并启动 RTSS（可随 MSI Afterburner 一起安装，或从 guru3d.com 下载），再点击“应用”。',
  'rtss must be running while you play: it does the actual limiting.': '游戏期间请保持 RTSS 运行，否则限帧不会生效。',
  'x2 fg mod (new gpu)': 'x2 FG 模组（新 GPU）', 'x3 fg mod (new gpu)': 'x3 FG 模组（新 GPU）',
  'x4 fg mod (new gpu)': 'x4 FG 模组（新 GPU）', 'x2 fg mod (old gpu)': 'x2 FG 模组（旧 GPU）',
  'x3 fg mod (old gpu)': 'x3 FG 模组（旧 GPU）', 'x4 fg mod (old gpu)': 'x4 FG 模组（旧 GPU）',
  'x2 fg · new gpu': 'x2 FG · 新 GPU', 'x3 fg · new gpu': 'x3 FG · 新 GPU', 'x4 fg · new gpu': 'x4 FG · 新 GPU',
  'x2 fg · old gpu': 'x2 FG · 旧 GPU', 'x3 fg · old gpu': 'x3 FG · 旧 GPU', 'x4 fg · old gpu': 'x4 FG · 旧 GPU',
  'on  (forcevsync = true, syncinterval = 0)': '开启（ForceVsync = true，SyncInterval = 0）',
  'on  (forcevsync = true, syncinterval = 1)': '开启（ForceVsync = true，SyncInterval = 1）',
  'off  (forcevsync = false)': '关闭（ForceVsync = false）',
  'dlss 5 enabled ([dlssnr] enabled = true).': 'DLSS 5 已启用（[DlssNr] Enabled = true）。',
  'dlss 5 disabled ([dlssnr] enabled = auto).': 'DLSS 5 已禁用（[DlssNr] Enabled = auto）。',
  'hud fix on ([optifg] hudfix = true).': 'HUD FIX 已启用（[OptiFG] HUDFix = true）。',
  'hud fix off ([optifg] hudfix = auto).': 'HUD FIX 已关闭（[OptiFG] HUDFix = auto）。',
  'async on ([fsrfg] allowasync = true).': '异步已启用（[FSRFG] AllowAsync = true）。',
  'async off ([fsrfg] allowasync = auto).': '异步已关闭（[FSRFG] AllowAsync = auto）。',
  'on: [dlssnr] enabled = true': '开启：[DlssNr] Enabled = true',
  'off: [dlssnr] enabled = auto': '关闭：[DlssNr] Enabled = auto',
  'opticapturehost  -  by mk-jh': 'OptiCaptureHost  -  作者：MK-JH',
  'intel mfg (xell/new gpu)': 'Intel MFG（xell / 新 GPU）',
  'artur mfg (old/new gpu)': 'Artur MFG（旧/新 GPU）',
  'intel fg (xell/new gpu)': 'Intel FG（xell / 新 GPU）',
  'combo mfg (old/new gpu)': 'Combo MFG（旧/新 GPU）',
  'nukem fg (old/new gpu)': 'Nukem FG（旧/新 GPU）',
  'fsr3-4 fg (old/new gpu)': 'FSR3-4 FG（旧/新 GPU）',
  'nvidia mfg (rtx only)': 'NVIDIA MFG（仅限 RTX）',
  'fsr fg (antilag2/rdna2.3.4)': 'FSR FG（AntiLag2 / rdna2.3.4）',
  'fsr fg (latency flex/old/new gpu)': 'FSR FG（Latency Flex / 旧/新 GPU）',
  'bring the launcher on top': '将启动器置顶', 'fps overlay': 'FPS 悬浮窗',
  'overlay type': '悬浮窗类型', 'show / hide it': '显示/隐藏', 'turn it on / off': '开启/关闭',
  'change what it shows': '更改显示内容', 'close the capture host': '关闭采集端',
  'limiter off.': '限帧已关闭。', 'frame limiter is off.': '限帧已关闭。',
  'frame limiter is off (limit set to 0).': '限帧已关闭（帧率上限为 0）。',
  'rtss did not take the change.': 'RTSS 未能应用更改。',
  'rtss did not accept the change.': 'RTSS 未接受更改。',
  'rtss did not take the limit.': 'RTSS 未能应用限帧设置。',
  'administrator permission was not given.': '未授予管理员权限。',
  'rtss needs administrator rights to take the limit. press apply again and allow it.': 'RTSS 需要管理员权限才能应用限帧设置。请再次点击“应用”并授权。',
  'rtss did not take the limit. is rtss running?': 'RTSS 未能应用限帧设置。RTSS 正在运行吗？',
  'pick a game in the list first (the limiter is set for its .exe).': '请先从列表中选择游戏；限帧设置会应用到该游戏的 .exe。',
  'press apply to use the limiter setting.': '点击“应用”以启用限帧设置。',
  'auto  (default, nothing forced)': '自动（默认，不强制指定）',
  'auto  (fg on: no wait, fg off: vsync)': '自动（FG 开启：不等待；FG 关闭：VSync）',
  'always no wait  (interval 0)': '始终不等待（间隔 0）',
  'always wait for vblank  (interval 1)': '始终等待 vblank（间隔 1）',
  'classic  (behaves like version 2.2.0)': '经典模式（与 2.2.0 版本行为一致）',
  'lossless-style  (low latency, tearing allowed)': '无损模式（低延迟，允许画面撕裂）',
  'classic + half refresh  (exactly refresh / 2 frames)': '经典模式 + 半刷新率（帧率为刷新率的一半）',
  'classic + unlimited  (same, no half-refresh limit)': '经典模式 + 不限帧',
  'game  (the selected game\'s .exe)': '游戏（所选游戏的 .exe）',
  'host  (opticapturehost.exe)': '采集端（OptiCaptureHost.exe）',
  'global  (every program)': '全局（所有程序）',
  'refresh rate  (auto)': '刷新率（自动）',
  '1/2 refresh rate  (auto)': '1/2 刷新率（自动）', '1/3 refresh rate  (auto)': '1/3 刷新率（自动）',
  '1/4 refresh rate  (auto)': '1/4 刷新率（自动）',
  '1/2 refresh  (auto)': '1/2 刷新率（自动）', '1/3 refresh  (auto)': '1/3 刷新率（自动）',
  '1/4 refresh  (auto)': '1/4 刷新率（自动）',
  'frame limiter  ·  rivatuner': '限帧 · RivaTuner', 'bilinear  ·': '双线性  ·',
  'depth: reshade. output scaling automatically set to dlss · quality.': '深度模式已切换为 ReShade，输出缩放自动设为 DLSS 画质档。',
  'depth: flat, as always. output scaling set to off.': '深度模式：仍使用平面深度，输出缩放已关闭。',
  'the download link for nvngx_dlssnr.dll is not set yet.': '尚未设置 nvngx_dlssnr.dll 的下载链接。',
  'multi frame generation count is for intel mfg, artur, combo and nvidia mfg.': '多帧生成倍率适用于 Intel MFG、Artur、Combo 和 NVIDIA MFG。',
  'upscaler on the picture is on (sharpness 0.6, cas, native upscaler pass). restart the game to use the change.': '已启用画面超分（锐度 0.6、CAS、原生超分处理）。重启游戏后生效。',
  'upscaler on the picture is off. restart the game to use the change.': '已关闭画面超分。重启游戏后生效。',
  'borderless cannot be used while output scaling is on.': '启用输出缩放时无法使用无边框模式。',
  'turn output scaling off first, then press borderless.': '请先关闭输出缩放，再点击“无边框”。',
  'output scaling is on': '已启用输出缩放', 'output scaling is off.': '已关闭输出缩放。',
  'quality  (1.5x': '画质档（1.5x', 'balanced  (1.7x': '均衡档（1.7x',
  'performance  (2x': '性能档（2x', 'native  (1x': '原生（1x',
  'native: the game keeps its size and fsr only smooths the picture. "run the upscaler on the picture" was turned on too.': '原生模式下，游戏保持原分辨率，FSR 仅用于平滑画面。已同时开启“启用画面超分”。',
  'start makes the game window that small and scales it up. "run the upscaler on the picture" was turned on too.': '点击“启动”会将游戏窗口缩至所选尺寸，再把画面放大。已同时开启“启用画面超分”。',
  'off  (weak gpus)': '关闭（低性能 GPU）', 'light  (cpu priority only)': '轻量（仅提高 CPU 优先级）',
  'balanced  (cpu + gpu high)': '均衡（CPU、GPU 高优先级）', 'max  (best for strong gpus)': '最高（推荐高性能 GPU）',
  'dlss 3.7  (amd and gtx 16 cards)': 'DLSS 3.7（AMD 和 GTX 16 系列显卡）',
  'invalid upscaler selection.': '无效的超分方案。', 'invalid frame generation selection.': '无效的帧生成方案。',
  'choose output scaling fsr3/4 or dlss, then start.': '将输出缩放设为 FSR3/4 或 DLSS，再点击“启动”。',
  'capture any game · fsr 4 and xess multi frame generation through optiscaler': '捕获任意游戏 · 通过 OptiScaler 使用 FSR 4 和 XeSS 多帧生成',
  'game frames (capture)': '游戏帧（捕获）', 'presented by the host': '由采集端呈现',
  'presents counted by dxgi': 'DXGI 帧呈现次数', 'time inside present': 'Present 耗时',
  '· looks capped': '· 疑似被限帧',
  'host stopped. see opticapturehost.log.': '采集端已停止。详见 OptiCaptureHost.log。',
  'the capture host stopped (code {0}). see opticapturehost.log.': '采集端已停止（错误码 {0}）。请查看 OptiCaptureHost.log。',
  'the game changed its resolution ({0}): the window was set back to {1}.': '游戏分辨率已变更为 {0}，窗口已恢复为 {1}。',
  'game window is now {0} ({1}). the host scales it to {2}.': '游戏窗口当前为 {0}（{1}），采集端会将画面缩放至 {2}。',
  'window is now {0} (wanted {1}). set the game\'s own resolution if it differs.': '当前窗口分辨率为 {0}（预期为 {1}）。如不一致，请调整游戏内分辨率。',
  '{0}: limiter, present mode and frame generation set.': '已应用预设 {0}：限帧、呈现模式和帧生成设置已更新。',
  'could not apply the saved profile of {0}. check the file and permissions.': '无法应用 {0} 的配置。请检查文件和访问权限。',
  'you saved settings (profile {0}) for {1} earlier.': '此前已为 {1} 保存配置 {0}。',
  'do you want to apply them now? this replaces the optiscaler.ini next to the launcher with the one saved for this game.': '现在要应用吗？这会用该游戏的配置覆盖启动器目录中的 OptiScaler.ini。',
  'reshade installation': 'ReShade 安装',
  'copied {0} required file(s) to:': '已将 {0} 个必需文件复制到：',
  'notes:': '注意：',
  '1. install reshade (with add-on support) on the game first; this button copies only the required files.': '1. 请先为游戏安装支持 Add-on 的 ReShade；此按钮只会复制所需文件。',
  '2. works on direct3d 11 / 12 (gpu) and direct3d 9 / 10 / opengl / vulkan (cpu).': '2. 支持 Direct3D 11/12（GPU）以及 Direct3D 9/10、OpenGL、Vulkan（CPU）。',
  '3. in-game, open the reshade menu and tick opticapturedepth on (single-player games only).': '3. 进入游戏后打开 ReShade 菜单，启用 OptiCaptureDepth（仅限单人游戏）。',
  '{0} was closed and launched again from its executable.': '{0} 已关闭，并通过其可执行文件重新启动。',
  'warning: {0} was still running; close it and launch it again.': '警告：{0} 仍在运行。请将其关闭后重新启动。',
  '{0} was closed; please launch it again.': '{0} 已关闭，请重新启动游戏。',
  'warning: {0} file(s) could not be copied.': '警告：{0} 个文件复制失败。',
  'copied {0} file(s) to {1} and restarted the game.': '已将 {0} 个文件复制到 {1}，并重新启动游戏。',
  'no window to capture. use --title "part of the window title", or switch to the game during the countdown.': '未找到可捕获的窗口。请用 --title "窗口标题的一部分" 指定窗口，或在倒计时期间切回游戏。',
  'lossless-style  (low latency)': '无损模式（低延迟）',
  'front edge sync  (default)': '前沿同步（默认）',
  'reset all settings to their defaults?': '将所有设置恢复为默认值？',
  '(frame generation, limiter, output scaling, present mode, shortcuts...)': '（帧生成、限帧、输出缩放、呈现模式、快捷键……）',
  'live status – waiting for the host…': '实时状态：等待采集端……',
  'on · 0': '开启 · 0', 'on · 1': '开启 · 1',
  'display refresh': '显示器刷新率',
  'optiscaler\'s frame limit will be set to the monitor\'s refresh rate every time the game starts.': '每次启动游戏时，都会将 OptiScaler 的帧率上限设为显示器刷新率。',
  'optiscaler\'s frame limit was set to 0 (no limit) in optiscaler.ini.': 'OptiScaler.ini 中的 OptiScaler 帧率上限已设为 0（不限帧）。',
  'for the host\'s own output scaling (fsr 3 / 4, dlss).': '用于采集端自身的输出缩放（FSR 3 / 4、DLSS）。',
  'flat  (default)': '平面深度（默认）',
  'as always: nothing changes. works in every game.': '无需改动，所有游戏都能使用。',
  'the game\'s real depth buffer, found by reshade and shared by an add-on.': 'ReShade 通过 Add-on 获取并共享游戏的真实深度缓冲区。',
  'not used': '未使用', 'the flat depth is used, as before.': '与之前一样，使用平面深度。',
  'depth  ‹': '深度  ‹', 'depth  ›': '深度  ›',
  'pick the window of the game you want to enhance': '选择要增强的游戏窗口',
  'borderless: on': '无边框：开启', 'borderless: off': '无边框：关闭',
  'light  (cpu priority)': '轻量（提高 CPU 优先级）', 'max  (strong gpus)': '最高（适合高性能 GPU）',
  'classic + half refresh': '经典模式 + 半刷新率', 'classic + unlimited': '经典模式 + 不限帧',
  'lock:': '锁定：', 'mouse + keyboard: game / overlay': '鼠标 + 键盘：游戏 / 悬浮窗',
  'stopping…': '停止中…',
  'could not read the game\'s executable path (protected process).': '无法读取游戏的可执行文件路径（游戏进程受保护）。',
  'output scaling must be fsr 3/4 or dlss (not off or nis).': '输出缩放必须设为 FSR 3/4 或 DLSS，不能设为关闭或 NIS。',
  'open "reshade depth" folder': '打开“ReShade depth”文件夹',
  'in the game, open the optiscaler menu and go to the neural rendering section to change the settings you want.': '进入游戏后，打开 OptiScaler 菜单，再进入 Neural Rendering 页面调整设置。',
  'no windows found. start the game, then press refresh.': '未找到游戏窗口。启动游戏后点击“刷新”。',
  'copied {0} file(s) to {1}.': '已将 {0} 个文件复制到 {1}。',
  'vsync: {0}. restart the game to apply.': 'VSync：{0}。重启游戏后生效。',
  'optiscaler.ini: fgoutput = {0}, fgnngxreplacement = {1}. restart the game to use the change.': 'OptiScaler.ini：FGOutput = {0}，FGNvngxReplacement = {1}。重启游戏后生效。',
  '{0} - {1} fps - {2}': '限帧：{0} · {1} fps · {2}',
  'rtss limit set: {0} at {1} fps ({2}).': 'RTSS 已将限帧设为 {0}，当前为 {1} fps（{2}）。',
  'rtss is not running - start it before the game.': 'RTSS 未运行，请在启动游戏前先启动 RTSS。',
  'windows graphics capture is not supported on this system (windows 10 1903 or newer is required).': '此系统不支持 Windows Graphics Capture（需要 Windows 10 1903 或更高版本）。',
  'initialisation failed. see opticapturehost.log next to the exe.': '初始化失败。请查看程序目录中的 OptiCaptureHost.log。',
  'unexpected error. see opticapturehost.log next to the exe.': '发生意外错误。请查看程序目录中的 OptiCaptureHost.log。',
  'ready: {0}': '就绪：{0}',
  '{0} ms   · wait {1} ms': '{0} ms   · 等待 {1} ms',
  'limiter put back after the capture.': '捕获结束后已恢复限帧设置。',
  'capture ended.': '捕获已结束。',
  'rtss needs administrator rights to write the limit - allow the windows prompt.': 'RTSS 需要管理员权限才能写入限帧设置，请在 Windows 提示中授权。',
  'that window is gone. pick the game again.': '该窗口已关闭，请重新选择游戏。',
  'that window is gone. press refresh.': '该窗口已关闭，请点击“刷新”。',
  'the game window needs administrator rights - allow the windows prompt.': '游戏窗口需要管理员权限，请在 Windows 提示中授权。',
  'could not bring the window frame back.': '无法恢复窗口边框。',
  'borderless is off: the window frame is back.': '无边框模式已关闭，窗口边框已恢复。',
  'windows did not let the launcher change that window.': 'Windows 不允许启动器修改该窗口。',
  'borderless is on. press the button again to bring the frame back.': '无边框模式已开启。再次点击按钮可恢复窗口边框。',
  'pick a game first.': '请先选择游戏。',
  'this window has no program name, so a profile cannot be saved for it.': '此窗口没有程序名称，无法为其保存配置。',
  'could not save the profile. check the folder permissions.': '无法保存配置。请检查文件夹权限。',
  'hidden warnings will be shown again.': '已重新启用警告提示。',
  'stop the host first, then reset.': '请先停止采集端，再重置设置。',
  'all settings reset to default.': '所有设置已恢复为默认值。',
  'invalid frame generation selection.': '无效的帧生成选项。',
  'saved game profiles will be applied automatically when you pick the game.': '选择游戏后会自动应用该游戏的配置。',
  'you will be asked before a saved game profile is applied.': '应用已保存的游戏配置前会先询问。',
  'always apply saved profiles automatically': '始终自动应用已保存的配置',
  'don\'t show this warning again': '不再显示此警告',
  'start anyway': '仍然启动', 'no new pictures': '暂无新帧',
  'waiting for the add-on': '等待 ReShade 加载 Add-on',
  'is reshade with opticapturedepth.addon64 running in the game?': '游戏中是否正在运行带有 OptiCaptureDepth.addon64 的 ReShade？',
  'optional  ·  needs nvngx_dlssnr.dll': '可选 · 需要 nvngx_dlssnr.dll',
  'required file': '必需文件', 'found next to the program': '已在程序目录中找到',
  'not found next to the program': '程序目录中未找到', 'dlss 5 will not work without it.': '缺少该文件时 DLSS 5 无法运行。',
  'download it and place it exactly next to the program.': '下载后将文件放在程序所在目录。',
  'dlss 5 needs version 310.9.16.': 'DLSS 5 需要 310.9.16 版本。', 'dlss 5 settings': 'DLSS 5 设置',
  'press keys…': '请按下按键……',
  'focus overlay, open optiscaler menu': '聚焦悬浮窗，打开 OptiScaler 菜单',
  'could not start gdi+.': '无法启动 GDI+。', 'could not create the window.': '无法创建窗口。',
  'already used by "{0}".': '已被“{0}”占用。',
  'created "{0}" next to the launcher. put the required files inside it and click install again.': '已在启动器旁创建“{0}”。请将所需文件放入其中，再点击“安装”。',
  'folder "{0}" is empty. put the required files inside it first.': '文件夹“{0}”为空。请先将所需文件放入其中。',
  'closing {0} completely before copying reshade files...': '复制 ReShade 文件前，正在完全关闭 {0}……',
  'capturing "{0}". {1} opens the optiscaler menu, {2} shows this window.': '正在捕获“{0}”。按 {1} 打开 OptiScaler 菜单，按 {2} 显示此窗口。',
  '{0} to {1}  ({2} · {3})': '从 {0} 到 {1}  ({2} · {3})',
}));
// Keep brand names, hardware/API acronyms and literal config values out of machine translation.
const terms = [
  'NVIDIA graphics cards', 'NVIDIA graphics card', 'Nvidia graphics cards', 'Nvidia graphics card',
  'GeForce', 'rdna2.3.4', 'vblank',
  'RivaTuner Statistics Server', 'OptiCaptureLauncher', 'OptiCaptureHost', 'Windows Graphics Capture',
  'MSI Afterburner', 'OptiScaler', 'OptiCaptureDepth', 'RivaTuner', 'ReShade', 'Direct3D', 'OpenGL', 'Vulkan',
  'ForceVsync', 'FGOutput', 'FGNvngxReplacement', 'DLSS 5', 'DLSS', 'FSR 3 / 4', 'FSR 3/4', 'FSR3/4',
  'FSR3-4', 'FSR 1', 'FSR 4', 'FSR', 'CAS', 'Neural Rendering', 'Latency Flex',
  'DXGI', 'VSync', 'XeSS', 'XeLL', 'xell', 'NVIDIA', 'Nvidia', 'GitHub', 'NIS', 'NGX', 'GDI+', 'RTSS',
  'GPUs', 'GPU', 'CPU', 'AMD', 'Intel', 'FPS', 'fps', 'Hz', 'ms', 'MFG', 'FG', 'RTX', 'GTX',
  'AntiLag2', 'Ctrl', 'Shift', 'Alt', 'Esc', 'DittoSR', 'MK-JH', 'Artur', 'Nukem', 'Telegram',
  'DlssNr', 'SyncInterval', 'Enabled', 'OptiFG', 'HUDFix', 'FSRFG', 'AllowAsync', 'true', 'false',
].sort((a, b) => b.length - a.length).map(s => s.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&'));
const protectedPattern = new RegExp(`(${[
  String.raw`\{[0-9]+\}`,
  String.raw`--[a-z][a-z0-9-]*`,
  String.raw`(?:Ctrl|Shift|Alt|Win)(?:\+[A-Za-z0-9]+){1,3}`,
  String.raw`\bF(?:[1-9]|1[0-2])\b`,
  String.raw`(?:[A-Za-z0-9_.-]+)?\.(?:exe|ini|log|dll|addon64|fx|cfg|com)\b`,
  String.raw`(?<![A-Za-z0-9_])(?:${terms.join('|')})(?![A-Za-z0-9_])`,
].join('|')})`, 'gu');
const hasEnglish = text => /[A-Za-z]/u.test(text);
const hasChinese = text => /\p{Script=Han}/u.test(text);
const unprotected = text => text.replace(protectedPattern, '');
// Chinese does not inflect GPU for the plural.
const tokens = text => (text.match(protectedPattern) ?? []).map(token => token === 'GPUs' ? 'GPU' : token);

function validTranslation(source, translation) {
  if (typeof translation !== 'string' || !translation.trim() || !translation.isWellFormed()) return false;
  if (/[\p{Cc}\u2028\u2029]/u.test(translation.replace(/[\n\t]/gu, ''))) return false;
  if (source.split('\n').length !== translation.split('\n').length) return false;
  const braces = text => (text.match(/\{[0-9]+\}|[{}]/gu) ?? []).sort().join('\0');
  if (braces(source) !== braces(translation)) return false;
  const literals = tokens(translation);
  for (const token of tokens(source)) {
    const index = literals.indexOf(token);
    if (index < 0) return false;
    literals.splice(index, 1);
  }
  return !hasEnglish(unprotected(source)) || hasChinese(translation);
}

// Give the model context around protected values. If it damages a marker, translate
// only the unprotected fragments and reinsert the actual values verbatim.
function maskLine(line) {
  const parts = line.split(protectedPattern);
  const values = parts.filter((_, index) => index % 2 === 1);
  const marker = index => `812300${517 + index}`;
  const input = parts.map((part, index) => index % 2 === 0 ? part : marker((index - 1) / 2)).join('');
  return {
    input,
    parts,
    restore(text) {
      const markers = text.match(/812300[0-9]+/gu) ?? [];
      if (markers.length !== values.length || values.some((_, index) => markers.filter(value => value === marker(index)).length !== 1)) return null;
      return text.replace(/812300([0-9]+)/gu, (_, index) => values[Number(index) - 517]);
    },
  };
}

function encodeCppString(text) {
  return `L"${text.replaceAll('\\', '\\\\').replaceAll('"', '\\"').replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}"`;
}

function withEdges(source, translate) {
  const core = source.trim();
  if (!core) return source;
  const start = source.indexOf(core);
  return source.slice(0, start) + translate(core) + source.slice(start + core.length);
}

function fixedLine(line) {
  return glossary.get(line.toLowerCase()) ?? (hasEnglish(unprotected(line)) ? undefined : line);
}

function cacheMatches(source, cached) {
  if (cached?.source !== source || !validTranslation(source, cached.translation)) return false;
  const cachedLines = cached.translation.split('\n');
  return source.split('\n').every((line, index) => {
    const fixed = fixedLine(line.trim());
    return fixed === undefined || cachedLines[index] === withEdges(line, () => fixed);
  });
}

const emptyCache = { version: CACHE_VERSION, model: MODEL, revision: REVISION, entries: {} };
const catalog = readCatalog(await fs.readFile(sourceFile, 'utf8'));
let oldCache = emptyCache;
try {
  const parsed = JSON.parse(await fs.readFile(cacheFile, 'utf8'));
  if (parsed?.version === CACHE_VERSION && parsed.model === MODEL && parsed.revision === REVISION && parsed.entries) oldCache = parsed;
} catch (error) {
  if (error.code !== 'ENOENT') throw error;
}

const pending = new Map();
for (const [id, source] of catalog) {
  if (!cacheMatches(source, oldCache.entries[id])) pending.set(id, source);
}

const translatedLines = new Map();
const lines = [...new Set([...pending.values()].flatMap(source => source.split('\n').map(line => line.trim())))];
const toTranslate = [];
for (const line of lines) {
  const fixed = fixedLine(line);
  if (fixed === undefined) toTranslate.push({ line, ...maskLine(line) });
  else translatedLines.set(line, fixed);
}

if (toTranslate.length) {
  const [{ pipeline, env }, { Converter }] = await Promise.all([import('@huggingface/transformers'), import('opencc-js')]);
  const simplify = Converter({ from: 't', to: 'cn' });
  env.allowLocalModels = false;
  env.cacheDir = cacheDir;
  const translator = await pipeline('translation', MODEL, { revision: REVISION, dtype: 'q8', device: 'cpu' });
  // Variable-length batches introduce padding artifacts in this Marian export.
  async function translate(text) {
    const [{ translation_text: result }] = await translator(text, {
      max_new_tokens: Math.min(256, Math.max(24, Math.ceil(text.length / 2) + 16)),
      no_repeat_ngram_size: 4,
    });
    return simplify(result).trim();
  }
  try {
    const retry = [];
    for (const item of toTranslate) {
      const restored = item.restore(await translate(item.input));
      if (validTranslation(item.line, restored)) translatedLines.set(item.line, restored);
      else retry.push(item);
    }

    const fragments = [...new Set(retry.flatMap(item => item.parts.filter((_, index) => index % 2 === 0).map(part => part.trim())))];
    const fragmentTranslations = new Map();
    for (const fragment of fragments) {
      const known = glossary.get(fragment.toLowerCase());
      if (known !== undefined) fragmentTranslations.set(fragment, known);
      else if (hasEnglish(fragment)) fragmentTranslations.set(fragment, await translate(fragment));
    }
    for (const item of retry) {
      translatedLines.set(item.line, item.parts.map((part, index) => index % 2 === 1 ? part :
        withEdges(part, core => fragmentTranslations.get(core) ?? core)).join(''));
    }
  } finally {
    await translator.dispose();
  }
}

const invalid = [];
const entries = {};
const include = ['// Generated by i18n/translate.mjs; do not edit.', ...[...catalog].map(([id, source]) => {
  const translation = pending.has(id)
    ? source.split('\n').map(line => withEdges(line, core => translatedLines.get(core))).join('\n')
    : oldCache.entries[id].translation;
  if (!validTranslation(source, translation)) invalid.push(`${id}: ${JSON.stringify(translation)}`);
  entries[id] = { source, translation };
  return `OC_UI(${id}, ${encodeCppString(translation)})`;
})];
if (invalid.length) throw new Error(`Invalid UI translations:\n${invalid.join('\n')}`);
await fs.mkdir(outputDir, { recursive: true });
await fs.writeFile(path.join(outputDir, 'ui_zh_CN.inc'), `${include.join('\n')}\n`, 'utf8');
await fs.writeFile(path.join(outputDir, 'zh-CN.json'), `${JSON.stringify({ ...emptyCache, entries }, null, 2)}\n`, 'utf8');
console.log(`Validated ${catalog.size} zh-CN UI strings; ${pending.size} new or changed, ${catalog.size - pending.size} reused.`);
