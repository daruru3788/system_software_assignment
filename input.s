.equ JTAG_UART_BASE, 0xFF201000
.equ STACK_TOP,      0x04000000

.text
.global _start
_start: li      sp, STACK_TOP           # 스택 포인터 설정 (어셈블러는 sp 초기값이 0)
        la      s0, TEXT_STRING
LOOP:   lbu     a0, 0(s0)               # 문자열은 0 으로 끝남
        beqz    a0, CONT
        jal     PUT_JTAG                # a0 의 문자를 전송
        j       LOOP
CONT:   jal     GET_JTAG                # 수신 문자 -> a0 (없으면 0)
        beqz    a0, CONT
        jal     PUT_JTAG                # 에코
        j       CONT

PUT_JTAG: li    t0, JTAG_UART_BASE
        lw      t1, 4(t0)               # control 레지스터
        srli    t1, t1, 8               # WSPACE
        beqz    t1, END_PUT
        sw      a0, 0(t0)               # data 레지스터에 a0 에 있는 보낼 문자 문자 쓰기
END_PUT: ret

GET_JTAG: li    t0, JTAG_UART_BASE
        lw      a0, 0(t0)               # data 레지스터 읽기 (FIFO에서 1문자 꺼냄)
        li      t1, 0
        and     t1, a0, t1              # RVALID 검사
        beqz    t1, RET_NULL
        andi    a0, a0, 0xFF            # DATA 필드만 남김, 수신 문자를 a0 로 반환
        ret
RET_NULL: li    a0, 0                   #수신 문자가 없는 경우 a0=0을 리턴
        ret

.data
TEXT_STRING:
        .asciz  "\nJTAG UART example code\n> "