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
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
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

Generate a starter config:

```bash
./build/cli-starter config init
```

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

The interactive shell supports command, subcommand, and option completion.

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

- `about`: describe the starter project
- `hello`: print a greeting and demonstrate command options
- `echo`: echo positional arguments, with formatting flags
- `config init`: write a JSON config template
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
  -DCLI_STARTER_PROMPT_LABEL=mycli
```

After renaming, replace the sample commands with your own application behavior.

## Adding A Command

1. Add a command implementation under `src/commands/`.
2. Declare its registrar in [include/starter/commands/registrars.hpp](include/starter/commands/registrars.hpp).
3. Register it from [src/commands/register_commands.cpp](src/commands/register_commands.cpp).
4. Add tests in [tests/config_tests.cpp](tests/config_tests.cpp) or a new test file.
5. Document user-facing behavior in this README or [docs/architecture.md](docs/architecture.md).

The command parser is CLI11, so commands can use CLI11 options, flags, positional arguments, and
nested subcommands.

## Project Layout

- `src/app/`: application lifecycle, dispatch, and interactive shell flow
- `src/commands/`: built-in command implementations
- `src/core/`: shared helpers such as config loading, tokenization, and completion
- `include/starter/`: public headers for the starter
- `config/`: checked-in config template
- `tests/`: starter behavior tests
- `third_party/`: vendored header-only dependencies and license files
- `docs/`: architecture and migration notes

## Testing

For normal starter behavior tests:

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

With multi-config generators, add the configuration:

```powershell
ctest --test-dir build -C Debug -R starter_tests --output-on-failure
```

## Dependencies

- [CLI11](third_party/cli11): command-line parsing
- [nlohmann/json](third_party/nlohmann): JSON configuration
- [doctest](third_party/doctest): tests

Dependency license files are in [third_party/licenses](third_party/licenses).

## More Documentation

- [docs/project-overview.md](docs/project-overview.md): project purpose and scope
- [docs/architecture.md](docs/architecture.md): structure and extension points
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
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
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

설정 파일 템플릿을 생성합니다.

```bash
./build/cli-starter config init
```

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

인터랙티브 셸은 명령, 하위명령, 옵션 자동완성을 지원합니다.

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

- `about`: 스타터 프로젝트 설명 출력
- `hello`: 옵션 사용 예시를 보여주는 인사 명령
- `echo`: 위치 인자와 출력 옵션 예시
- `config init`: JSON 설정 템플릿 작성
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
  -DCLI_STARTER_PROMPT_LABEL=mycli
```

이름을 바꾼 뒤에는 예제 명령을 실제 애플리케이션 동작으로 교체하면 됩니다.

## 명령 추가하기

1. `src/commands/` 아래에 명령 구현 파일을 추가합니다.
2. [include/starter/commands/registrars.hpp](include/starter/commands/registrars.hpp)에 registrar를 선언합니다.
3. [src/commands/register_commands.cpp](src/commands/register_commands.cpp)에서 명령을 등록합니다.
4. [tests/config_tests.cpp](tests/config_tests.cpp) 또는 새 테스트 파일에 테스트를 추가합니다.
5. 사용자에게 보이는 동작을 이 README나 [docs/architecture.md](docs/architecture.md)에 문서화합니다.

명령 파서는 CLI11을 사용하므로 CLI11의 옵션, 플래그, 위치 인자, 중첩 하위명령 기능을
그대로 활용할 수 있습니다.

## 프로젝트 구조

- `src/app/`: 애플리케이션 생명주기, 명령 dispatch, 인터랙티브 셸 흐름
- `src/commands/`: 기본 명령 구현
- `src/core/`: 설정 로딩, 토큰화, 자동완성 같은 공통 helper
- `include/starter/`: 스타터용 public header
- `config/`: 체크인된 설정 템플릿
- `tests/`: 스타터 동작 테스트
- `third_party/`: vendored header-only 의존성과 라이선스 파일
- `docs/`: 아키텍처와 마이그레이션 노트

## 테스트

일반적인 스타터 동작 테스트는 다음 명령으로 실행합니다.

```bash
cmake -S . -B build -DCLI_STARTER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R starter_tests --output-on-failure
```

multi-config 생성기를 사용하면 설정 이름을 추가합니다.

```powershell
ctest --test-dir build -C Debug -R starter_tests --output-on-failure
```

## 의존성

- [CLI11](third_party/cli11): 명령줄 파싱
- [nlohmann/json](third_party/nlohmann): JSON 설정
- [doctest](third_party/doctest): 테스트

의존성 라이선스 파일은 [third_party/licenses](third_party/licenses)에 있습니다.

## 추가 문서

- [docs/project-overview.md](docs/project-overview.md): 프로젝트 목적과 범위
- [docs/architecture.md](docs/architecture.md): 구조와 확장 지점
- [docs/migration-from-legacy.md](docs/migration-from-legacy.md): 마이그레이션 노트
- [third_party/README.md](third_party/README.md): 의존성 출처 정보
