<div align="center">

# InputLeafPlus

**하나의 키보드와 마우스로 Mac과 Windows를 함께 쓰기**

[![Release](https://img.shields.io/github/v/release/Craft374/InputLeafPlus?label=%EB%8B%A4%EC%9A%B4%EB%A1%9C%EB%93%9C&color=2ea44f)](https://github.com/Craft374/InputLeafPlus/releases/latest)
![Platforms](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)
[![License](https://img.shields.io/badge/license-GPLv2-blue)](LICENSE)

[Input Leap](https://github.com/input-leap/input-leap)을 바탕으로 **Mac ↔ Windows 조합**에 맞춰 다듬은 개인 포크입니다.<br>
키 배치 변환, 한글 입력, 클립보드, 휴대폰 트랙패드, 버튼 한 번으로 하는 역할 교환을 지원합니다.

</div>

---

## 주요 기능

| | 기능 | 설명 |
|---|---|---|
| ⌨️ | **키 배치 변환** | 화면별로 Ctrl/Alt/⌘/⌥ 위치를 바꿉니다. 스페이스바 옆 키는 어느 OS에서든 스페이스바 옆 키로 동작합니다 |
| 🇰🇷 | **한글과 기능 키** | 한/영 전환, Caps Lock→F18 프리셋, Mac 상단 F1~F12 전달, F17~F20 인식 |
| 📋 | **클립보드 공유** | 한글 UTF-8과 줄바꿈 변환을 지원합니다. Windows→Mac 복사 불가 문제를 근본적으로 고쳤습니다 |
| 📱 | **휴대폰 트랙패드** | 앱 설치 없이 휴대폰 브라우저를 Mac 트랙패드처럼 씁니다 |
| 🔁 | **역할 교환** | 버튼 한 번으로 서버와 클라이언트를 맞바꿉니다. 화면 구성도 함께 넘어갑니다 |
| 🖱️ | **화면별 스크롤** | 화면마다 스크롤 방향을 따로 정합니다 |
| 🎮 | **게임 모드 (실험)** | 상대 이동 방식과 `Ctrl+Shift+G` 커서 고정 |

## 설치

[**최신 릴리즈 받기 →**](https://github.com/Craft374/InputLeafPlus/releases/latest)

> [!IMPORTANT]
> **두 PC를 모두 같은 버전으로 업데이트하세요.** 3.1.0부터 SSL 암호화 기능을 없애고 항상 일반 연결을 씁니다. 그래서 SSL이 켜진 예전 버전과는 연결되지 않습니다.

### macOS (Apple Silicon)

1. `InputLeafPlus-<버전>.dmg`를 열고 앱을 **응용 프로그램** 폴더로 옮깁니다.
2. 처음 실행할 때 "손상되었거나 확인되지 않은 개발자" 경고가 뜨면 터미널에서 아래 명령을 한 번 실행합니다. 공증받지 않은 개인 빌드라서 생기는 경고입니다.
   ```bash
   xattr -cr /Applications/InputLeafPlus.app
   ```
3. `시스템 설정 › 개인정보 보호 및 보안 › 손쉬운 사용`에서 InputLeafPlus를 허용합니다.

### Windows (x64)

1. `InputLeafPlusSetup-<버전>.exe`를 실행해 설치합니다.
2. 방화벽 허용 창이 뜨면 **허용**을 누릅니다. 서버 역할이면 24800, 휴대폰 트랙패드는 24803, 역할 교환은 24802 포트를 씁니다.
3. 이미 켜져 있을 때 앱을 다시 실행하면 새로 뜨지 않고 기존 창이 앞으로 나옵니다.

## 빠른 시작

1. 키보드와 마우스가 연결된 PC에서 **서버**를, 다른 PC에서 **클라이언트**를 고릅니다.
2. 서버의 `서버 설정`에서 클라이언트 화면을 원하는 방향에 배치합니다. 화면 이름은 클라이언트 PC 이름과 같아야 합니다.
3. 클라이언트에 서버 IP를 입력합니다. 자동 설정을 켜 두면 같은 네트워크의 서버를 찾아 줍니다.
4. 양쪽에서 **시작**을 누릅니다.

## 📱 휴대폰 트랙패드

설정 창의 `휴대폰 트랙패드`에서 **사용**을 켭니다. 휴대폰을 같은 와이파이에 연결한 뒤 화면에 표시된 주소를 열고 PIN을 입력합니다.
**현재 서버인 PC의 주소로 접속하세요.** 그러면 서버를 거쳐 두 화면을 모두 조작할 수 있습니다.

Mac 트랙패드와 최대한 같은 느낌을 목표로 했습니다.

| 동작 | 결과 |
|---|---|
| 한 손가락 이동 | 커서 이동. Mac과 비슷하게 손가락 속도에 따라 가속됩니다 |
| 탭 / 두 번 탭 | 클릭 / 더블클릭 |
| 탭 후 바로 다시 눌러 끌기 | 드래그 (탭하여 끌기) |
| 두 손가락 탭 | 오른쪽 클릭 |
| 두 손가락 스크롤 | 픽셀 단위 부드러운 스크롤과 관성. 기본 방향은 PC 설정을 따릅니다 |
| 두 손가락 오므리기/벌리기 | 확대/축소 (Mac `⌘+`/`⌘-`, Windows `Ctrl+휠`) |
| 두 손가락 빠르게 좌우로 쓸기 | 뒤로/앞으로 (기본 켜짐, 설정에서 끌 수 있음) |
| 세 손가락 끌기 | 창이나 선택 영역 끌기. 손을 뗀 뒤 0.5초 안에 다시 잡으면 이어집니다 |
| 세 손가락 탭 | 찾아보기 (Mac) |
| 네 손가락 쓸기 | 데스크톱 전환, Mission Control / 작업 보기 |
| 네 손가락 오므리기/펼치기 | Launchpad·시작 메뉴 / 바탕화면 보기 |

- 제스처 단축키는 **커서가 지금 있는 화면의 OS**에 맞춰 보냅니다. 서버의 키 배치 변환 설정도 반영합니다.
- 한글은 두벌식 키 위치로 보냅니다. 받는 PC의 입력기를 페이지의 `한/영` 버튼으로 한글 상태로 바꿔 주세요.
- 같은 와이파이의 누구나 접속을 시도할 수 있습니다. 추측하기 어려운 PIN을 쓰고, 공용 네트워크에서는 기능을 꺼 두세요.

## 🔁 역할 교환

두 PC 모두 설정 창의 `PC 간 역할 교환`에서 **사용**을 켜고 같은 페어링 코드(8자 이상)를 입력합니다.
이후 메인 창의 **역할 교환** 버튼을 누르면 양쪽이 동시에 역할을 바꾸고 다시 시작합니다. 포트와 화면 구성도 함께 넘어갑니다.

- 두 PC의 현재 역할이 서로 달라야 합니다.
- 서버는 `상호작용 설정`을 써야 합니다. `기존 설정을 사용`(외부 설정 파일)은 교환되지 않습니다.

## 원본 대비 개선 사항

<details>
<summary><b>키보드</b></summary>

- 화면별 키 매핑 설정 창과 Shift/Ctrl/Alt/Meta/Super 재배치를 추가했습니다.
- Windows 키보드를 Mac 배열로 쓰는 프리셋이 있습니다. `Alt`는 `⌘`, `Windows` 키는 `⌥`로 보냅니다.
- 저장된 modifier 배치를 덮어쓰던 자동 마이그레이션을 없앴습니다.
- 키 누름, 뗌, 반복, 브로드캐스트에 같은 매핑을 적용합니다. 매핑된 키의 원래 modifier가 새어 나가 조합키로 오인되던 문제도 고쳤습니다.
- Windows 한/영 가상 키 코드를 바로잡았습니다.
- Caps Lock을 다른 키로 매핑해도 Windows의 Caps Lock 상태가 함께 바뀌지 않습니다.
- Mac 서버에서 지정한 USB 입력 장치(여러 개 가능)는 원격으로 보내지 않고 Mac에서만 씁니다.
- Mac 상단 기능 키를 Windows에서 F1~F12로 보낼 수 있습니다.

</details>

<details>
<summary><b>클립보드</b></summary>

- macOS `public.utf8-plain-text`를 지원하고 OS별 줄바꿈을 변환합니다.
- 화면을 전환하는 순간 서버 클립보드를 즉시 읽습니다. 소유권 알림 순서가 엇갈려도 안전하게 처리합니다.
- Windows 클립보드를 다른 프로그램이 잠시 점유하고 있으면 재시도합니다. 읽기 실패를 성공으로 보고하던 문제도 고쳤습니다.
- **Windows→Mac 복사 불가 문제의 원인**: 세션 0에서 도는 서비스가 `OpenInputDesktop()`으로 데스크톱 이름을 읽었는데, 항상 빈 값이 돌아왔습니다. 그래서 매번 로그인 화면으로 오인하고 서버를 `SYSTEM` 권한으로 띄웠고, `SYSTEM` 프로세스는 사용자 클립보드를 읽을 수 없었습니다. 이제 `WTSQueryUserToken()`으로 사용자 토큰을 먼저 시도합니다.
  - 그 대가로 잠금 화면과 UAC 창은 원격으로 조작할 수 없습니다. 필요하면 권한 상승을 `Always`로 바꿀 수 있지만, 그러면 클립보드 공유가 다시 막힙니다.

</details>

<details>
<summary><b>Windows 실행·종료</b></summary>

- 앱은 한 번에 하나만 실행됩니다. 다시 실행하면 기존 창이 앞으로 나옵니다.
- 서비스는 수동 시작으로 설치됩니다. 앱을 켜면 함께 시작하고, 트레이나 메인 창의 `종료`를 누르면 서비스까지 멈춥니다.
- 서비스 중지에 실패하면 GUI만 사라지지 않도록 종료를 취소하고 알려 줍니다.
- 서버로 시작할 때 사용하는 포트의 방화벽 규칙을 자동으로 추가합니다.

</details>

<details>
<summary><b>제거한 기능</b></summary>

- **SSL/TLS 암호화**: 양쪽 설정이 어긋나면 연결이 조용히 실패하는 주요 원인이어서 통째로 없앴습니다. 인증서, 지문 확인 창, OpenSSL 의존성도 함께 빠졌습니다. 예전 설정의 `--enable-crypto` 같은 옵션은 무시합니다.
- 연결이 암호화되지 않으므로 **신뢰할 수 있는 집이나 사무실 네트워크에서만** 쓰세요.

</details>

## 직접 빌드하기

<details>
<summary><b>macOS</b></summary>

```bash
./build_macos.command
```

Homebrew로 `cmake`, `ninja`, `qt`를 준비한 뒤 ARM64 Release로 빌드합니다. 결과물은 `build/bundle/InputLeafPlus.app`과 DMG입니다.
빌드가 실패해도 기존 앱은 손상되지 않습니다.

</details>

<details>
<summary><b>Windows</b></summary>

```powershell
.\build_windows.bat
```

Visual Studio C++ Build Tools, Qt 6.6.3, Bonjour 호환 SDK를 확인하고, 없는 항목은 동의를 받아 준비합니다.

| 경로 | 내용 |
| --- | --- |
| `build\input-leap-install\` | 실행 파일 모음. `input-leap.exe` 실행 |
| `build\installer-inno\bin\` | 설치 파일 (Inno Setup 6이 있을 때) |

서비스는 **설치된 위치**에서 실행됩니다. 그래서 데몬(`input-leapd`)을 고쳤다면 설치 파일로 다시 설치하거나 `C:\Program Files\InputLeafPlus\`에 직접 복사해야 합니다.

데몬 로그는 `C:\ProgramData\InputLeap\input-leapd.log`에 남습니다.

</details>

## 호환성과 라이선스

설정 형식과 실행 파일 이름(`input-leap`, `input-leaps`, `input-leapc`, `input-leapd`)은 원본과 같습니다. 기존 InputLeap 설정은 처음 실행할 때 자동으로 가져옵니다.

원본 Input Leap과 같은 **GNU GPL v2**로 배포합니다. 자세한 내용은 [LICENSE](LICENSE)를 보세요.
