# CLI Starter 한국어 안내

`CLI Starter`는 새 명령줄 도구를 빈 저장소에서 시작하지 않도록 만든 C++17
템플릿입니다. 저장소를 복사한 뒤 실행 파일 이름, 표시 이름, 설정 파일과
프롬프트를 바꾸고 예제 명령을 실제 동작으로 교체하는 흐름을 제공합니다.

## 빠른 시작

CMake 3.18 이상, C++17 컴파일러, Python 3과 Git이 필요합니다.

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
./build/cli-starter hello --name "template user"
```

로컬 설정을 만들고 인터랙티브 셸을 시작하려면 다음과 같이 실행합니다.

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json
```

Visual Studio 같은 멀티 구성 생성기에서는 실행 파일이
`build/Debug/cli-starter.exe`처럼 구성별 디렉터리에 생성될 수 있습니다.

## 주요 기능

- 옵션, 위치 인자와 하위 명령을 사용하는 단발 명령 실행
- 명령·하위 명령·옵션 `Tab` 자동완성을 제공하는 인터랙티브 셸
- `about`, `hello`, `echo`, `config`, `doctor`, `shell` 예제 명령
- JSON 설정 템플릿과 명시적 설정 경로 지원
- 같은 공개 명령 레지스트리에서 Bash/Zsh/PowerShell 자동완성, roff man
  페이지, JSON 메타데이터와 Markdown 명령 참조 생성
- CLI11, nlohmann/json, doctest 헤더 의존성 내장

## 설정과 운영 주의사항

프로젝트 이름은 다음 CMake 캐시 변수로 조정합니다.

- `CLI_STARTER_BINARY_NAME`: 실행 파일 이름
- `CLI_STARTER_DISPLAY_NAME`: 사용자에게 표시할 이름
- `CLI_STARTER_CONFIG_FILE`: 기본 JSON 설정 파일 이름
- `CLI_STARTER_PROMPT_LABEL`: 기본 셸 프롬프트

설정 경로는 실행 시점의 현재 디렉터리를 기준으로 해석됩니다. 실험용 설정은
`config/local.json`이나 `config/*.local.json`처럼 무시되는 경로를 사용하세요.
`enabled_commands`는 현재 표시용 설정이며 런타임 허용 목록이 아닙니다.
생성 명령은 셸 프로필이나 시스템 문서를 자동으로 설치하지 않습니다.

`build/`, `build-local-*`, `.sandbox-user/`와 로컬 설정 파일은 생성 산출물로
취급하고 소스 변경과 함께 커밋하지 마세요.

## 검증

문서 변경의 프로젝트 상태 검사는 다음과 같습니다.

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tests/check_project_completion.py
git diff --check
```

전체 동작 검증은 새 빌드 디렉터리에서 실행합니다.

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

세부 명령, 설정 의미와 확장 절차는 [원본 README](../../README.md)를
참고하세요.
