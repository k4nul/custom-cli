# CLI Starter

[English](#english) | [한국어](#한국어)

## English

`CLI Starter` is a small C++17 command-line application template. It is meant to be copied,
renamed, and extended when you want to build a new CLI tool without starting from an empty
repository.

It includes:

- One-shot command execution, such as `cli-starter hello --name "Ada"`
- An interactive shell mode with `Tab` completion
- A JSON configuration template
- Example commands that demonstrate options, positional arguments, subcommands, and validation
- Vendored header-only dependencies for command parsing, JSON, and tests

## Requirements

- CMake 3.18 or newer
- A C++17 compiler

On Linux, `cmake`, `g++` or `clang++`, and `ctest` are enough for the normal build and test flow.
On Windows, use Visual Studio, Build Tools for Visual Studio, or another CMake-supported C++ toolchain.

## Quick Start

Configure and build:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
```

Run a sample command:

```bash
./build/cli-starter hello --name "template user"
```

If your CMake generator is multi-config, such as Visual Studio, the executable may be under a
configuration directory:

```powershell
.\build\Debug\cli-starter.exe hello --name "template user"
```

Generate an ignored local starter config:

```bash
./build/cli-starter --config ./config/local.json config init
```

This keeps the checked-in template at `config/cli-starter.json` unchanged while you experiment.

Start the interactive shell:

```bash
./build/cli-starter
```

In the shell:

```text
starter> help
starter> hello --name "Ada"
starter> config show
starter> exit
```

## Interactive Completion

The interactive shell supports command, subcommand, and option completion when
it is attached to an interactive terminal. If stdin is redirected, or terminal
raw mode is unavailable, the shell still accepts commands but reads plain lines
without `Tab` completion.

- Press `Tab` when there is one match to complete it.
- Press `Tab` when several matches share a longer prefix to complete that shared prefix.
- Press `Tab` twice on the same input to list all matches for the current prefix.

Examples:

- If `help` and `hello` are available, `h<Tab>` completes to `hel`.
- If `help`, `hello`, and `happy` are available, `h<Tab>` does not change the input because `h`
  is already the longest shared prefix.
- If only `hello` matches `he`, `he<Tab>` completes to `hello`.
- `config s<Tab>` completes to `config show`.
- `hello --n<Tab>` completes to `hello --name`.

## Built-In Commands

Global options are available before any command:

- `--version`: print the configured display name and project version
- `--help`: show top-level command help
- `--help-all`: show help for all subcommands
- `-c, --config <path>`: use a non-default JSON config path

- `about`: describe the starter project
- `hello`: print a greeting; supports `--name <value>` and `-e, --enthusiastic`
- `echo`: echo required positional `text...`; supports `--uppercase` and `--numbered`
- `config init`: write a JSON config template; supports `-o, --output <path>`
- `config show`: show the effective config
- `doctor`: check basic starter layout assumptions
- `shell`: start the interactive shell explicitly

Inside the interactive shell, `help`, `exit`, and `quit` are also available.

## Configuration

The default config template is [config/cli-starter.json](config/cli-starter.json).

```json
{
  "prompt": "starter",
  "default_name": "world",
  "enabled_commands": ["about", "hello", "echo", "config", "doctor"],
  "notes": "Customize this file after copying the starter."
}
```

Use `config init` to write a config file, then edit the generated JSON for your project.
Pass `--config <path>` before the command to use a non-default config file. `config init` writes
to that path unless `--output <path>` is supplied:

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json hello
./build/cli-starter --config ./config/local.json config init --output ./starter-template.json
```

`config init` generates a template from the current `AppConfig` defaults and project metadata. It is
not a byte-for-byte copy of the checked-in template, so keep `config/cli-starter.json`, config
defaults, and generated template behavior aligned when the schema changes.

`enabled_commands` is currently serialized and shown by `config show`; it is not a runtime
allowlist and does not disable command registration. Command availability comes from the
compile-time registrars in `src/commands/register_commands.cpp`.

## Customizing The Starter

The main naming knobs are CMake cache variables:

- `CLI_STARTER_BINARY_NAME`: executable file name
- `CLI_STARTER_DISPLAY_NAME`: human-readable application name
- `CLI_STARTER_CONFIG_FILE`: default JSON config file name
- `CLI_STARTER_PROMPT_LABEL`: interactive shell prompt label

Example:

```bash
cmake -S . -B build \
  -DCLI_STARTER_BINARY_NAME=my-cli \
  -DCLI_STARTER_DISPLAY_NAME="My CLI" \
  -DCLI_STARTER_CONFIG_FILE=my-cli.json \
  -DCLI_STARTER_PROMPT_LABEL=mycli
```

After renaming, replace the sample commands with your own application behavior.
If you change `CLI_STARTER_CONFIG_FILE`, copy or rename the JSON template to
`config/<configured-name>` or pass `--config <path>` while you are migrating. If
you change `CLI_STARTER_PROMPT_LABEL`, update the JSON `prompt` field too when
you want disk-backed shell sessions to use the same prompt; a config file's
`prompt` value overrides the CMake fallback.

## Adding A Command

1. Add a command implementation under `src/commands/`.
2. Add the command `.cpp` file to the `starter_core` source list in [CMakeLists.txt](CMakeLists.txt).
3. Declare its registrar in [include/starter/commands/registrars.hpp](include/starter/commands/registrars.hpp).
4. Register it from [src/commands/register_commands.cpp](src/commands/register_commands.cpp).
5. Add tests in [tests/config_tests.cpp](tests/config_tests.cpp), or add a new test file and include
   it in the `starter_tests` target in [CMakeLists.txt](CMakeLists.txt).
6. Document user-facing behavior in this README and the nearest relevant docs, such as
   [docs/architecture.md](docs/architecture.md), [docs/testing.md](docs/testing.md), or
   [docs/troubleshooting.md](docs/troubleshooting.md).

The command parser is CLI11, so commands can use CLI11 options, flags, positional arguments, and
nested subcommands.

## Project Layout

- `src/app/`: application lifecycle, dispatch, and interactive shell flow
- `src/commands/`: built-in command implementations
- `src/core/`: shared helpers such as config loading, tokenization, and completion
- `include/starter/`: public headers for the starter
- `cmake/`: templates for generated project metadata
- `config/`: checked-in config template
- `tests/`: starter behavior tests
- `third_party/`: vendored header-only dependencies and license files
- `docs/`: onboarding, architecture, testing, troubleshooting, maintenance, and migration notes

## Testing

For normal starter behavior tests, including the doctest suite, built-executable smoke checks, and
repository hygiene checks:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

This unfiltered CTest run covers the registered entries: `starter_tests`, `cli_starter_smoke`, and
`repository_hygiene`.
The tracked GitHub Actions workflow at [.github/workflows/ci.yml](.github/workflows/ci.yml) runs the
same CMake/CTest validation on Linux and Windows.
Use the local flow above before reporting source changes.

Use a fresh ignored `build/` tree for validation. Do not cite historical `build-local-*` or
`.sandbox-user/` artifacts as proof that the current source still builds or tests cleanly.
Before treating the unfiltered CTest result as passing, check for tracked ignored artifacts:

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

If that command returns paths, `repository_hygiene` is expected to fail until those tracked generated
artifacts are removed in a cleanup change. In that state, report full validation as blocked by
artifact hygiene instead of replacing it with historical build output or a filtered test run.

With multi-config generators, build and test the same configuration:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Dependencies

- [CLI11](third_party/cli11): command-line parsing
- [nlohmann/json](third_party/nlohmann): JSON configuration
- [doctest](third_party/doctest): tests

Dependency license files are in [third_party/licenses](third_party/licenses).

## More Documentation

- [docs/project-overview.md](docs/project-overview.md): project purpose and scope
- [docs/onboarding.md](docs/onboarding.md): first local build, smoke test, and customization loop
- [docs/architecture.md](docs/architecture.md): structure and extension points
- [docs/testing.md](docs/testing.md): CTest/doctest validation flow and coverage notes
- [docs/troubleshooting.md](docs/troubleshooting.md): common local setup and runtime issues
- [docs/maintenance.md](docs/maintenance.md): maintainer checklist for command, config, dependency, and
  documentation changes
- [docs/migration-from-legacy.md](docs/migration-from-legacy.md): migration notes
- [third_party/README.md](third_party/README.md): dependency source notes

---

## 한국어

`CLI Starter`는 C++17 기반의 작은 커맨드라인 애플리케이션 템플릿입니다. 새 CLI 도구를
만들 때 빈 저장소에서 시작하지 않고, 이 프로젝트를 복사하거나 포크한 뒤 이름과 명령을
바꿔서 확장하는 용도로 만들었습니다.

포함된 기능은 다음과 같습니다.

- `cli-starter hello --name "Ada"` 같은 단발 명령 실행
- `Tab` 자동완성을 지원하는 인터랙티브 셸
- JSON 설정 파일 템플릿
- 옵션, 위치 인자, 하위명령, 검증 방식을 보여주는 예제 명령
- 명령 파싱, JSON, 테스트를 위한 header-only 의존성 내장

## 요구 사항

- CMake 3.18 이상
- C++17 컴파일러

Linux에서는 보통 `cmake`, `g++` 또는 `clang++`, `ctest`만 있으면 빌드와 테스트가
가능합니다. Windows에서는 Visual Studio, Visual Studio Build Tools, 또는 CMake가 지원하는
C++ 툴체인을 사용하면 됩니다.

## 빠른 시작

설정하고 빌드합니다.

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
```

예제 명령을 실행합니다.

```bash
./build/cli-starter hello --name "template user"
```

Visual Studio처럼 multi-config 생성기를 사용하면 실행 파일이 설정별 디렉터리 아래에 있을
수 있습니다.

```powershell
.\build\Debug\cli-starter.exe hello --name "template user"
```

무시되는 로컬 설정 파일 템플릿을 생성합니다.

```bash
./build/cli-starter --config ./config/local.json config init
```

이렇게 하면 실험 중에도 체크인된 `config/cli-starter.json` 템플릿을 바꾸지 않습니다.

인터랙티브 셸을 시작합니다.

```bash
./build/cli-starter
```

셸 안에서는 다음처럼 사용할 수 있습니다.

```text
starter> help
starter> hello --name "Ada"
starter> config show
starter> exit
```

## 인터랙티브 자동완성

인터랙티브 셸이 interactive terminal에 연결되어 있을 때 명령, 하위명령, 옵션 자동완성을
지원합니다. stdin이 redirect되었거나 terminal raw mode를 사용할 수 없으면 셸은 계속 명령을
받지만 `Tab` 자동완성 없이 일반 줄 입력으로 동작합니다.

- 후보가 하나뿐이면 `Tab`으로 완성합니다.
- 후보가 여러 개라도 더 긴 공통 prefix가 있으면 `Tab`으로 그 지점까지 완성합니다.
- 같은 입력에서 `Tab`을 두 번 누르면 현재 prefix에 맞는 후보 목록을 출력합니다.

예시:

- `help`, `hello`가 있을 때 `h<Tab>`은 `hel`까지 완성됩니다.
- `help`, `hello`, `happy`가 있을 때 `h<Tab>`은 입력을 바꾸지 않습니다. 이미 `h`가 가장 긴
  공통 prefix이기 때문입니다.
- `he`에 매칭되는 후보가 `hello` 하나뿐이면 `he<Tab>`은 `hello`로 완성됩니다.
- `config s<Tab>`은 `config show`로 완성됩니다.
- `hello --n<Tab>`은 `hello --name`으로 완성됩니다.

## 기본 명령

전역 옵션은 명령 앞에서 사용할 수 있습니다.

- `--version`: 설정된 표시 이름과 프로젝트 버전 출력
- `--help`: 최상위 명령 도움말 출력
- `--help-all`: 모든 하위명령 도움말 출력
- `-c, --config <path>`: 기본값이 아닌 JSON 설정 경로 사용

- `about`: 스타터 프로젝트 설명 출력
- `hello`: 인사 출력, `--name <value>`와 `-e, --enthusiastic` 지원
- `echo`: 필수 위치 인자 `text...` 출력, `--uppercase`와 `--numbered` 지원
- `config init`: JSON 설정 템플릿 작성, `-o, --output <path>` 지원
- `config show`: 현재 적용되는 설정 출력
- `doctor`: 기본 스타터 구조 점검
- `shell`: 인터랙티브 셸 명시적 시작

인터랙티브 셸 안에서는 `help`, `exit`, `quit`도 사용할 수 있습니다.

## 설정 파일

기본 설정 템플릿은 [config/cli-starter.json](config/cli-starter.json)에 있습니다.

```json
{
  "prompt": "starter",
  "default_name": "world",
  "enabled_commands": ["about", "hello", "echo", "config", "doctor"],
  "notes": "Customize this file after copying the starter."
}
```

`config init`으로 설정 파일을 만든 뒤, 프로젝트에 맞게 JSON 값을 수정하면 됩니다.
기본 경로가 아닌 설정 파일을 쓰려면 명령 앞에 `--config <path>`를 전달합니다.
`config init`은 `--output <path>`가 없을 때 이 경로에 설정 파일을 씁니다.

```bash
./build/cli-starter --config ./config/local.json config init
./build/cli-starter --config ./config/local.json hello
./build/cli-starter --config ./config/local.json config init --output ./starter-template.json
```

`config init`은 현재 `AppConfig` 기본값과 프로젝트 metadata에서 템플릿을 생성합니다.
체크인된 템플릿을 byte-for-byte로 복사하는 방식이 아니므로 schema를 바꿀 때는
`config/cli-starter.json`, 설정 기본값, 생성되는 템플릿 동작을 함께 맞춥니다.

`enabled_commands`는 현재 직렬화되고 `config show`에서 표시되지만, 런타임 allowlist가 아니며
명령 등록을 비활성화하지 않습니다. 명령 사용 가능 여부는 `src/commands/register_commands.cpp`의
컴파일 타임 registrar가 결정합니다.

## 스타터 커스터마이징

주요 이름 설정은 CMake cache 변수로 바꿀 수 있습니다.

- `CLI_STARTER_BINARY_NAME`: 실행 파일 이름
- `CLI_STARTER_DISPLAY_NAME`: 사용자에게 보이는 애플리케이션 이름
- `CLI_STARTER_CONFIG_FILE`: 기본 JSON 설정 파일 이름
- `CLI_STARTER_PROMPT_LABEL`: 인터랙티브 셸 프롬프트 이름

예시:

```bash
cmake -S . -B build \
  -DCLI_STARTER_BINARY_NAME=my-cli \
  -DCLI_STARTER_DISPLAY_NAME="My CLI" \
  -DCLI_STARTER_CONFIG_FILE=my-cli.json \
  -DCLI_STARTER_PROMPT_LABEL=mycli
```

이름을 바꾼 뒤에는 예제 명령을 실제 애플리케이션 동작으로 교체하면 됩니다.
`CLI_STARTER_CONFIG_FILE`을 바꾸면 JSON 템플릿도 `config/<설정한-이름>`으로 복사하거나
이름을 바꾸고, 전환 중에는 `--config <path>`를 사용합니다. `CLI_STARTER_PROMPT_LABEL`을
바꾼 뒤 디스크 설정을 사용하는 셸에서도 같은 프롬프트를 쓰려면 JSON의 `prompt` 값도 함께
바꿉니다. 설정 파일의 `prompt` 값은 CMake fallback보다 우선합니다.

## 명령 추가하기

1. `src/commands/` 아래에 명령 구현 파일을 추가합니다.
2. 명령 `.cpp` 파일을 [CMakeLists.txt](CMakeLists.txt)의 `starter_core` source list에 추가합니다.
3. [include/starter/commands/registrars.hpp](include/starter/commands/registrars.hpp)에 registrar를 선언합니다.
4. [src/commands/register_commands.cpp](src/commands/register_commands.cpp)에서 명령을 등록합니다.
5. [tests/config_tests.cpp](tests/config_tests.cpp)에 테스트를 추가하거나, 새 테스트 파일을 만들고
   [CMakeLists.txt](CMakeLists.txt)의 `starter_tests` target에 포함합니다.
6. 사용자에게 보이는 동작을 이 README와 관련 문서에 맞춰 문서화합니다. 예를 들어
   [docs/architecture.md](docs/architecture.md), [docs/testing.md](docs/testing.md),
   [docs/troubleshooting.md](docs/troubleshooting.md)를 함께 확인합니다.

명령 파서는 CLI11을 사용하므로 CLI11의 옵션, 플래그, 위치 인자, 중첩 하위명령 기능을
그대로 활용할 수 있습니다.

## 프로젝트 구조

- `src/app/`: 애플리케이션 생명주기, 명령 dispatch, 인터랙티브 셸 흐름
- `src/commands/`: 기본 명령 구현
- `src/core/`: 설정 로딩, 토큰화, 자동완성 같은 공통 helper
- `include/starter/`: 스타터용 public header
- `cmake/`: 생성되는 프로젝트 metadata용 템플릿
- `config/`: 체크인된 설정 템플릿
- `tests/`: 스타터 동작 테스트
- `third_party/`: vendored header-only 의존성과 라이선스 파일
- `docs/`: 온보딩, 아키텍처, 테스트, 문제 해결, 유지보수, 마이그레이션 노트

## 테스트

doctest suite, 빌드된 실행 파일 smoke check, repository hygiene check를 포함한 일반적인 스타터 동작 테스트는 다음 명령으로 실행합니다.

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

필터를 걸지 않은 이 CTest 실행은 등록된 `starter_tests`, `cli_starter_smoke`, `repository_hygiene`를 포함합니다.
추적되는 GitHub Actions workflow인 [.github/workflows/ci.yml](.github/workflows/ci.yml)은 Linux와
Windows에서 같은 CMake/CTest 검증을 실행합니다.
소스 변경 결과를 보고하기 전에는 위의 로컬 흐름을 사용하세요.

검증에는 새로 만든 무시 대상 `build/` 트리를 사용하세요. 과거의 `build-local-*` 또는
`.sandbox-user/` 산출물을 현재 소스가 빌드되거나 테스트된 증거로 인용하지 마세요.
필터를 걸지 않은 CTest 결과를 성공으로 간주하기 전에 추적 중인 ignored artifact가 있는지
확인합니다.

```bash
git ls-files 'build-local-*' '.sandbox-user/*'
```

이 명령이 경로를 출력하면 해당 tracked generated artifact를 별도 cleanup change에서 제거할
때까지 `repository_hygiene`가 실패하는 것이 정상입니다. 이 상태에서는 과거 build output이나
필터링된 test run으로 대체하지 말고, 전체 검증이 artifact hygiene 때문에 blocked되었다고
보고합니다.

multi-config 생성기를 사용하면 같은 설정으로 빌드하고 테스트합니다.

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## 의존성

- [CLI11](third_party/cli11): 명령줄 파싱
- [nlohmann/json](third_party/nlohmann): JSON 설정
- [doctest](third_party/doctest): 테스트

의존성 라이선스 파일은 [third_party/licenses](third_party/licenses)에 있습니다.

## 추가 문서

- [docs/project-overview.md](docs/project-overview.md): 프로젝트 목적과 범위
- [docs/onboarding.md](docs/onboarding.md): 첫 로컬 빌드, smoke test, 커스터마이징 흐름
- [docs/architecture.md](docs/architecture.md): 구조와 확장 지점
- [docs/testing.md](docs/testing.md): CTest/doctest 검증 흐름과 테스트 범위
- [docs/troubleshooting.md](docs/troubleshooting.md): 로컬 설정과 실행 중 자주 겪는 문제
- [docs/maintenance.md](docs/maintenance.md): 명령, 설정, 의존성, 문서 변경을 위한 maintainer 체크리스트
- [docs/migration-from-legacy.md](docs/migration-from-legacy.md): 마이그레이션 노트
- [third_party/README.md](third_party/README.md): 의존성 출처 정보
