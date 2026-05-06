.text
addi s1, zero, 0 #initialize s1 to 0
addi s2, zero, 0 #initialize s2 to 0
addi s2, s2, 10  #loop bound set to 10
loop:
lw s4, 256(s1)   #s4 = mem[257 + s1]     
lw s5, 273(s1)   #s5 = mem[273 + s1]
add s6, s4, s5   # x(i) + y(i)
sw s6, 285(s1)   #mem[285 + s1] = s6
addi s1, s1, 1   #s1++
bne s1, s2, loop #check loop bound

addi zero, zero, 1        

.data
256: .word 10
257: .word 10
258: .word 20
259: .word 30
260: .word 40
261: .word 50
262: .word 60
263: .word 70
264: .word 80
265: .word 90
266: .word 90
267: .word 3
268: .word 3
269: .word 3
270: .word 3
271: .word 3
272: .word 4
273: .word 11
274: .word 10
275: .word 20
276: .word 30
277: .word 40
278: .word 50
279: .word 60
280: .word 70
281: .word 80
282: .word 90
283: .word 100
284: .word 3
285: .word 3
286: .word 3
287: .word 3
288: .word 3
289: .word 3
290: .word 3
291: .word 3
292: .word 3
293: .word 3
294: .word 3
295: .word 3
296: .word 3


