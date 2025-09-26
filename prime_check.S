#define START_NUM 2
#define END_NUM 201
#define RESULT_ADDR 0x50200000
.global main
main:
  # s0: loop counter, init value = START_NUM
  # s1: mem pointer, set to RESULT_ADDR
  li s0, START_NUM
  la s1, RESULT_ADDR + 2
  li s2, END_NUM
  main_loop:
  # begin of main loop

    bge s0, s2, end_main_loop
    addi a0, s0, 0
    call is_prime
    sb a0, (s1)
    addi s1, s1, 1
    addi s0, s0, 1
    j main_loop

  # end of main loop
  end_main_loop:

    j .


# define a function to check prime number
is_prime:

  # a0: for the input num, and the output true/false
  # t0: for the counter i
  # r1: for the remainder, num % i

  // logic:
  // for (int i = 2; i < num; i++) {
  //   if (num % i == 0)
  //     return false;
  // }
  // return true;

  # check for 0 and 1
  li t0, 2
  blt a0, t0, less_than_2

  addi t0,zero,2
  begin_loop:
  bge t0,a0,endloop

  # begin of for loop
    rem t1,a0,t0
    bne t1,zero,continue_loop

      # return false
      xor a0,a0,a0
      ret

    continue_loop:
    addi t0,t0,1
    j begin_loop
  # end of for loop

  endloop:
  ret_true:
    addi a0,zero,1
    ret

  less_than_2:
    xor a0, a0, a0
    ret
