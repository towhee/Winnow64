{{TITLE}} for macOS (Apple Silicon and Intel), Windows and Linux.

{{CHANGES}}
## What to Download

| OS | Architecture | Format | Requirements | Download Link |
|:---:|:---:|:---:|:---:|:---:|
| **macOS (Apple Silicon)** | ARM64 | `.dmg` | {{MACOS_ARM_MIN}} | [Winnow for macOS ARM](https://github.com/{{REPO}}/releases/download/{{TAG}}/Winnow{{VERSION}}.dmg) |
| **macOS (Intel)** | x86_64 | `.dmg` | {{MACOS_INTEL_MIN}} | [Winnow for macOS Intel](https://github.com/{{REPO}}/releases/download/{{TAG}}/Winnow{{VERSION}}_Intel.dmg) |
| **Windows** | x64 | `.exe` | Windows 10 or 11 | [Winnow for Windows](https://github.com/{{REPO}}/releases/download/{{TAG}}/WinnowSetup-{{VERSION}}.exe) |
| **Linux** | x86_64 | `.AppImage` | **Ubuntu 26.04 or later** (glibc 2.43+) | [Winnow for Linux x86_64](https://github.com/{{REPO}}/releases/download/{{TAG}}/Winnow-{{VERSION}}-x86_64.AppImage) |
| **Linux** | ARM64 | `.AppImage` | **Ubuntu 26.04 or later** (glibc 2.43+) | [Winnow for Linux ARM64](https://github.com/{{REPO}}/releases/download/{{TAG}}/Winnow-{{VERSION}}-aarch64.AppImage) |

**macOS:** open the DMG and drag Winnow into Applications. The app is signed and notarized by Apple.

**Windows:** the installer is not code-signed yet, so Microsoft Defender SmartScreen warns twice: once when you download it and once when you run it.

1. **Downloading in Microsoft Edge.** Edge may say *"WinnowSetup-{{VERSION}}.exe isn't commonly downloaded"* and stop the download. Type `edge://downloads` in the address bar and click **Keep** under the file. If Edge warns again, click **Show more**, then **Keep anyway**. Chrome and Firefox do not block the download.

   ![The Edge downloads page with the Keep button under WinnowSetup](https://raw.githubusercontent.com/{{REPO}}/master/.github/images/Windows_EdgeKeepAnyway.png)

2. **Running the installer.** If a blue *"Windows protected your PC"* window appears, click **More info**, then **Run anyway**.

On a work or school PC an administrator may have turned off **Keep anyway**; download Winnow on another PC or with another browser.

**Linux:** this build needs **Ubuntu 26.04 or later**, or another distribution with glibc 2.43 or newer (check with `ldd --version`). It will not start on Ubuntu 24.04, 22.04, Debian 12 or other older releases. Make the AppImage executable and run it (`chmod +x Winnow-*.AppImage`, then double-click it or run `./Winnow-*.AppImage`). Nothing is installed; keep the file wherever you like. Writing metadata into files needs Perl, which most distributions include.
