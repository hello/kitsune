include ../makedefs.ti

CFLAGS += --gcc -I../mfilib -I./crypto \
        -Dsl_ExtLib_UART_Print=sl_ExtLib_UART_Print \
        -Dsl_ExtLib_Time_Delay=sl_ExtLib_Time_Delay \
        -Dsl_ExtLib_SHA_Init=sl_ExtLib_SHA_Init \
        -Dsl_ExtLib_SHA_Final=sl_ExtLib_SHA_Final \
        -Dsl_ExtLib_SHA_Update=sl_ExtLib_SHA_Update \
        -Dsl_ExtLib_AES_set_decrypt_key=sl_ExtLib_AES_set_decrypt_key \
        -Dsl_ExtLib_AES_set_encrypt_key=sl_ExtLib_AES_set_encrypt_key \
        -Dsl_ExtLib_CTR128_Encrypt=sl_ExtLib_CTR128_Encrypt \
        -Dsl_ExtLib_CTR128_Decrypt=sl_ExtLib_CTR128_Decrypt \
        -Dsl_ExtLib_Curve_Donna=sl_ExtLib_Curve_Donna \
	-Dxdc_target_types__=ti/targets/arm/elf/std.h -Dxdc_target_name__=M4

wac.lib: wac.o
	$(AR) r wac.lib $^

clean:
	rm -f *.o *.lib
