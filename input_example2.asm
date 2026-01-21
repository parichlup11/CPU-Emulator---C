; Fibonacci

movr c 10
movr a 1
movr b 1
movr d 32
out a
put d

start:
	out a
	put d
	push a
	add b
	pop b
	dec c
	loop start
	movr b 10
	put b
	halt
