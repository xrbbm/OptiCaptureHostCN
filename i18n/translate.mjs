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

// Keep literal API names and settings in the UI, and localize common labels consistently.
const glossary = new Map(Object.entries({
  'apply': '应用', 'auto': '自动', 'auto apply': '自动应用', 'async': '异步',
  'balanced': '均衡', 'bilinear': '双线性', 'borderless': '无边框', 'cancel': '取消',
  'choose a game': '选择游戏', 'depth': '深度', 'focus': '焦点', 'frame generation': '帧生成',
  'frame limiter': '帧率限制器', 'front edge sync': '前沿同步', 'back edge sync': '后沿同步',
  'by': '作者：', 'hud fix': '修复 HUD', 'mod': '模组', 'n/a': '不适用',
  'game': '游戏', 'host': '捕获端', 'live status': '实时状态', 'native': '原生',
  'no fg': '关闭 FG', 'off': '关闭', 'on': '开启', 'on/off': '开/关', 'output scaling': '输出缩放',
  'overlay': '叠加层', 'performance': '性能', 'present delay': '呈现延迟',
  'present is waiting': '呈现等待中',
  'present mode': '呈现模式', 'quality': '画质', 'refresh rate': '刷新率', 'reset all': '全部重置',
  'save': '保存', 'load': '加载', 'sharpness': '锐度', 'shortcuts': '快捷键',
  'start': '启动', 'start capture': '开始捕获', 'stop': '停止', 'stop capture': '停止捕获',
  'sync interval': '同步间隔', 'to': '至', 'wait': '等待',
  'apply on start': '启动时应用', 'archive': '归档', 'enable': '启用', 'global': '全局',
  'hold ctrl, shift or alt together with the key (so it does not clash with the game).': '按住 Ctrl、Shift 或 Alt 的同时按下该按键（避免与游戏快捷键冲突）。',
  'installed': '已安装', 'no fg mod': '不使用 FG 模组', 'opti upscaler': 'Opti 升频器',
  'run the upscaler on the picture': '在画面上运行升频器',
  'source code': '源代码',
  'waiting': '等待', 'waiting for start': '等待启动',
  'the limiter will be applied every time you press start.': '每次按下 START，都会应用帧率限制。',
  'the limiter is applied only with the apply button.': '只能通过“应用”按钮启用帧率限制。',
  'tick opticapturedepth on in reshade\'s menu.': '在 ReShade 菜单中勾选 OptiCaptureDepth。',
  'download nvngx_dlssnr.dll': '下载 nvngx_dlssnr.dll',
  'could not start opticapturehost.exe (error {0}).': '无法启动 OptiCaptureHost.exe（错误 {0}）。',
  'profile {0} saved for {1} ({2}).': '已为 {1} 保存配置文件 {0}（{2}）。',
  'profile {0} of {1} is empty. press save to store the current settings in it.': '配置文件 {0}（{1}）为空。按“保存”将当前设置保存到此配置文件中。',
  'profile {0} of {1} applied (optiscaler.ini replaced).': '已应用配置文件 {0}（{1}）（已替换 OptiScaler.ini）。',
  'sets optiscaler forcevsync. auto leaves it to optiscaler and gives the lowest latency.': '设置 OptiScaler 的 ForceVsync。选择“自动”时交由 OptiScaler 控制，以获得最低延迟。',
  'the frame limiter uses rivatuner statistics server (rtss), and it was not found on this pc.': '未在此电脑上找到帧率限制器所需的 RivaTuner Statistics Server（RTSS）。',
  'install rtss (it comes with msi afterburner, or from guru3d.com), start it, then press apply again.': '安装并启动 RTSS（可随 MSI Afterburner 一同安装，或从 guru3d.com 下载），然后再次点击“应用”。',
  'rtss must be running while you play: it does the actual limiting.': '游玩时必须保持 RTSS 运行；实际的帧率限制由它执行。',
  'x2 fg mod (new gpu)': 'x2 FG 模组（新 GPU）', 'x3 fg mod (new gpu)': 'x3 FG 模组（新 GPU）',
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
}));
const terms = [
  'RivaTuner Statistics Server', 'OptiCaptureLauncher', 'OptiCaptureHost', 'Windows Graphics Capture',
  'MSI Afterburner', 'OptiScaler', 'OptiCaptureDepth', 'RivaTuner', 'ReShade', 'Direct3D', 'OpenGL', 'Vulkan',
  'ForceVsync', 'FGOutput', 'FGNvngxReplacement', 'DLSS 5', 'DLSS', 'FSR 3/4', 'FSR3/4', 'FSR 1',
  'DXGI', 'VSync', 'XeSS', 'XeLL', 'xell', 'NVIDIA', 'Nvidia', 'GitHub', 'NIS', 'NGX', 'GDI+', 'RTSS',
  'GPUs', 'GPU', 'CPU', 'AMD', 'Intel', 'FPS', 'fps', 'Hz', 'ms', 'MFG', 'FG', 'RTX', 'GTX',
  'AntiLag2', 'Ctrl', 'Shift', 'Alt', 'Esc', 'DittoSR', 'MK-JH', 'Artur', 'Nukem', 'Telegram',
  'DlssNr', 'SyncInterval', 'Enabled', 'OptiFG', 'HUDFix', 'FSRFG', 'AllowAsync', 'true', 'false', 'auto',
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
const tokens = text => text.match(protectedPattern) ?? [];

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
  const cached = oldCache.entries[id];
  const expected = source.split('\n').map(line => {
    const fixed = glossary.get(line.trim().toLowerCase());
    return fixed === undefined ? line : withEdges(line, () => fixed);
  }).join('\n');
  if (cached?.source !== source || !validTranslation(source, cached.translation) ||
      (expected !== source && cached.translation !== expected)) {
    pending.set(id, source);
  }
}

const translatedLines = new Map();
const lines = [...new Set([...pending.values()].flatMap(source => source.split('\n').map(line => line.trim())))];
const toTranslate = [];
for (const line of lines) {
  const known = glossary.get(line.toLowerCase());
  if (known !== undefined) translatedLines.set(line, known);
  else if (!hasEnglish(unprotected(line))) translatedLines.set(line, line);
  else toTranslate.push({ line, ...maskLine(line) });
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
