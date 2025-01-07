#define _CRT_SECURE_NO_WARNINGS
#include "RSA.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <D:\msys\home\YYDS\gmp-6.2.0\gmp.h>
#include <time.h>

void randbits(mpz_t result, int bits)
{
    gmp_randstate_t state;
    gmp_randinit_default(state);
    gmp_randseed_ui(state, rand());
    mpz_rrandomb(result, state, bits);
    gmp_randclear(state);
}

void randprime(mpz_t result, int bits)
{
    mpz_t temp;
    mpz_init(temp);
    randbits(temp, bits);
    mpz_nextprime(result, temp);
    mpz_clear(temp);
}

void setKey(mpz_t n, mpz_t e, mpz_t d, int nbits, int ebits)
{
    if (nbits / 2 <= ebits)
    {
        ebits = nbits / 2;
    }

    mpz_t p, q, fn, gcd, temp;
    mpz_inits(p, q, fn, gcd, temp, NULL);

    randprime(p, nbits / 2);
    randprime(q, nbits / 2);

    mpz_mul(n, p, q);

    mpz_sub_ui(temp, p, 1);
    mpz_sub_ui(p, q, 1);
    mpz_mul(fn, temp, p);

    do
    {
        randprime(e, ebits);
        mpz_gcd(gcd, e, fn);
    } while (mpz_cmp_ui(gcd, 1) != 0);

    mpz_invert(d, e, fn);

    mpz_clears(p, q, fn, gcd, temp, NULL);
}

void encrypt(mpz_t result, const mpz_t m, const mpz_t e, const mpz_t n)
{
    mpz_powm(result, m, e, n);
}

void decrypt(mpz_t result, const mpz_t c, const mpz_t d, const mpz_t n)
{
    mpz_powm(result, c, d, n);
}

void string_to_mpz(mpz_t result, const char *str)
{
    size_t len = strlen(str);
    char *hex = (char *)malloc(len * 2 + 1);
    for (size_t i = 0; i < len; i++)
    {
        sprintf(&hex[i * 2], "%02x", (unsigned char)str[i]);
    }
    mpz_set_str(result, hex, 16);
    free(hex);
}

void mpz_to_string(char *result, const mpz_t num)
{
    char *hex = mpz_get_str(NULL, 16, num);
    size_t hex_len = strlen(hex);
    size_t len = hex_len / 2;
    for (size_t i = 0; i < len; i++)
    {
        sscanf(&hex[i * 2], "%2hhx", &result[i]);
    }
    result[len] = '\0';
    free(hex);
}
