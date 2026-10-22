# NanaZip auto encoding

[English](README.md) | 한국어

[NanaZip](https://github.com/M2Team/NanaZip)으로 압축을 풀 때 ZIP 파일명 인코딩을 자동으로 골라 주는 탐색기 우클릭 메뉴와 오른쪽 드래그 메뉴입니다. 일본어 Windows에서 만든(Shift-JIS) 압축 파일도 한국어나 영어 Windows에서 파일명이 깨지지 않고 풀립니다.

<img src="docs/images/before-after.png" width="800" alt="같은 Shift-JIS 압축 파일을 한국어 Windows에서 푼 결과: NanaZip 기본은 깨진 한글 이름, 이 애드온은 원래 일본어 이름">

같은 압축 파일을 한국어 Windows에서 NanaZip 기본 방식과 이 애드온으로 각각 푼 결과입니다.

## 왜 필요한가

예전 Windows 압축 프로그램으로 만든 ZIP 파일은 파일명을 만든 PC의 문자 코드로 저장하면서, 어떤 코드인지는 기록하지 않습니다. NanaZip은 이런 이름을 시스템 문자 코드로 읽기 때문에, Shift-JIS 압축 파일을 한국어 Windows에서 열면 `テスト`가 `긡긚긣`처럼 보입니다.

NanaZip의 압축 해제기는 문자 코드를 알려 주면 제대로 풉니다(예: `NanaZipC x archive.zip -mcp=932`). 하지만 GUI에는 아직 이 옵션이 없습니다([M2Team/NanaZip#325](https://github.com/M2Team/NanaZip/issues/325)). 이 애드온은 문자 코드를 판별해서 대신 넘겨 줍니다.

## 기능

- **`.zip` 파일 우클릭:**
  - **NanaZip으로 여기에 압축 풀기 (스마트, 인코딩 자동):** 최상위에 폴더나 파일이 하나뿐이면 같은 이름이 이미 있지 않은 한 압축 파일 옆에 바로 풀고, 그 밖의 경우에는 압축 파일 이름으로 새 폴더를 만들어 그 안에 풉니다.
  - **NanaZip으로 폴더에 압축 풀기 (인코딩 자동):** 항상 압축 파일 이름의 새 폴더에 풉니다.

  <img src="docs/images/context-menu.png" width="400" alt=".zip 파일 우클릭 메뉴에서 즐겨찾기에 추가와 Windows 기본 압축 풀기 사이에 들어간 두 항목">

- **압축 파일을 폴더로 오른쪽 드래그** (마우스 오른쪽 버튼으로 끌어다 놓기): 같은 두 항목으로 놓은 폴더에 풉니다. zip, 7z, rar, tar, gz, xz, zst, cab, iso 등 일반 압축 형식에서 동작합니다. NanaZip 자체에는 오른쪽 드래그 메뉴가 없습니다.

  <img src="docs/images/drag-menu.png" width="400" alt="오른쪽 드래그 메뉴에서 Windows 복사, 이동 항목 위에 추가된 두 항목">

- **선택: 파란 압축 파일 아이콘:** NanaZip이 여는 압축 파일과 디스크 이미지 62개 형식(zip, 7z, rar, tar, gz, iso, vhdx 등)에 "NanaZip (파란 아이콘)" 앱을 등록합니다. 어떤 형식의 기본 앱으로 고르면 그 형식의 파일은 NanaZip으로 열리고, NanaZip의 흰 문서 아이콘 대신 파란 앱 아이콘으로 보입니다.

  <img src="docs/images/file-icon.png" width="440" alt="NanaZip의 흰 .zip 파일 아이콘과 파란 NanaZip 아이콘">

세부 동작:

- 새 폴더 이름이 이미 있으면 번호를 붙입니다(`이름 (2)`).
- 여러 압축 파일을 한꺼번에 고르면 한 번 실행으로 차례대로 풉니다.
- `.tar.gz`, `.tgz`, `.tar.xz` 같은 형식은 `.tar`에서 멈추지 않고 끝까지 풉니다.
- 실제 압축 해제는 NanaZip의 GUI 압축 해제기(`NanaZipG`)가 합니다.
- 메뉴 문구는 영어 또는 한국어입니다.
- 모든 설정은 현재 사용자에게만 설치되며 관리자 권한이 필요 없습니다.

스크린샷은 한국어 Windows(시스템 기본 항목이 한국어)에 애드온을 영어로 설치한 화면입니다.

## 설치

1. NanaZip을 Microsoft Store에서 설치하거나 `winget install M2Team.NanaZip`으로 설치합니다.
2. [Releases](../../releases)에서 `nanazip-auto-encoding.zip`을 받아 압축을 풉니다.
3. `Install-NanaZipAutoCP.ps1`을 우클릭해서 **PowerShell에서 실행**을 누르거나, 다음을 실행합니다.
   ```
   powershell -ExecutionPolicy Bypass -File Install-NanaZipAutoCP.ps1
   ```
   결과는 메시지 창으로 알려 줍니다. 메뉴 언어는 Windows 표시 언어를 따르며, 직접 고르려면 `-Language ko` 또는 `-Language en`을 붙입니다.
4. 선택, 파란 아이콘: **설정 > 앱 > 기본 앱 > NanaZip (파란 아이콘)**에서 원하는 형식마다 이 앱을 고릅니다. 압축 파일을 우클릭해서 **연결 프로그램 > 다른 앱 선택**에서 **NanaZip (파란 아이콘)** 항목을 고르고 **항상**을 눌러도 됩니다. Windows는 기본 앱을 사용자가 형식마다 직접 고르게만 허용합니다.

Windows 11의 간단한 우클릭 메뉴에서는 **추가 옵션 표시** 안에 항목이 있습니다.

제거하려면 PowerShell에서 다음을 실행합니다.

```
powershell -ExecutionPolicy Bypass -File "$env:LOCALAPPDATA\NanaZipAutoCP\Install-NanaZipAutoCP.ps1" -Uninstall
```

명령 프롬프트에서는 `$env:LOCALAPPDATA` 대신 `%LOCALAPPDATA%`를 씁니다. 탐색기가 메뉴 DLL을 아직 불러 둔 상태라면, 설치 폴더는 다음 로그인 때 지워집니다. "NanaZip (파란 아이콘)"을 기본 앱으로 고른 형식이 있다면 제거 후 그 형식에 다른 앱을 골라 주세요.

## 인코딩 판별 방식

판별은 확장자가 `.zip`인 파일에만 적용합니다. 다른 형식은 NanaZip 기본 방식으로 풉니다. 7z와 RAR5는 유니코드로 이름을 저장하고, 옛날 방식 이름을 담을 수 있는 RAR4와 TAR는 처리하지 않습니다.

1. UTF-8 표시가 있는 항목, 체크섬이 이름과 맞는 Info-ZIP Unicode Path 필드가 있는 항목, 영문(ASCII) 이름은 NanaZip에 그대로 맡깁니다.
2. 남은 이름을 UTF-8, Shift-JIS(932), CP949(949)로 각각 읽고, 실제 파일명처럼 보이는 정도로 점수를 매깁니다.
   - 잘못된 바이트 배열은 크게 감점합니다.
   - 가나, 자주 쓰는 KS X 1001 한글, 한자, 악센트가 붙은 라틴 문자, 결합 악센트는 가점입니다.
   - 잘못 읽었을 때 나오는 특징은 감점합니다. CP949를 Shift-JIS로 읽으면 반각 가타카나가 되고, Shift-JIS를 CP949로 읽으면 거의 쓰이지 않는 UHC 확장 영역의 한글이 됩니다(`テスト`가 `긡긚긣`가 됨).
3. 점수가 가장 높은 쪽을 쓰고, 동점이면 CP949, 그다음 Shift-JIS를 씁니다. 0점보다 높은 결과가 없으면 NanaZip 기본 방식을 그대로 씁니다.

## 한계

- 후보는 UTF-8, Shift-JIS, CP949뿐입니다. 예를 들어 중국어 GBK 이름은 CP949로 읽힐 수 있으므로, 중국어처럼 자체 옛날 문자 코드가 있는 로캘로 설정된 Windows에서는 권장하지 않습니다.
- 반각 가타카나로만 된 이름은 CP949로 판별됩니다.
- UTF-8 표시 없이 UTF-8로 저장된 이름 중 비ASCII 문자가 악센트 붙은 라틴 문자뿐인 이름(예: `café.txt`)은 CP949와 동점이 되어 CP949로 읽힙니다. 결합 악센트를 쓰는 macOS의 ZIP 파일은 UTF-8로 판별됩니다.
- 메뉴 DLL은 x64로만 빌드되어, ARM용 Windows에서는 메뉴를 쓸 수 없습니다.
- Windows 11 x64와 NanaZip 7.0에서 테스트했습니다.

## 파일

| 파일 | 역할 |
| --- | --- |
| `Install-NanaZipAutoCP.ps1` | 현재 사용자에게 모두 설치하거나 제거합니다. BOM이 있는 UTF-8로 유지해야 합니다. 그렇지 않으면 Windows PowerShell 5.1이 한국어 문구를 잘못 읽습니다. |
| `Expand-ArchiveAuto.ps1` | 문자 코드를 판별하고 NanaZip을 실행합니다. 단독으로도 쓸 수 있습니다(예: `Expand-ArchiveAuto.ps1 -Mode Folder a.zip b.7z` 또는 `-DetectOnly a.zip`). |
| `src/NanaZipAutoCP.cpp` | 메뉴 DLL(`NanaZipAutoCP.dll`)입니다. `.zip`의 우클릭 메뉴 핸들러와 폴더의 드래그 앤 드롭 핸들러로 등록되는 작은 COM 셸 확장으로, 메뉴 문구와 경로를 `HKCU\Software\NanaZipAutoCP`에서 읽고 Windows PowerShell로 `Expand-ArchiveAuto.ps1`을 실행합니다. |
| `tests/` | 테스트용 ZIP 생성기, 판별 및 압축 해제 테스트, 대역 스크립트를 쓰는 DLL 테스트, 등록된 핸들러로 셸이 만드는 메뉴를 보여 주는 작은 프로그램들입니다. |

## 빌드와 테스트

C++ 도구와 Windows SDK가 설치된 **x64 Native Tools Command Prompt for VS**에서 DLL과 테스트 프로그램을 빌드합니다.

```
src\build.cmd
```

`NanaZipAutoCP.dll`이 설치 스크립트 옆에 생기고, 설치 스크립트가 이를 설치합니다.

테스트:

```
python -I tests/harness.py --detect-only   # 문자 코드 판별만
python -I tests/harness.py                 # 모든 테스트 ZIP을 실제로 풀어 봄, NanaZip 필요
python -I tests/shell_test.py              # 대역 스크립트로 DLL 테스트, HKCU\Software\NanaZipAutoCP는 원래대로 복원
tests\test_drag.exe <dll> drag|click <폴더> -1 <압축 파일>...   # 탐색기처럼 DLL을 호출해서 메뉴 출력
tests\test_shellmenu.exe click <압축 파일>...                   # 셸이 만드는 우클릭 메뉴 출력, 화면에는 띄우지 않음
tests\test_shellmenu.exe drop <폴더> <압축 파일>...             # 실제 오른쪽 드롭, 메뉴가 잠깐 화면에 뜸
tests\test_assoc.exe .zip                                       # .zip의 연결 프로그램 목록 출력
```

GitHub Actions 워크플로가 push할 때마다 DLL을 빌드하고 판별 테스트와 DLL 테스트를 실행합니다.

## 라이선스

MIT, [LICENSE](LICENSE)를 참고하세요. NanaZip 및 M2Team과 관련 없는 비공식 프로젝트입니다.

스크린샷에 보이는 NanaZip 로고와 파일 아이콘은 Shomnipotence가 NanaZip 프로젝트를 위해 디자인했으며 [CC BY-ND 4.0](https://creativecommons.org/licenses/by-nd/4.0/) 라이선스입니다([NanaZip License](https://github.com/M2Team/NanaZip/blob/main/License.md)). 이 저장소에는 아이콘이 들어 있지 않으며, 설치 스크립트가 PC에 설치된 NanaZip에서 읽어 옵니다.

## 다른 프로젝트도 써 보세요

마음에 드셨다면 제가 만든 다른 오픈소스도 둘러보세요. 안드로이드 앱, Claude 채팅 게임, Claude 애드온 등이 있어요: [wonjoonseol-ws.github.io](https://wonjoonseol-ws.github.io/ko/?from=nanazip-auto-encoding)
