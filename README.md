# RV32IM 어셈블러

C++17로 구현했으며 하나의 어셈블리 소스 파일을 입력받는 RV32IM 어셈블러다. 소스를 해석한 뒤 라벨과 섹션 주소를 먼저 확정하고, 실제 명령어와 데이터를 리틀 엔디언 raw binary로 생성한다. 과제의 실제 명령어 56종과 가상명령·지시어를 처리하며, 제공된 예제 4개를 원본 수정 없이 어셈블했다.

주소와 코드가 일치하는지 검토할 수 있도록 `.bin`과 함께 선택적으로 `.lst`를 출력한다. listing에는 가상명령을 확장한 기계어, 원본 줄, 심볼의 주소·영역·공개 여부가 포함된다.

## 구현 결과 요약

| 구현에서 중점을 둔 부분 | 구현 내용 | 확인 근거 |
|---|---|---|
| 명령어 인코딩 | R/I/S/B/U/J·CSR·고정 코드의 56종 처리 | 56종 대표 입력의 기대 기계어와 출력 바이트 비교 |
| 주소 계산의 일관성 | 전방 참조, 반복 섹션, 실제 주소 기준 정렬, BSS 분리 | 라벨 주소·정렬 공간·최종 이미지 바이트 검사 |
| 가상명령 확장 | `li`의 부호·상하위 보정, `la`의 첫 PC 기준 계산, 정정된 `jal` | 실제 명령열과의 비교, 경계값과 전방 참조 검사 |
| 제공 예제 호환 | 예제에서 사용하는 수식과 추가 가상명령 지원 | 원본 4개 어셈블 성공, 예제 소스와 ZIP 원본의 바이트 일치 확인 |
| CPUlator 실행 | 제공 예제 1~4의 바이너리를 로드하여 실행 | 제출자가 네 예제의 동작을 직접 확인하고 약 9분의 시연 영상 촬영 완료 |
| 오류 처리 | 잘못된 레지스터·범위·라벨·문자열을 거부하고 소스 오류 시 출력 보존 | 실패 입력 검사와 기존 바이너리 보존 검사 |
| 검증 재현 | 검사 입력·기대값·명령·로그·출력 해시 제공 | 자동 검사 16개 메서드 통과, 상세 결과 문서 연결 |

기계어·데이터 생성과 오류 처리를 자동 검사로 확인한 뒤, 제출자가 CPUlator에서 제공 예제 1~4를 모두 실행하고 시연 영상 촬영을 완료했다. 실행·촬영 완료 보고일은 2026-11-29이며 영상 길이는 약 9분이다. 자동 검사 수치와 CPUlator 실행 결과는 [검증 결과](docs/RESULTS.md)에 정리했다.

## 제출 자료 안내

| 자료 | 내용 |
|---|---|
| [구현 설명](docs/SPECIFICATION.md) | PDF 요구사항과 코드의 대응, 자료구조, 주소·인코딩·출력 규칙 |
| [개발 과정](docs/PROCESS.md) | 초기 구현부터의 Git 이력, 시행착오, 수정 및 검토 과정 |
| [검증 결과](docs/RESULTS.md) | 자동 검사, CPUlator 예제 4개 실행·영상 촬영 결과, 재현 명령과 출력 SHA-256 |
| [실행 로그](docs/logs/verification-2026-11-28.txt) | 실제 빌드·자동 검사·예제 생성 출력 발췌 |
| [AI 사용 요청](docs/PROMPTS.md) | 실제 생성 요청, 구현·검증 지시와 수정 요청 |
| [작성 범위와 라인 수](docs/AI_USAGE.md) | AI 기여 범위와 계산 대상에 따른 보수적 추정 |
| [제공 예제](examples/) | 제공 ZIP의 원본 어셈블리 4개 |
| [자동 검사](tests/test_assembler.py) | 검증용 어셈블리 입력과 기대 기계어·오류 결과 |

검증용 작은 어셈블리 프로그램은 `tests/test_assembler.py` 안에 문자열로 들어 있으며, 검사 시 임시 `.s` 파일로 만들어 실행한다. 제공 예제와 별도로 라벨·가상명령·섹션·오류 조건을 확인한다. `input.s`는 초기 수동 확인용 파일이고 최종 검증 결과는 위 예제와 자동 검사에 근거한다.

## 빌드와 실행

Windows PowerShell에서 저장소 루트(현재 로컬 작업 경로에서는 `project` 폴더)로 이동한 뒤 실행한다. GCC의 `g++`가 PATH에 있어야 한다.

```powershell
.\build.cmd
.\assembler.exe examples\asm_example1.s -o example1.bin --list example1.lst
python tests\test_assembler.py .\assembler.exe
```

`build.cmd`는 PowerShell 스크립트 실행 정책을 바꾸지 않고 사용할 수 있다. `.ps1` 실행이 허용된 환경에서는 `build.ps1`도 사용할 수 있다. 스크립트를 사용하지 않는 경우에는 다음 명령으로 빌드할 수 있다. Linux에서는 출력 이름을 `assembler`로 바꾸고 `./assembler`로 실행한다.

```text
g++ -std=c++17 -O2 -Werror=narrowing -Werror=return-type main.cpp assembler.cpp parser.cpp operand.cpp encoder.cpp value.cpp instruction.cpp layout.cpp output.cpp -o assembler.exe
```

사용 형식은 다음과 같다.

```text
assembler input.s [-o output.bin] [--list output.lst]
```

`-o`를 생략하면 입력 파일의 확장자를 `.bin`으로 바꾼 경로에 저장한다. 입력 파일, 바이너리, listing은 서로 다른 경로여야 한다. 소스 분석과 기계어 생성이 모두 성공한 뒤에만 출력 파일을 연다. 소스 오류가 있으면 새 바이너리를 만들지 않고 기존 바이너리도 덮어쓰지 않는다. 파일 기록 중 디스크/I/O 오류가 난 경우는 오류를 보고하며, 기록 중인 출력 파일은 불완전할 수 있다.

## 파일 구성과 처리 순서

| 파일 | 역할 |
|---|---|
| `parser.cpp/.h` | 주석 제거, 라벨·명령어·피연산자 분리 |
| `operand.cpp/.h` | 레지스터, 정수, 문자, 메모리 피연산자 해석 |
| `value.cpp/.h` | 상수·심볼·수식 계산, 문자열 이스케이프 해석 |
| `encoder.cpp/.h` | 실제 명령어의 비트 조립 |
| `assembler.cpp/.h` | 소스 읽기, `.equ` 수집, 줄별 크기 계산 |
| `instruction.cpp/.h` | 가상명령 확장과 인코더 호출 |
| `layout.cpp/.h` | 섹션 배치, 라벨 주소, 데이터·기계어 바이트 생성 |
| `output.cpp/.h` | 바이너리와 listing 기록 |
| `main.cpp` | 명령행 인수와 전체 단계 연결 |
| `tests/test_assembler.py` | 독립적인 자동 검증 |

처리 순서는 `read_source → collect_constants → layout_sections → generate_image → 파일 기록`이다. 함수는 결과 구조체의 `is_success`와 `err`로 실패를 전달한다.

## 지원 범위

- RV32I 40개, M 8개, CSR 6개, `mret`, `wfi`의 실제 명령어 56개.
- 과제의 `li`, `la`, `mv`, `nop`, `not`, `neg`, `j`, `jr`, `jal`, `call`, `ret`, 조건 분기·CSR 가상명령과 제공 예제의 `csrs`, `bgtz`, `bltz`.
- `.text`, `.data`, `.bss`, `.equ`, `.globl`/`.global`, `.byte`, `.half`, `.word`, `.ascii`, `.asciz`, `.space`, `.align`.
- ABI 레지스터 이름, `x0`~`x31`, `fp`; CSR 이름 또는 번호.
- 10진·16진·2진 정수, 문자 상수, 심볼, 괄호, 단항 `+ - ~`, 이항 `* / % + - << >> & ^ |` 수식.
- 문자열과 문자에서 `\n`, `\r`, `\t`, `\b`, `\f`, `\0`, `\\`, `\'`, `\"`.

`fence`는 과제 조건대로 피연산자 없는 `0x0FF0000F`만 지원한다. `.ascii`와 `.asciz`는 한 줄에 문자열 하나를 받는다. `.half`가 홀수 주소에 놓이면 오류로 처리한다. `.word`에 자동 정렬을 삽입하지 않으며 비정렬 저장도 가능하다.

## 주소와 가상명령 규칙

- 기본 섹션은 `.text`이고 시작 주소는 `0x00010000`이다.
- `.data`는 text 끝, `.bss`는 data 끝을 각각 16바이트 경계로 올린 주소에서 시작한다. 같은 섹션이 재등장하면 이어 붙인다.
- `.align k`는 실제 주소를 `2^k` 경계로 맞춘다. 섹션 시작을 0으로 가정하여 계산하지 않는다.
- 바이너리는 text 시작부터 data 끝까지만 포함한다. text와 data 사이 정렬 공간은 0이며 BSS와 data 뒤의 BSS 정렬 공간은 파일에 넣지 않는다.
- 기본 심볼은 `TEXT_START`, `_data_start`, `_bss_start`, `_bss_end`, `_stack_top`이다. `_stack_top`은 `0x00040000`이다.
- 교수님 정정을 적용하여 `jal label`은 `jal ra, label`로 처리한다.
- `li`는 값을 RV32의 부호 있는 32비트 값으로 해석한 뒤 signed 12비트에 들어가면 한 명령, 그 외에는 `lui`와 `addi` 두 명령을 만든다. `0xFFFFFFFF`는 -1이다. 배치 시 미정인 심볼 수식은 두 명령 크기를 예약하고 나중에도 그 크기를 유지한다.
- `la`는 항상 `auipc`와 `addi` 두 명령이며 첫 번째 명령의 PC를 사용한다. `call`은 범위 안의 `jal ra` 한 명령이다.
- 분기·점프의 숫자나 `.equ`만으로 된 수식은 PC 기준 오프셋이다. 라벨 또는 기본 주소 심볼을 포함한 수식은 목적지 주소로 계산하고 현재 PC를 뺀다.

크기와 주소를 결정하는 `.space`/`.align`에는 그 시점에 계산 가능한 값이 필요하다. `.equ`끼리의 앞쪽 참조는 반복 계산으로 처리하지만 라벨 주소에 의존하는 `.equ`와 순환 정의는 지원하지 않는다. `.equ` 계산은 signed 64비트 범위를 사용한다. 줄 하나의 크기는 기존 `int byte_size` 구조에 맞춰 `INT_MAX`까지 허용하며 전체 주소는 RV32 범위를 검사한다. ELF, 재배치, 외부 링크, 일반 GNU 어셈블러의 모든 문법을 지원하는 프로그램은 아니다.

## 검증과 제출 자료

`examples/asm_example1.s`~`asm_example4.s`는 제공 ZIP에서 꺼낸 원본이다. 자동 검사는 실제 명령어와 가상명령 기계어, 배치·정렬, 문자열, 범위 오류, 잘못된 입력과 출력 보존을 확인한다. 이어서 제출자가 CPUlator에서 네 예제의 실행 동작을 직접 확인하고 영상을 촬영했다. 영상 링크는 제출 폼의 동영상 링크 항목에 기재한다.

검증 결과는 [docs/RESULTS.md](docs/RESULTS.md), 구현·검토 과정과 AI 기여는 [docs/PROCESS.md](docs/PROCESS.md), 실제 요청과 검토 지시는 [docs/PROMPTS.md](docs/PROMPTS.md)에 기록한다. 기존 파서·피연산자 처리·인코더·크기 계산을 바탕으로 남은 구현을 AI 작성 에이전트가 추가했고, 검토 에이전트가 독립 검사와 수정 요청을 수행했다. 초기 단계에서도 명령어 표·CSR 인코더·일부 오류 처리에 AI 도움을 받았으며, 작성 범위는 [AI_USAGE.md](docs/AI_USAGE.md)에 구분했다.
