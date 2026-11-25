<div align="center">

# OptiCaptureHost

### Frame generation and modern upscaling for **any** game window — now open source.
**FSR 4 · XeSS · Multi Frame Generation up to 6x · DLSS / FSR 3-4 output scaling** — without touching the game.

[![Latest release](https://img.shields.io/github/v/release/tegraultra/OptiCaptureHost?style=for-the-badge&label=download)](../../releases/latest)
[![Build](https://img.shields.io/github/actions/workflow/status/tegraultra/OptiCaptureHost/build.yml?style=for-the-badge&label=build)](../../actions)
[![Platform](https://img.shields.io/badge/Windows-10%20%2F%2011%20x64-0078d4?style=for-the-badge)](#requirements)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=for-the-badge)](LICENSE)
[![Telegram](https://img.shields.io/badge/Telegram-MKJH__APPS-26A5E4?style=for-the-badge&logo=telegram&logoColor=white)](https://t.me/MKJH_APPS)

**by MK-JH**

</div>

---

## 简体中文构建

- 在本 fork 的 [构建工作流](https://github.com/xrbbm/OptiCaptureHostCN/actions/workflows/build.yml?query=branch%3Azh-cn) 中点 `Run workflow`，选择 `zh-cn`。推送到 `zh-cn` 也会自动构建。
- 在成功运行的 **Artifacts** 下载 `OptiCaptureHost-zh-CN-win64`。其中的 ZIP 包含 Launcher、Host、许可证和本次构建的对应源代码；如需 OptiScaler 等运行时，请按下文另行安装。
- `src/ui_strings.inc` 只登记本项目的界面文案。新增或改动的条目由 Actions 的本地翻译模型处理，不需要 API Key 或人工校对。编译成功后，译文自动保存到 `i18n/zh-CN.json`。文件名、配置键、第三方菜单和日志保持不变。
- 产物的 `source/` 附带本次构建的译文；在该目录运行 `cmake -S . -B build -A x64 -DOC_ZH_CN_DIR=generated`，再运行 `cmake --build build --config Release` 可重新编译。

---

## What is this?

**Run OptiScaler in any game, app or window.** OptiCaptureHost is a window capture plus a Direct3D 12 overlay that unlocks DLSS 5, DLSS, FSR 3/4 and XeSS upscaling,
all frame generation and Multi Frame Generation (up to 6x), and every other OptiScaler feature, without touching the game.

[OptiScaler](https://github.com/janblade/OptiScaler-F5-DLSSNR-Multipass) can add frame generation and better upscaling to games, but only to games it can hook.
Many games can't be hooked: old engines, DX11-only titles, launchers with anti-tamper, emulators, video players.

**OptiCaptureHost takes a different road.** It captures the game's window with Windows Graphics Capture and shows it in its own
Direct3D 12 flip-model overlay. To OptiScaler that overlay looks like a normal modern game, so it can do its work on it.
The game itself is **never injected, patched or modified**.

```
 your game  ──(window capture)──▶  OptiCaptureHost  ──▶  D3D12 overlay  ──▶  OptiScaler  ──▶  your monitor
 (untouched)                       motion + upscaling      (flip model)      FG / FSR 4 / XeSS
```

The project has two programs:

| Program | Job |
|---|---|
| `OptiCaptureLauncher.exe` | The control panel: pick a game, profiles, frame generation, limiter, output scaling, writes `OptiScaler.ini`. |
| `OptiCaptureHost.exe` | The capture + overlay + upscaling worker the launcher starts. |


<div align="center">

![OptiCaptureHost launcher](docs/launcher.png)

</div>

## Highlights

- **Works on almost anything that runs in a window** — DX9, DX11, DX12, Vulkan, OpenGL, emulators.
- **Multi Frame Generation from 2x up to 6x** — Intel XeFG and the DLSSG-based modes.
- **One-click profiles** — *NO FG*, *x2 / x3 / x4 (NEW GPU)*, *x2 / x3 / x4 (OLD GPU)*: frame generation, frame limiter and present mode set together.
- **Output scaling** — run the game at a lower resolution, let the host scale it to your monitor:

  | Method | What it is |
  |---|---|
  | **DLSS** | NVIDIA DLSS Super Resolution (RTX, needs `nvngx_dlss.dll`) |
  | **FSR 3 / 4** | AMD FidelityFX upscaler (any GPU, needs `amd_fidelityfx_dx12.dll`, which is AMD's `amd_fidelityfx_upscaler_dx12.dll` renamed) |
  | FSR 1 | EASU + RCAS, no extra files |
  | NIS | NVIDIA Image Scaling, no extra files |
  | Bilinear | Plain scaling |

  Levels: **Native (1x)**, **Quality (1.5x)**, **Balanced (1.7x)**, **Performance (2x)**.
- **The host measures motion itself.** A window capture has no motion vectors, so the host estimates them from consecutive frames on the GPU (block matching, sub-pixel refinement, temporal steadiness, a reactive mask).
- **Depth source (optional, experimental)** — by default the host's own FSR 3 / 4 and DLSS use a flat depth, so nothing is injected into the game. The *Depth* button in the launcher can switch to **ReShade**: a small ReShade add-on (`ReShade depth` folder in the full package) shares the game's real depth buffer with the host. Direct3D 11 / 12, single-player games only. See `ReShade depth/README.txt`.
- **Opti Upscaler selector** — FSR 3.1.6, FSR 4.1.2, DLSS 3.7, DittoSR, XeSS, DLSS; picking one writes `OptiScaler.ini` and copies that upscaler's folder next to the exes.
- Built-in frame limiter, low-latency present modes, VSync on/off/auto, configurable hotkeys, live status panel.

## Requirements

- Windows 10 (2004+) or Windows 11, 64-bit
- A GPU with DirectX 12 (Intel Arc, NVIDIA, AMD)
- Game in **Borderless Fullscreen or Windowed** mode (not Exclusive Fullscreen)
- OptiScaler files (not included, see below)
- Optional: RivaTuner Statistics Server (frame limiter), an NVIDIA RTX card (DLSS)

## Quick start (users)

1. Download `OptiCaptureHost-<version>-win64.zip` from the [latest release](../../releases/latest) into a folder of its own.
2. **Get OptiScaler.** Download the OptiScaler build from the
   [OptiScaler-F5-DLSSNR-Multipass](https://github.com/janblade/OptiScaler-F5-DLSSNR-Multipass) repository (janblade's fork of OptiScaler),
   **rename it to `dxgi.dll`** and put it, with its `OptiScaler.ini`, in the folder where you installed the program.
   **To update OptiScaler**, download the new version from the same repository, rename it to `dxgi.dll` again and replace the old file in the install folder.
3. Add the files of your frame generation / upscaler (see below). Optional: `amd_fidelityfx_dx12.dll` (FSR 3/4) and `nvngx_dlss.dll` (DLSS) next to the exes. `amd_fidelityfx_dx12.dll` is AMD's official `amd_fidelityfx_upscaler_dx12.dll` (FidelityFX SDK, [download](https://gpuopen.com/download-fsr-sdk/)) **renamed** to `amd_fidelityfx_dx12.dll`; the file itself is not changed.
4. Start the game in Borderless / Windowed mode, run `OptiCaptureLauncher.exe`, click the game, choose a profile, press **START**.
5. **Connect OptiScaler to the host.** There are two ways; use one of them:
   - **Through OptiScaler itself (after START):** press **START** first. Then open the OptiScaler menu (the key is shown in the launcher's status line) and, in the
     **Optical F5Low** section, enable **NR + Frame Generation**.
   - **Through Output scaling in the launcher (before START):** choose **FSR 3/4** or **DLSS** under **Output scaling first**, and only then press **START**.
     It needs the DLSS / FSR files (`nvngx_dlss.dll`, `amd_fidelityfx_dx12.dll`) to be **next to the program**.

   Either way OptiScaler attaches to the host, and its upscaling and frame generation start working.

   **OptiScaler menu keys:** the menu normally opens with **Insert**. If **Insert** does not open it in the game, press **Ctrl + Shift + F10**.
   After you close the OptiScaler menu, press **Ctrl + Shift + F11** to give the focus back to the game. **If you don't press it, the controller, mouse and keyboard will not work in the game.**

> **Full package:** everything above is already prepared in it, so you can use it right away.

> The source repository and the small `OptiCaptureHost-<version>-win64.zip` contain **no** third-party runtimes. They are theirs: get them from the official sources.

### Full package

Next to each release there can also be a **full package** with the third-party files already in it (OptiScaler, FSR 3/4, DLSS, Intel XeSS, DLSS Enabler), unmodified,
with their licence texts in the `licenses` folder. Each keeps its own licence; see [NOTICE.md](NOTICE.md).

**Download these separately** and put them next to the program (they are not included, for licence reasons):

- **DittoSR**: [github.com/Vancete/DittoSR/releases](https://github.com/Vancete/DittoSR/releases), copy its files into the `DittoSR` folder.
- **`nvngx_dlssnr.dll`** (DLSS 5 runtime), **optional**: it is needed only for DLSS 5, and everything else works without it. DLSS 5 needs **version 310.9.16** of this file. It is not distributed here; get it yourself from a source you trust and check that you may use it.

## ReShade depth (GPU & CPU shared memory)

By default, when the host's own temporal upscaler (**FSR 3/4** or **DLSS**) runs on a captured window, it uses a flat depth buffer so the game itself does not need any extra files or add-ons. When you enable **ReShade** in the **Depth** panel, `OptiCaptureDepth` reads the game's linearized depth (`OC_LinearDepth` from `OptiCaptureDepth.fx`) and shares it with `OptiCaptureHost.exe`:

- **Direct3D 11 & Direct3D 12 (GPU path):** Creates a shared `R32_FLOAT` texture with an NT handle (`Local\OptiCaptureDepth_<PID>`) and copies it directly on the GPU into `fsrDepth_` via `OpenSharedHandle` and `CopyResource`.
- **Direct3D 9, Direct3D 10, OpenGL & Vulkan (CPU path):** Because NT shared GPU texture handles are not available across these APIs into D3D12, the add-on copies the depth texture into a staging buffer (`memory_heap::gpu_to_cpu`), maps it on the CPU, and writes the float depth rows into a shared memory buffer (`Local\OptiCaptureDepthCpu_<PID>`). `OptiCaptureHost.exe` reads that buffer into a D3D12 upload buffer and uploads it to `fsrDepth_` via `CopyTextureRegion`.

### How to install and use ReShade depth

1. **Install ReShade with Add-on support first:** Download ReShade 6+ (**with add-on support**) from [reshade.me](https://reshade.me) and install it into your game. The launcher's install button copies only the required helper files — it does not install ReShade itself.
2. **Prepare the `ReShade depth` folder:** Place your required files inside the `ReShade depth` folder next to `OptiCaptureLauncher.exe` (you can click **Open "ReShade depth" folder** in the **Depth** panel to create/open it). Typically this includes `OptiCaptureDepth.addon64`, `OptiCaptureDepth.fx` (under `reshade-shaders\Shaders\`), and any required `.ini` files.
3. **Install into the selected game:** Select the running game in the launcher, open the **Depth** panel, and click **Install ReShade files**.
   - The launcher **completely closes the selected game** first so files are not locked.
   - It **copies only the required files** from `ReShade depth` next to the game's executable.
   - It **launches the game again** from its executable while **keeping the launcher window open**.
   - Once `OptiCapture.ini`, `OptiCaptureDepth.addon64`, and `reshade-shaders\Shaders\OptiCaptureDepth.fx` are present in the game folder, the install button turns **green** and displays **Installed**.
4. **Enable in ReShade and start capture:**
   - Open the ReShade overlay in-game (`Home` key), tick **OptiCaptureDepth** on, and verify depth detection with ReShade's **DisplayDepth** effect (adjusting reversed/logarithmic/flip depth settings if needed).
   - In the launcher's **Depth** panel, choose **ReShade** (automatically sets output scaling to **DLSS · Quality**; choosing **Flat** sets output scaling to **Off**), ensure output scaling is **FSR 3/4** or **DLSS**, and press **START**.
   - Watch the live status box in the **Depth** panel (`Receiving W×H`, `Different size`, `Cannot open it`, `No new pictures`, `Waiting for the add-on`).

> **Important notes:** Use ReShade **only in single-player games** (anti-cheat systems in online games can flag or ban ReShade/add-ons). CPU depth transfer on D3D9/D3D10/OpenGL/Vulkan copies depth data every frame through system RAM and may add CPU/memory overhead at high resolutions. Depth is matched when its dimensions match the captured picture size (`width_ × height_`).

## Administrator rights and RTSS

The launcher itself runs as a **normal program** (not as administrator), so the shortcuts of the games it starts keep working.

The frame limiter and the on-screen display are set through RivaTuner Statistics Server (RTSS) by writing its profile files. RTSS is usually installed in
`Program Files`, where Windows only lets an administrator write. So:

- When you press **Apply** (or START) and RTSS does not accept the change, the launcher starts a second copy of itself **as administrator** only for that one
  step, and Windows shows a **UAC prompt**. Click **Yes**. If you click **No**, the limit is not written and the launcher says so; press Apply again and allow it.
- The same happens if the **game runs as administrator** and the launcher does not: the launcher asks for permission to change the game window.
- OptiCaptureHost.exe always gets its own RTSS profile with the limit at **0** (no limit), unless you chose the host as the limiter target.
- Keep **RTSS running** while you apply. If RTSS is not installed, the launcher tells you and the rest still works.
- Your RTSS **Global** profile is never changed; the launcher only edits the profile of the game (or of the host) and puts it back when you turn the limiter off.
- To avoid the prompt every time, you can run the launcher as administrator yourself (right-click, Run as administrator), but then games started from it also
  run as administrator.

## Building from source

You need **Windows**, **Visual Studio 2022** (Desktop development with C++) and **CMake 3.20+**.

```powershell
git clone https://github.com/tegraultra/OptiCaptureHost.git
cd OptiCaptureHost
cmake -S . -B build -A x64
cmake --build build --config Release
# build\Release\OptiCaptureHost.exe, build\Release\OptiCaptureLauncher.exe
```

**DLSS support is optional at build time.** NVIDIA's NGX SDK is not stored in this repository (it has its own licence). The GitHub Actions
workflow downloads it from NVIDIA's public repository into `third_party/ngx` before building. If you build locally, copy NVIDIA's headers into
`third_party/ngx/include` and `nvsdk_ngx_s.lib` into `third_party/ngx/lib`; without them CMake builds the host **without DLSS** and the log says so.

**Continuous builds:** every push runs [`build.yml`](.github/workflows/build.yml). On `main` it publishes a release named after
`src/version.h` (tag `vX.Y.Z`); older releases are kept.

## Project layout

```
src/
  main.cpp               host: capture, D3D12 overlay, upscaling passes, FSR/DLSS integration
  launcher.cpp           launcher UI (GDI+), profiles, OptiScaler.ini writer
  settings.h             all settings, OptiCaptureHost.ini load/save
  version.h              the one place for the version number
  upscale_shader.h       FSR 1 (EASU/RCAS) shaders        (AMD, MIT)
  nis_shader.h/_config.h NVIDIA Image Scaling             (NVIDIA, MIT)
  fsr_flow_shader.h      motion vectors (block search + per-pixel pass)
  depth_share.h          what the ReShade depth add-on and the host exchange
  reshade_addon/         optional ReShade add-on (OptiCaptureDepth.addon64), built on its own
reshade/                 OptiCaptureDepth.fx (ReShade effect) and the add-on's README
third_party/ffx/         AMD FidelityFX API headers       (MIT)
```

### Where to work

- **Motion estimation** (the biggest quality lever for DLSS / FSR 3): `fsr_flow_shader.h`. New ideas should go in a **new, switchable module**, with the current one kept.
- **Launcher options:** add an id in `launcher.cpp`, draw it in `Paint`, handle it in `OnClick`, store it in `settings.h`.
- Keep `motionVectorScale` at `{1, -1}`; the vectors point from a pixel to where it was in the previous frame.

### Contributing

Pull requests and issues are welcome. Please:
1. Keep changes focused; one feature or fix per PR.
2. Say what you tested (GPU, game, upscaler). The CI only checks that it builds.
3. For quality work attach a short before/after clip and `OptiCaptureHost.log`.
4. Don't add third-party binaries (DLLs from AMD, NVIDIA, Intel, OptiScaler) to the repository.

## Good to know

- **Latency:** the capture adds one hop; VSync off at START gives the lowest latency.
- **Mouse:** with output scaling on, the pointer is not remapped.
- **Temporal upscalers on a capture:** there is no game jitter and, by default, no true depth (the optional ReShade depth source below changes that), so DLSS / FSR 3 work from measured motion and look a bit softer than in-game. FSR 4 copes best. This part is experimental.
- **Online games:** capturing and resizing a window can still upset some anti-cheat systems. Use at your own risk.

## Troubleshooting

| Problem | Try |
|---|---|
| Launcher says the host stopped | Open `OptiCaptureHost.log` next to the exes; the last lines say why. |
| Output scaling falls back to FSR 1 | The log names the missing file (`nvngx_dlss.dll` / `amd_fidelityfx_dx12.dll`). |
| Black or frozen picture | The game must be Borderless / Windowed. |
| No frame limiter | Install RivaTuner Statistics Server (RTSS) and keep it running, then read "Administrator rights and RTSS" below. |

## Third-party

| Component | Licence |
|---|---|
| FSR 1 EASU/RCAS shader code, FidelityFX API headers (AMD) | MIT |
| NVIDIA Image Scaling (NIS) shader code | MIT |
| NVIDIA NGX / DLSS SDK headers | NVIDIA's licence; **downloaded at build time, not in this repo** |
| OptiScaler, AMD / NVIDIA / Intel runtimes | their own licences; not included |

**This product uses NVIDIA DLSS.** NVIDIA, NVIDIA RTX, DLSS and GeForce are trademarks of NVIDIA Corporation. The DLSS files in the full package are
NVIDIA's, are distributed unmodified together with this application (never as a stand-alone product) and are covered by the NVIDIA RTX SDKs licence
(`licenses/NVIDIA-RTX-SDK-LICENSE_DLSS.txt`). NVIDIA Streamline files are MIT licensed (`licenses/NVIDIA-Streamline-LICENSE_MIT.txt`).
Anyone who redistributes them must keep those licence terms.

All trademarks belong to their owners. This project is not affiliated with or endorsed by any of them, including NVIDIA.

## Support

Telegram **[@MKJH_APPS](https://t.me/MKJH_APPS)** · bugs and ideas: [issues](../../issues) (attach `OptiCaptureHost.log`).

## License

OptiCaptureHost is free software under the **GNU General Public License v3.0** ([LICENSE](LICENSE)), © MK-JH:
you may use, study, change and share it, and anything you distribute that is based on it must stay GPL-3.0 with its source available.
Third-party components keep their own licences (table above).

---

<div align="center">Made by <b>MK-JH</b> · <a href="https://t.me/MKJH_APPS">t.me/MKJH_APPS</a></div>
