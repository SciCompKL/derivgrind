// Some bit-trick tests require external libraries like libgcrypt, libz. 
// Compile with -DTRICKTEST_NO_EXTERNAL_DEPENDENCIES in order to disable these tests.
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif
#include <iostream>
#include <cmath>
#include <map>
#ifndef TRICKTEST_NO_EXTERNAL_DEPENDENCIES
  #include <gcrypt.h> // for the encryption-decryption test
  #include <zlib.h> // for the compression-inflation test
  #include <sys/mman.h> // for the multiple-mmap test
  #include <unistd.h> // also for the multiple-mmap test
#endif
#include "valgrind/derivgrind.h"

template<typename fp>
void bittrick_input(fp& a){
  char mode = DG_GET_MODE;
  if(mode=='d'){
    fp const one=1.0;
    DG_SET_DOTVALUE(&a,&one,sizeof(fp));
  } else if(mode=='b'){
    DG_INPUTF(a);
  } else if(mode=='t'){
    DG_MARK_FLOAT(a);
  }
}

template<typename fp>
void bittrick_output(fp& var, fp expected_value, fp expected_derivative){
  DG_DISABLE(1,0); // keep the bit-trick finder from emitting warnings at this place
  if( std::fabs((var-expected_value)/expected_value) > 1e-6 ){
    std::cout << "WRONG VALUE: computed=" << var << " expected=" << expected_value << std::endl;
  }
  DG_DISABLE(0,1);
  char mode = DG_GET_MODE;
  if(mode=='d'){
    fp derivative = 0.0;
    DG_GET_DOTVALUE(&var,&derivative,sizeof(fp));
    if( std::fabs((derivative-expected_derivative)/expected_derivative) > 1e-6 ){
      std::cout << "WRONG FORWARD-MODE DERIVATIVE: computed=" << derivative << " expected=" << expected_derivative << std::endl;
    }
  } else if(mode=='b'){
    DG_OUTPUTF(var);
  } else if(mode=='t'){
    // just use the output somehow
    fp volatile var2 = var;
    var2 += (fp)1.0;
  }
}

using ul = unsigned long long;
using ui = unsigned int;

ul as_ul(double val){ return *(ul*)&val; }
ui as_ui(float val){ return *(ui*)&val; }
double as_double(ul val){ return *(double*)&val; }
float as_float(ui val){ return *(float*)&val;} 

/*! Bit-trick: Multiply with a power of two by making an integer addition 
 * to the exponent bits.
 */
void integer_addition_to_exponent_double(){
  double a = 2.7;
  bittrick_input(a);
  ul exponent = 5;
  double b = as_double( as_ul(a) + (exponent<<52) );
  bittrick_output(b,2.7*32.0,32.0);
}

/*! Bit-trick: See integer_addition_to_exponent_double.
 */
void integer_addition_to_exponent_float(){
  float a = -2.7;
  bittrick_input(a);
  ui exponent = 3;
  float b = as_float( as_ui(a) + (exponent<<23) );
  bittrick_output(b,-2.7f*8.0f,8.0f);
}

/*! Bit-trick: Perform frexp by overwriting exponent bytes with 0b01111111110.
 * 
 * These eleven bits are the exponent bits of all numbers between 0.5 (inclusive)
 * and 1.0 (exclusive). Setting them this way multiplied the value with a power of two,
 * chosen such that the result ends up between 0.5 and 1.0.
 */
void incomplete_masking_to_perform_frexp_double(){
  double a = 38.1;
  bittrick_input(a);
  double b = as_double( (as_ul(a) & 0x800ffffffffffffful) | 0x3fe0000000000000ul );
  bittrick_output(b, 38.1/64.0, 1/64.0);
}

/*! Bit-trick: Perform frexpf by overwriting exponent bytes with 0b01111110.
 *
 * See incomplete_masking_to_perform_frexp_float.
 */
void incomplete_masking_to_perform_frexp_float(){
  float a = -38.1;
  bittrick_input(a);
  float b = as_float( (as_ui(a) & 0x807fffff) | 0x3f000000 );
  bittrick_output(b, -38.1f/64.0f, 1/64.0f);
}

/*! Bit-trick: Round a double to an integer by adding 1.5*2^52 and
 * subtracting it right away.
 * 
 * Between 1.0*2^52 and 2.0*2^52, the real number representable by double are
 * precisely the integers. Thus, when adding a number smaller than 2^51 in magnitude, 
 * the result will be rounded to an integer, according to the present rounding mode.
 * For this example to work, it should be "round to nearest". 
 *
 * If the rounding does not work (i.e. b is 123.456 and not 123.000), try the 
 * following fixes:
 *  - compile with -O0 to avoid compiler optimizations
 *  - compile with -mfpmath=sse to avoid using x87 arithmetic, which can have higher precision
 */
void exploiting_imprecision_for_rounding_double(){
  double a = 123.456;
  bittrick_input(a);
  double b = (a + 0x1.8p52) - 0x1.8p52;
  bittrick_output(b,123.0,0.0);
}

/*! Bit-trick: Round a float to an integer by adding 1.5*2^23 and
 * subtracting it right away.
 *
 * See explointing_imprecision_for_rounding_float.
 */
void exploiting_imprecision_for_rounding_float(){
  float a = 123.456;
  bittrick_input(a);
  float b = (a + 0x1.8p23f) - 0x1.8p23f;
  bittrick_output(b,123.0f,0.0f);
}


#ifndef TRICKTEST_NO_EXTERNAL_DEPENDENCIES
/*! Bit-trick: Binary identity, encryption followed by decryption.
 *
 * We use the gcrypt library with the twofish cipher, as the AES ciphers 
 * could be implemented with special AES instructions and we don't want to
 * test those.
 */
gcry_cipher_hd_t get_hd(){
  gcry_cipher_hd_t hd;
  gcry_cipher_open(&hd, GCRY_CIPHER_TWOFISH, GCRY_CIPHER_MODE_CBC, 0);
  gcry_cipher_setkey(hd, "some 256-bit secret stored here ", 32);
  return hd;
}
void encrypt_decrypt(){
  double x = 3.14159;
  bittrick_input(x);
  char plaintext[32], ciphertext[32];
  ((double*)plaintext)[0] = x;
  gcry_cipher_encrypt(get_hd(),
    ciphertext, 32, plaintext, 32);
  gcry_cipher_decrypt(get_hd(),
    plaintext, 32, ciphertext, 32);
  double y = ((double*)plaintext)[0];
  bittrick_output(y,3.14159,1.0);
}
#endif

#ifndef TRICKTEST_NO_EXTERNAL_DEPENDENCIES
/*! Bit-trick: Binary identity, compression followed by inflation.
 *
 * We use the zlib library implementing the DEFLATE algorithm.
 * If for the given "plaintext", the algorithm just copies around bytes
 * of data, that's fine for derivatives and the bit-trick finder.
 */
void compress_inflate(){
  constexpr int N = 16;
  double x[N];
  for(int i=0; i<N; i++) x[i] = 42.0;
  bittrick_input(x[0]);
  unsigned char buf[1024];
  unsigned long buflen=1024;
  compress(buf,&buflen,(unsigned char*)&x,N*sizeof(double));
  double y[N];
  unsigned long ylen = N*sizeof(double);
  uncompress((unsigned char*)&y,&ylen,buf,buflen);
  bittrick_output(y[0],42.0,1.0);
}
#endif

#ifndef TRICKTEST_NO_EXTERNAL_DEPENDENCIES
/*! Bit-trick: Multiply-mapped memory
 *
 * We perform two mmap calls for the same anonymous file,
 * to obtain two different virtual memory addresses mapping
 * to the same physical memory address. 
 *
 * This is a problem for Derivgrind as it provides shadow 
 * memory for virtual addresses, and so one piece of data is
 * now shadowed twice.
 */
void multiple_mmap(){
  int fd = memfd_create("",0);
  ftruncate(fd,0x1000);
  double* x = (double*)mmap(NULL,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
  double* y = (double*)mmap(NULL,0x1000,PROT_READ,MAP_PRIVATE,fd,0);
  *x = 2.718;
  bittrick_input(*x);
  bittrick_output(*y,2.718,1.0);
}
#endif


int main(int argc, char* argv[]){
  std::map<std::string,void(*)(void)> tests = {
    {"integer_addition_to_exponent_double", &integer_addition_to_exponent_double},
    {"integer_addition_to_exponent_float", &integer_addition_to_exponent_float},
    {"incomplete_masking_to_perform_frexp_double", &incomplete_masking_to_perform_frexp_double},
    {"incomplete_masking_to_perform_frexp_float", &incomplete_masking_to_perform_frexp_float},
    {"exploiting_imprecisions_for_rounding_double", &exploiting_imprecision_for_rounding_double},
    {"exploiting_imprecisions_for_rounding_float", &exploiting_imprecision_for_rounding_float},
    #ifndef TRICKTEST_NO_EXTERNAL_DEPENDENCIES
      {"encrypt_decrypt", &encrypt_decrypt},
      {"compress_inflate", &compress_inflate},
      {"multiple_mmap", &multiple_mmap},
    #endif
  };
  if(argc!=2){
    std::cerr << "Usage:\n" 
              << "  " << argv[0] << " --list             List names of bit-tricks.\n"
              << "  " << argv[0] << " name_of_bittrick   Run code with bit-trick."
              << std::endl;
    std::exit(1);
  }
  std::string s(argv[1]);
  if(s=="--list"){
    for(auto const& name : tests){
      std::cout << name.first << " ";
    }
    std::cout << std::endl;
  } else if(tests.count(s)) {
    (tests[s])();
  } else {
    std::cerr << "Unknown argument '"<<s<<"'." << std::endl;
  }
}



