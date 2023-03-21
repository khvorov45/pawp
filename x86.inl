mov(RegisterMemory_ToFrom_Register) | 100010 d(1) w(1) | mod(2) reg(3) r_m(3) | disp_lo | disp_hi
mov(Immediate_To_RegisterMemory)    | 1100011 w(1) | mod(2) 000 r_m(3) | disp_lo | disp_hi | data_lo | data_hi
mov(Immediate_To_Register)          | 1011 w(1) reg(3) | data_lo | data_hi
mov(Memory_To_Accumulator)          | 1010000 w(1) | addr_lo | addr_hi
