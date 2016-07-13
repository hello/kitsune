include ../makedefs.ti

CFLAGS +=   -Dsl_ExtLib_I2C_Open=sl_ExtLib_I2C_Open \
            -Dsl_ExtLib_I2C_Read=sl_ExtLib_I2C_Read \
            -Dsl_ExtLib_I2C_Write=sl_ExtLib_I2C_Write \
            -Dsl_ExtLib_I2C_Close=sl_ExtLib_I2C_Close \
            -Dsl_ExtLib_Time_Delay=sl_ExtLib_Time_Delay

mfi.lib: MFi.o
	$(AR) r mfi.lib $^

clean:
	rm -f *.o *.lib
