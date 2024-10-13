#ifndef EMP_GROUP_H
#define EMP_GROUP_H

#include <string>
#include <cstring>

#include "emp-tool/utils/utils.h"

#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

namespace emp {
class BigInt {
public:
    mbedtls_mpi n;
    BigInt();
    BigInt(const BigInt &oth);
    BigInt &operator=(BigInt oth);
    ~BigInt();

    int size();
    void to_bin(unsigned char * in);
    void from_bin(const unsigned char * in, int length);

    BigInt add(const BigInt &oth);
    BigInt mul(const BigInt &oth);
    BigInt mod(const BigInt &oth);
    BigInt add_mod(const BigInt & b, const BigInt& m);
    BigInt mul_mod(const BigInt & b, const BigInt& m);
};
class Group;
class Point {
public:
    mbedtls_ecp_point point;
    Group * group = nullptr;
    Point (Group * g = nullptr);
    ~Point();
    Point(const Point & p);
    Point& operator=(Point p);

    void to_bin(unsigned char * buf, size_t buf_len);
    size_t size();
    void from_bin(Group * g, const unsigned char * buf, size_t buf_len);

    Point add(Point & rhs);
    Point mul(const BigInt &m);
    Point inv();
    bool operator==(Point & rhs);
};

class Group {
public:
    mbedtls_ecp_group ec_group;
    BigInt order;
    unsigned char * scratch;
    size_t scratch_size = 256;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    Group();
    ~Group();
    void resize_scratch(size_t size);
    void get_rand_bn(BigInt & n);
    Point get_generator();
    Point mul_gen(const BigInt &m);
};

}

#include "group_mbedtls.h"

#endif