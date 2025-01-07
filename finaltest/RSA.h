#ifndef RSA_H
#define RSA_H

#include <D:\msys\home\YYDS\gmp-6.2.0\gmp.h>

// 生成指定比特数的随机数
void randbits(mpz_t result, int bits);

// 生成指定比特数的随机素数
void randprime(mpz_t result, int bits);

// 生成公钥和私钥
void setKey(mpz_t n, mpz_t e, mpz_t d, int nbits, int ebits);

// 加密函数，计算c = m^e mod n
void encrypt(mpz_t result, const mpz_t m, const mpz_t e, const mpz_t n);

// 解密函数，计算m = c^d mod n
void decrypt(mpz_t result, const mpz_t c, const mpz_t d, const mpz_t n);

// 将字符串转换为大整数
void string_to_mpz(mpz_t result, const char *str);

// 将大整数转换为字符串
void mpz_to_string(char *result, const mpz_t num);

#endif // RSA_H
