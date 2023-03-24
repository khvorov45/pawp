mov(RegisterMemory_ToFrom_Register)         | 100010 d(1) w(1) | mod(2) reg(3) r_m(3) | disp_lo | disp_hi
mov(Immediate_To_RegisterMemory)            | 1100011 w(1) | mod(2) 000 r_m(3) | disp_lo | disp_hi | data_lo | data_hi
mov(Immediate_To_Register)                  | 1011 w(1) reg(3) | data_lo | data_hi
mov(Memory_To_Accumulator)                  | 1010000 w(1) | addr_lo | addr_hi
mov(Accumulator_To_Memory)                  | 1010001 w(1) | addr_lo | addr_hi
add(RegisterMemory_With_Register_To_Either) | 000000 d(1) w(1) | mod(2) reg(3) r_m(3) | disp_lo | disp_hi
add(Immediate_To_RegisterMemory)            | 100000 s(1) w(1) | mod(2) 000 r_m(3) | disp_lo | disp_hi | data_lo | data_hi
add(Immediate_To_Accumulator)               | 0000010 w(1) | data_lo | data_hi
sub(RegisterMemory_With_Register_To_Either) | 001010 d(1) w(1) | mod(2) reg(3) r_m(3) | disp_lo | disp_hi
sub(Immediate_From_RegisterMemory)          | 100000 s(1) w(1) | mod(2) 101 r_m(3) | disp_lo | disp_hi | data_lo | data_hi
sub(Immediate_From_Accumulator)             | 0010110 w(1) | data_lo | data_hi
cmp(RegisterMemory_And_Register)            | 001110 d(1) w(1) | mod(2) reg(3) r_m(3) | disp_lo | disp_hi
cmp(Immediate_With_RegisterMemory)          | 100000 s(1) w(1) | mod(2) 111 r_m(3) | disp_lo | disp_hi | data_lo | data_hi
cmp(Immediate_With_Accumulator)             | 0011110 w(1) | data_lo | data_hi
je(Jump)                                    | 01110100 | ip_inc8
jl(Jump)                                    | 01111100 | ip_inc8
jle(Jump)                                   | 01111110 | ip_inc8
jb(Jump)                                    | 01110010 | ip_inc8
jbe(Jump)                                   | 01110110 | ip_inc8
jp(Jump)                                    | 01111010 | ip_inc8
jo(Jump)                                    | 01110000 | ip_inc8
js(Jump)                                    | 01111000 | ip_inc8
jne(Jump)                                   | 01110101 | ip_inc8
jnl(Jump)                                   | 01111101 | ip_inc8
jnle(Jump)                                  | 01111111 | ip_inc8
jnb(Jump)                                   | 01110011 | ip_inc8
jnbe(Jump)                                  | 01110111 | ip_inc8
jnp(Jump)                                   | 01111011 | ip_inc8
jno(Jump)                                   | 01110001 | ip_inc8
jns(Jump)                                   | 01111001 | ip_inc8
loop(Loop)                                  | 11100010 | ip_inc8
loopz(Loop)                                 | 11100001 | ip_inc8
loopnz(Loop)                                | 11100000 | ip_inc8
jcxz(Jump)                                  | 11100011 | ip_inc8
