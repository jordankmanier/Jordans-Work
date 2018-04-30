	lw	0	5	ten
	lw	0	6	neg1
start	lw	0	1	num
	add	2	1	2
	sw	0	2	num
	add	5	6	5
	beq	0	5	done
	beq	0	0	start
done	halt
ten	.fill	10
neg1	.fill	-1
num	.fill	1