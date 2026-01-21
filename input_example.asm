; 10 even numbers

movr c 10
movr a 2
movr b 2
movr d 32

start:
	out a
	put d
	add b
	dec c
	loop start
	movr b 10
	put b
	halt
