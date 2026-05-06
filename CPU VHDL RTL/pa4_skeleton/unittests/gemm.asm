.text
# Load base addresses
addi x1, x0, 256      # input base
addi x2, x0, 264      # output base

# Load bytes from memory into registers
lw x5, 0(x1)         # mem[256]
lw x6, 1(x1)         # mem[257]
lw x7, 2(x1)         # mem[258]
lw x8, 3(x1)         # mem[259]
lw x9, 4(x1)         # mem[260]
lw x10, 5(x1)         # mem[261]
lw x11, 6(x1)         # mem[262]
lw x12, 7(x1)         # mem[263]

# GEMM Instruction 
gemm1

# Load data from registers into memory
sw x13, 0(x2)         # mem[264]
sw x14, 1(x2)         # mem[265]
sw x15, 2(x2)         # mem[266]
sw x16, 3(x2)         # mem[267]

# terminate instruction
addi zero, zero, 1 

# data memory content
.data
256: .word 1
257: .word 2
258: .word 3
259: .word 4
260: .word 1
261: .word 1
262: .word 1
263: .word 1
