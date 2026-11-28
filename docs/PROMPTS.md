# AI 사용 요청과 검토 지시 기록

아래는 이 구현 단계에서 실제 전달된 사용자 요청과 검토 지시다. 전체 대화와 내부 작업 지시를 모두 복제한 기록은 아니며, 작업 지시는 명시한 부분만 발췌했다. 생성된 코드의 AI 기여 범위는 [PROCESS.md](PROCESS.md)를 참고한다.

최종 단일 구현 에이전트에게 최초로 전달한 전체 작업 지시는 [INITIAL_IMPLEMENTATION.txt](prompts/INITIAL_IMPLEMENTATION.txt)에 별도로 보존했다. 아래 발췌는 그 지시와 후속 검증·수정 메시지의 주요 부분이다. 앞선 학습 대화 전체의 원문을 새로 만들어 넣지는 않았다.

## 남은 구현을 맡긴 사용자 요청 원문

> 그냥 나머지도 딱 내 코드 수준으로 너가 짜주면 안돼 완성까지? 너무 힘든데

## 작성·검토 방식에 관한 사용자 요청 원문

> 그렇게 하지 말고 코드를 짜는 에이전트를 하나 만드는데 내가 자주 틀린 부분이라던지 자주 쓰는 스타일 비효율적이더라도 어떻게 하는지 자세히 묘사해서 그렇게 코드를 짜게 해 그리고 너가 틀린 부분 짚어주고 다시 에이전트가 수정하는 식으로 반복해서 생성해줘. 목표는 AI를 전혀 쓴것 같지 않은 코드야

이 요청에 대해 기존 코드의 문법과 구조를 맞추되 AI가 관여하지 않은 것처럼 보인다고 보장하지 않고, 실제 작성 기여를 기록한다는 조건으로 작업했다. 실제 버그를 흉내 내거나 작성 이력을 꾸미지는 않았다.

## 구현 에이전트 지시 발췌

> You are the only implementation agent. Root will inspect/test your code and send fixes for YOU to apply. Complete implementation, then iterate until root checks pass.

> student uses free functions, plain structs with fields + err string + is_success bool, *_rtn result names, snake_case function/variable names, aggregate return {value,"",true} and early error returns. cpp uses using namespace std; headers use std::. Header include guards, .h/.cpp by responsibility. Ordinary if/else-if chains and switch, for loops and push_back, std::string::substr/find, explicit temporary variables like instruction, integer_v, const_map, num, rtn, cnt.

> Passing/copying strings and structs by value and some repeated branches is okay; don't optimize gratuitously. Four-space indentation, mostly short English error strings, line N error prefixes. Avoid custom classes/inheritance, visitors, templates, std::variant, exceptions as normal control flow, fluent APIs, dense lambdas, excessive helper layers, wholesale naming/formatting rewrites, and large speculative infrastructure. Suitable helpers are small named functions with concrete purposes. Correct integer types/casts are required even if newer to student.

> Do NOT use old rollback_backups solution. Do NOT commit/push. Do not spawn agents. Preserve existing implementation where practical.

## 검증 지시 발췌

> Root test suite is now project/tests/test_assembler.py (do not edit expectations yourself; send discrepancies for review). Run `python tests/test_assembler.py <built-exe>` when CLI ready. It checks 56 real encodings, all implemented pseudos equivalence, byte layouts/BSS omission/alignment>16, branch equ offset, expressions, malformed input, original examples, and preserving previous output.

## 실제 수정 요청

> First concrete review finding to fix: `: nop` and a line containing only `:` currently pass parser/line_parse with empty label; read_source then accepts nop or skips the line. PDF requires a nonempty valid label, so detect an empty label before colon and report its line. Please handle this in your implementation (root won't edit source).

> Style review correction: output.cpp write_file currently adds chrono-based temporary names, backup rename/restore transaction (~40 lines). This is beyond the student's established style and assignment needs. Please simplify output writing to straightforward ofstream open/write/close and explicit errors, with ALL source validation/encoding completed before any output is opened. Preserve existing .bin on assembly errors (root test), detect input/output/list path collisions.

> Actual final usability failure: `./build.ps1` is blocked here by PowerShell execution policy (PSSecurityException). Don't change machine policy. Please add a simple build.cmd (pushd %~dp0, same g++ command, preserve exit code/popd) and make README/docs primary Windows build command `.\build.cmd`; retain direct g++ fallback and optional ps1.

수정 요청은 구현 에이전트가 적용하고 검토 에이전트가 다시 빌드·검사하는 방식으로 진행했다. 전체 자동 검사 통과와 원본 예제 어셈블 성공은 [RESULTS.md](RESULTS.md)에 기록했다.

## 제출 문서 정리 요청

사용자는 제출 항목을 다음과 같이 제시하고 코드와 개발·검증 문서가 들어 있는 저장소를 준비하도록 요청했다.

> Github Repo에 개발과 관련된 모든 노력을 담아 문서로 만들어 넣고, 코드와 함께 (검증을 위한 어셈블리 코드 포함) 제출

> 이것만 준비하면 돼 pdf 읽어서 나머지 정보는 충당해

이 단계에서는 제품 코드를 변경하지 않고 PDF와 현재 저장소를 대조했다. 초기 커밋 이력, 시행착오, 요구사항 대응 설명, 기존 실제 실행 로그, 예제 원본 비교 및 출력 SHA-256, 계산 범위에 따른 AI 라인 수 추정을 문서로 보완했다. 확인하지 않은 하드웨어 실행을 성공으로 바꾸거나, AI 기여를 사용자 작성으로 재분류하지 않았다.
