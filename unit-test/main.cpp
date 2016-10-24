#include <gtest/gtest.h>
#include <iostream>
extern "C" {
    void vAssertCalled( const char * s ) {
        std::cerr << s << std::endl;
        assert(1);
    }

    void uart_logf(const char *format, va_list ap) {
        printf(format,ap);
    }
    
    void get_random(int num_rand_bytes, uint8_t *rand_data) {
        
    }


}



int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

