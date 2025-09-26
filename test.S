#define limit 50
#define target 0x50200000

.global main
main:

# TODO: [p0-task1] Add your code here!

  # a0 is for the sum
  # t0 is the counter
  # t1 is the loop limit

  xor a0, a0, a0
  xor t0, t0, t0
  addi t1, zero, limit

loop_start:

  addi t0, t0, 1
  add a0, a0, t0
  blt t0, t1, loop_start

loop_end:

  # a1 to store the address

  la a1, target
  sd a0, (a1)

  # halt loop
  j .
