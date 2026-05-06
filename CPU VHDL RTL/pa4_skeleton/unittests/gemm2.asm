.text
# Load base addresses
addi x1, x0, 256      # input base
addi x2, x0, 272      # output base

# Load gemm_1 data from memory into registers
lw x5, 0(x1)        # mem[256]
lw x6, 1(x1)        # mem[257]
lw x7, 2(x1)        # mem[258]
lw x8, 3(x1)        # mem[259]
lw x9, 4(x1)        # mem[260]
lw x10, 5(x1)       # mem[261]
lw x11, 6(x1)       # mem[262]
lw x12, 7(x1)       # mem[263]

# GEMM Instruction 1 (binary representation placeholder)
gemm1

# Store gemm_1 results from registers into memory
sw x13, 0(x2)       # mem[272]
sw x14, 1(x2)       # mem[273]
sw x15, 2(x2)       # mem[274]
sw x16, 3(x2)       # mem[275]

# Load gemm_2 data from memory into registers
lw x17, 8(x1)       # mem[264]
lw x18, 9(x1)       # mem[265]
lw x19, 10(x1)      # mem[266]
lw x20, 11(x1)      # mem[267]
lw x21, 12(x1)      # mem[268]
lw x22, 13(x1)      # mem[269]
lw x23, 14(x1)      # mem[270]
lw x24, 15(x1)      # mem[271]

# GEMM Instruction 2 (binary representation placeholder)
gemm2

# Store gemm_2 results from registers into memory
sw x25, 4(x2)       # mem[276]
sw x26, 5(x2)       # mem[277]
sw x27, 6(x2)       # mem[278]
sw x28, 7(x2)       # mem[279]

# Terminate instruction
addi x0, x0, 1

.data
256: .word 1
257: .word 2
258: .word 3
259: .word 4
260: .word 1
261: .word 1
262: .word 1
263: .word 1
264: .word 1
265: .word 2
266: .word 3
267: .word 4
268: .word 1
269: .word 1
270: .word 1
271: .word 1