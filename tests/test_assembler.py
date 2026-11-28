"""Run the built assembler against independent encoding and integration checks."""
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSEMBLER = pathlib.Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "assembler.exe"


def words(values):
    return b"".join(struct.pack("<I", value & 0xFFFFFFFF) for value in values)


def padded(data):
    return data + bytes((-len(data)) % 16)


class AssemblerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rv32-test-")
        self.addCleanup(self.temp.cleanup)
        self.folder = pathlib.Path(self.temp.name)

    def assemble(self, source, name="test", listing=False):
        path = self.folder / (name + ".s")
        path.write_text(source, encoding="utf-8")
        output = self.folder / (name + ".bin")
        command = [str(ASSEMBLER), str(path), "-o", str(output)]
        if listing:
            command += ["--list", str(self.folder / (name + ".lst"))]
        result = subprocess.run(command, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(output.exists())
        return output.read_bytes()

    def rejected(self, source, name="bad"):
        path = self.folder / (name + ".s")
        path.write_text(source, encoding="utf-8")
        output = self.folder / (name + ".bin")
        result = subprocess.run([str(ASSEMBLER), str(path), "-o", str(output)],
                                capture_output=True, text=True, timeout=20)
        self.assertNotEqual(result.returncode, 0, source)
        self.assertFalse(output.exists(), source)
        self.assertTrue(result.stderr.strip(), source)

    def test_known_machine_words(self):
        source = """add a0, a1, a2
addi sp, sp, -16
lw ra, 12(sp)
sw t0, 8(sp)
beq a0, a1, +8
jal ra, +8
lui t0, 0xFF200
auipc t1, 0
ecall
ebreak
fence
mret
wfi
"""
        expected = [0x00C58533, 0xFF010113, 0x00C12083, 0x00512423,
                    0x00B50463, 0x008000EF, 0xFF2002B7, 0x00000317,
                    0x00000073, 0x00100073, 0x0FF0000F, 0x30200073, 0x10500073]
        self.assertEqual(self.assemble(source), padded(words(expected)))

    def test_all_real_instruction_formats(self):
        instructions, expected = [], []
        r_codes = [("add", 0, 0), ("sub", 0, 32), ("sll", 1, 0), ("slt", 2, 0),
                   ("sltu", 3, 0), ("xor", 4, 0), ("srl", 5, 0), ("sra", 5, 32),
                   ("or", 6, 0), ("and", 7, 0)]
        r_codes += [(name, i, 1) for i, name in enumerate(
            ["mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"])]
        for name, funct3, funct7 in r_codes:
            instructions.append(f"{name} x31, x30, x29")
            expected.append((funct7 << 25) | (29 << 20) | (30 << 15) | (funct3 << 12) | (31 << 7) | 0x33)
        for name, funct3 in [("addi", 0), ("slti", 2), ("sltiu", 3), ("xori", 4), ("ori", 6), ("andi", 7)]:
            instructions.append(f"{name} x31, x30, -2048")
            expected.append((0x800 << 20) | (30 << 15) | (funct3 << 12) | (31 << 7) | 0x13)
        for name, funct3, imm in [("slli", 1, 31), ("srli", 5, 31), ("srai", 5, 0x41F)]:
            instructions.append(f"{name} x31, x30, 31")
            expected.append((imm << 20) | (30 << 15) | (funct3 << 12) | (31 << 7) | 0x13)
        for name, funct3 in [("lb", 0), ("lh", 1), ("lw", 2), ("lbu", 4), ("lhu", 5), ("jalr", 0)]:
            instructions.append(f"{name} x31, -1(x30)")
            opcode = 0x67 if name == "jalr" else 3
            expected.append((0xFFF << 20) | (30 << 15) | (funct3 << 12) | (31 << 7) | opcode)
        for name, funct3 in [("sb", 0), ("sh", 1), ("sw", 2)]:
            instructions.append(f"{name} x29, -1(x30)")
            expected.append((127 << 25) | (29 << 20) | (30 << 15) | (funct3 << 12) | (31 << 7) | 0x23)
        for name, funct3 in [("beq", 0), ("bne", 1), ("blt", 4), ("bge", 5), ("bltu", 6), ("bgeu", 7)]:
            instructions.append(f"{name} x30, x29, -4")
            expected.append((1 << 31) | (63 << 25) | (29 << 20) | (30 << 15) | (funct3 << 12) | (14 << 8) | (1 << 7) | 0x63)
        instructions += ["lui x31, 0xfffff", "auipc x31, -1", "jal x31, -4"]
        expected += [0xFFFFFFB7, 0xFFFFFF97, 0xFFDFFFEF]
        for name, funct3 in [("csrrw", 1), ("csrrs", 2), ("csrrc", 3),
                             ("csrrwi", 5), ("csrrsi", 6), ("csrrci", 7)]:
            source = "31" if funct3 >= 5 else "x31"
            instructions.append(f"{name} x31, 0xfff, {source}")
            expected.append((0xFFF << 20) | (31 << 15) | (funct3 << 12) | (31 << 7) | 0x73)
        for name, code in [("ecall", 0x73), ("ebreak", 0x100073), ("fence", 0xFF0000F),
                           ("mret", 0x30200073), ("wfi", 0x10500073)]:
            instructions.append(name)
            expected.append(code)
        self.assertEqual(len(instructions), 56)
        self.assertEqual(self.assemble("\n".join(instructions)), padded(words(expected)))

    def test_pseudo_expansion_matches_real_instructions(self):
        pseudo = """li t0, -1
li t1, 0x12345fff
li t2, 4096
mv a0, t0
nop
not a1, a0
neg a2, a1
csrr t0, mstatus
csrw mtvec, t0
csrs mie, t0
csrsi mstatus, 8
csrci mstatus, 8
beqz a0, done
bnez a0, done
bgt a0, a1, done
ble a0, a1, done
bgtu a0, a1, done
bleu a0, a1, done
bltz a0, done
bgtz a0, done
jal done
call done
j done
done: jr t0
ret
"""
        real = """addi t0, x0, -1
lui t1, 0x12346
addi t1, t1, -1
lui t2, 1
addi t2, t2, 0
addi a0, t0, 0
addi x0, x0, 0
xori a1, a0, -1
sub a2, x0, a1
csrrs t0, mstatus, x0
csrrw x0, mtvec, t0
csrrs x0, mie, t0
csrrsi x0, mstatus, 8
csrrci x0, mstatus, 8
beq a0, x0, done
bne a0, x0, done
blt a1, a0, done
bge a1, a0, done
bltu a1, a0, done
bgeu a1, a0, done
blt a0, x0, done
blt x0, a0, done
jal ra, done
jal ra, done
jal x0, done
done: jalr x0, 0(t0)
jalr x0, 0(ra)
"""
        self.assertEqual(self.assemble(pseudo, "pseudo"), self.assemble(real, "real"))

    def test_li_boundaries(self):
        pseudo = "li t0, -2048\nli t0, 2047\nli t0, 2048\nli t0, -2049\nli t0, 0xffffffff\nli t0, 0x80000000\n"
        real = "addi t0,x0,-2048\naddi t0,x0,2047\nlui t0,1\naddi t0,t0,-2048\nlui t0,-1\naddi t0,t0,2047\naddi t0,x0,-1\nlui t0,0x80000\naddi t0,t0,0\n"
        self.assertEqual(self.assemble(pseudo, "li"), self.assemble(real, "real"))

    def test_la_uses_first_instruction_pc(self):
        source = ".text\nnop\nla t0, target\nnop\n.data\ntarget: .word 123\n"
        self.assertEqual(self.assemble(source), words([0x13, 0x297, 0x00C28293, 0x13, 123]))

    def test_li_forward_expression_does_not_change_layout(self):
        source = "li t0, end - TEXT_START\nend: nop\n.data\n.word end\n"
        data = self.assemble(source)
        self.assertEqual(data, words([0x2B7, 0x00828293, 0x13, 0, 0x10008]))

    def test_sections_data_and_bss_symbols(self):
        source = r'''.text
nop
.data
bytes: .byte -128, 255, 'A'
.ascii "x,#:"
.asciz "\n"
.align 1
.half -32768, 65535
.word -2147483648, 4294967295, bytes, TEXT_START, _data_start, _bss_start, _bss_end, _stack_top
.bss
.align 4
buf: .space 19
'''
        expected = padded(words([0x13])) + b"\x80\xffAx,#:\n\x00\x00" + b"\x00\x80\xff\xff"
        expected += words([0x80000000, 0xFFFFFFFF, 0x10010, 0x10000, 0x10010, 0x10040, 0x10053, 0x40000])
        self.assertEqual(self.assemble(source), expected)

    def test_repeated_sections_and_actual_address_alignment(self):
        source = ".text\nnop\n.data\na: .byte 1\n.align 5\nb: .byte 2\n.text\nnop\n.data\nc: .byte 3\n.align 2\n.word a,b,c\n"
        expected = padded(words([0x13, 0x13])) + b"\x01" + bytes(15) + b"\x02\x03\x00\x00"
        expected += words([0x10010, 0x10020, 0x10021])
        self.assertEqual(self.assemble(source), expected)

    def test_label_on_section_switch(self):
        source = "nop\nold: .data\n.word old\n.data\na: .byte 1\nback: .text\nnop\n.data\n.word back\n"
        self.assertEqual(self.assemble(source), padded(words([0x13, 0x13])) + words([0x10004]) + b"\x01" + words([0x10015]))

    def test_expressions_constants_and_memory(self):
        source = ".equ A, B + 1\n.equ B, 7\n.equ OFF, 8\nli t0, (1 << A) | B\naddi a0,a0,-'a'+10\nlw t1, (A-4)( sp )\nbeq a0,a1,OFF\n.data\n.word 240*512/4, 2+3*4, (2+3)*4, -8>>2, ~0\n"
        real = "addi t0,x0,263\naddi a0,a0,-87\nlw t1,4(sp)\nbeq a0,a1,8\n.data\n.word 30720,14,20,-2,-1\n"
        self.assertEqual(self.assemble(source, "expr"), self.assemble(real, "real"))

    def test_string_escapes_and_comment_characters(self):
        source = r'''.data
.ascii "a,#:b\n\t\r\b\f\0\\\"\'"
.asciz ""
'''
        self.assertEqual(self.assemble(source), b"a,#:b\n\t\r\b\f\0\\\"'\0")

    def test_default_output_and_listing(self):
        source = self.folder / "default.s"
        source.write_text(".global _start\n_start: nop\n", encoding="utf-8")
        listing = self.folder / "default.lst"
        result = subprocess.run([str(ASSEMBLER), str(source), "--list", str(listing)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(source.with_suffix(".bin").read_bytes(), padded(words([0x13])))
        text = listing.read_text(encoding="utf-8")
        self.assertIn("00010000", text)
        self.assertIn("00000013", text)
        self.assertIn("_start", text)
        self.assertRegex(text, r"(?m)^(?=.*\b_start\b)(?=.*\bG\b).*$")

    def test_invalid_inputs_leave_no_binary(self):
        sources = ["j missing", "x: nop\nx: nop", "TEXT_START: nop", ".equ X,1\nX:nop",
                   ".equ X,1\n.equ X,2", ".equ A,B\n.equ B,A", "1bad: nop", ".equ bad!,1", ": nop", ":",
                   "addi t0,t0,2048", "lw t0,-2049(sp)", "slli t0,t0,32", "srai t0,t0,-1",
                   "beq a0,a1,4096", "beq a0,a1,3", "jal ra,1048576", "jal ra,3",
                   "add x32,x0,x0", "add t0,t1", "add t0,t1,t2,t3", "nop x0", "fence iorw,iorw",
                   "csrrwi t0,mstatus,32", "csrrw t0,4096,t1", "csrrw t0,sstatus,t1",
                   "fadd.s f0,f1,f2", ".section .text", ".byte", ".half", ".word",
                   ".byte 256", ".byte -129", ".half 65536", ".half -32769",
                   ".word 4294967296", ".word -2147483649", "li t0,4294967296",
                   ".space", ".space -1", ".space 1,2", ".space UNKNOWN", ".space 2147483648",
                   ".align -1", ".align 64", ".align UNKNOWN", ".align 32",
                   ".ascii", ".ascii 123", ".ascii \"a\",\"b\"", r'.ascii "\q"',
                   '.ascii "a" "b"', '.ascii "unclosed', ".asciz 'A'", ".asciz",
                   ".bss\n.word 1", ".bss\n.ascii \"x\"", ".bss\nnop", ".data\nnop",
                   ".text 1", ".equ X", ".global", "addi t0,t0,1/0", "li t0,1<<64",
                   ".equ X,9223372036854775807+1", ".equ X,-9223372036854775808/-1",
                   ".equ X,3037000500*3037000500", ".equ X,-9223372036854775808-1"]
        for i, source in enumerate(sources):
            with self.subTest(source=source):
                self.rejected(source + "\n", f"bad{i}")

    def test_error_preserves_existing_output(self):
        source = self.folder / "bad.s"
        source.write_text("j missing\n", encoding="utf-8")
        output = self.folder / "bad.bin"
        output.write_bytes(b"previous valid result")
        result = subprocess.run([str(ASSEMBLER), str(source), "-o", str(output)], capture_output=True, text=True, timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_bytes(), b"previous valid result")
        self.assertRegex(result.stderr, r"(?i)line\s+1")

    def test_output_cannot_overwrite_input(self):
        source = self.folder / "same.s"
        original = b"nop\n"
        source.write_bytes(original)
        result = subprocess.run([str(ASSEMBLER), str(source), "-o", str(source)], capture_output=True, text=True, timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(source.read_bytes(), original)

    def test_original_examples(self):
        sources = sorted((ROOT / "examples").glob("asm_example*.s"))
        self.assertEqual(len(sources), 4)
        for source in sources:
            with self.subTest(source=source.name):
                data = self.assemble(source.read_text(encoding="utf-8-sig"), source.stem, listing=True)
                self.assertGreater(len(data), 100)


if __name__ == "__main__":
    if not ASSEMBLER.is_file():
        raise SystemExit(f"Build the assembler first, then run: python tests/test_assembler.py <exe path>\nMissing: {ASSEMBLER}")
    unittest.main(verbosity=2)
