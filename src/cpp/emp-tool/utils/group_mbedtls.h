#ifndef EMP_GROUP_MBEDTLS_H
#define EMP_GROUP_MBEDTLS_H

namespace emp {

// BigInt implementation
inline BigInt::BigInt() {
    mbedtls_mpi_init(&n);
}
inline BigInt::BigInt(const BigInt &oth) {
    mbedtls_mpi_init(&n);
    mbedtls_mpi_copy(&n, &oth.n);
}
inline BigInt& BigInt::operator=(BigInt oth) {
    std::swap(n, oth.n);
    return *this;
}
inline BigInt::~BigInt() {
    mbedtls_mpi_free(&n);
}

inline int BigInt::size() {
    return mbedtls_mpi_size(&n);
}

inline void BigInt::to_bin(unsigned char * in) {
    mbedtls_mpi_write_binary(&n, in, mbedtls_mpi_size(&n));
}

inline void BigInt::from_bin(const unsigned char * in, int length) {
    mbedtls_mpi_read_binary(&n, in, length);
}

inline BigInt BigInt::add(const BigInt &oth) {
    BigInt ret;
    mbedtls_mpi_add_mpi(&ret.n, &n, &oth.n);
    return ret;
}

inline BigInt BigInt::mul_mod(const BigInt & b, const BigInt &m) {
    BigInt ret;
    mbedtls_mpi_mul_mpi(&ret.n, &n, &b.n);
    mbedtls_mpi_mod_mpi(&ret.n, &ret.n, &m.n);
    return ret;
}

inline BigInt BigInt::add_mod(const BigInt & b, const BigInt &m) {
    BigInt ret;
    mbedtls_mpi_add_mpi(&ret.n, &n, &b.n);
    mbedtls_mpi_mod_mpi(&ret.n, &ret.n, &m.n);
    return ret;
}

inline BigInt BigInt::mul(const BigInt &oth) {
    BigInt ret;
    mbedtls_mpi_mul_mpi(&ret.n, &n, &oth.n);
    return ret;
}

inline BigInt BigInt::mod(const BigInt &oth) {
    BigInt ret;
    mbedtls_mpi_mod_mpi(&ret.n, &n, &oth.n);
    return ret;
}

// Point implementation
inline Point::Point (Group * g) {
    if (g == nullptr) return;
    this->group = g;
    mbedtls_ecp_point_init(&point);
}

inline Point::~Point() {
    if (group == nullptr) return;
    mbedtls_ecp_point_free(&point);
}

inline Point::Point(const Point & p) {
    if (p.group == nullptr) return;
    this->group = p.group;
    mbedtls_ecp_point_init(&point);
    int ret = mbedtls_ecp_copy(&point, &p.point);
    if(ret != 0) error("ECC COPY");
}

inline Point& Point::operator=(Point p) {
    std::swap(p.point, point);
    std::swap(p.group, group);
    return *this;
}

inline void Point::to_bin(unsigned char * buf, size_t buf_len) {
    size_t olen = 0;
    int ret = mbedtls_ecp_point_write_binary(&group->ec_group, &point,
        MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, buf_len);
    if(ret != 0) error("ECC TO_BIN");
}

inline size_t Point::size() {
    size_t olen = 0;
    mbedtls_ecp_point_write_binary(&group->ec_group, &point,
        MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, NULL, 0);
    return olen;
}

inline void Point::from_bin(Group * g, const unsigned char * buf, size_t buf_len) {
    if (group == nullptr) {
        group = g;
        mbedtls_ecp_point_init(&point);
    }
    int ret = mbedtls_ecp_point_read_binary(&group->ec_group, &point, buf, buf_len);
    if(ret != 0) error("ECC FROM_BIN");
}

inline Point Point::add(Point & rhs) {
    Point ret(group);
    mbedtls_mpi one;
    mbedtls_mpi_init(&one);

    // Set 'one' to 1
    if (mbedtls_mpi_lset(&one, 1) != 0) {
        error("Failed to set MPI value");
    }

    int res = mbedtls_ecp_muladd(&group->ec_group, &ret.point, &one, &point, &one, &rhs.point);
    mbedtls_mpi_free(&one);

    if (res != 0) {
        error("ECC ADD");
    }
    return ret;
}

inline Point Point::mul(const BigInt &m) {
    Point ret (group);
    int res = mbedtls_ecp_mul(&group->ec_group, &ret.point, &m.n, &point, mbedtls_ctr_drbg_random, &group->ctr_drbg);
    if(res != 0) error("ECC MUL");
    return ret;
}

inline Point Point::inv() {
    Point ret(group);  // Create a new Point associated with the same Group
    mbedtls_mpi order_minus_one;
    mbedtls_mpi_init(&order_minus_one);

    // Compute (order - 1) which is equivalent to -1 mod order
    mbedtls_mpi_copy(&order_minus_one, &group->ec_group.N);
    mbedtls_mpi_sub_int(&order_minus_one, &order_minus_one, 1);

    // Check if the point is valid and on the curve before inversion
    int check = mbedtls_ecp_check_pubkey(&group->ec_group, &point);
    if (check != 0) {
        mbedtls_mpi_free(&order_minus_one);
        error("Point is not on the curve");
    }

    // Perform the point inversion using scalar multiplication with (order - 1)
    int res = mbedtls_ecp_mul(&group->ec_group, &ret.point, &order_minus_one, &point, mbedtls_ctr_drbg_random, &group->ctr_drbg);
    if (res != 0) {
        mbedtls_mpi_free(&order_minus_one);
        error("ECC INV");
    }

    mbedtls_mpi_free(&order_minus_one);
    return ret;
}

inline bool Point::operator==(Point & rhs) {
    int ret = mbedtls_ecp_point_cmp(&point, &rhs.point);
    if(ret < 0) error("ECC CMP");
    return (ret == 0);
}

// Group implementation
inline Group::Group() {
    mbedtls_ecp_group_init(&ec_group);
    mbedtls_ecp_group_load(&ec_group, MBEDTLS_ECP_DP_SECP256R1); // NIST P-256
    mbedtls_mpi_init(&order.n);
    mbedtls_mpi_copy(&order.n, &ec_group.N);
    scratch = new unsigned char[scratch_size];
    // Initialize entropy and DRBG
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    const char *pers = "emp_group";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *) pers, strlen(pers));
    if (ret != 0) error("DRBG SEED");
}

inline Group::~Group(){
    mbedtls_ecp_group_free(&ec_group);
    mbedtls_mpi_free(&order.n);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    if(scratch != nullptr)
        delete[] scratch;
}

inline void Group::resize_scratch(size_t size) {
    if (size > scratch_size) {
        delete[] scratch;
        scratch_size = size;
        scratch = new unsigned char[scratch_size];
    }
}

inline void Group::get_rand_bn(BigInt & n) {
    int ret = mbedtls_mpi_fill_random(&n.n, mbedtls_mpi_size(&order.n), mbedtls_ctr_drbg_random, &ctr_drbg);
    if(ret != 0) error("RAND BN");
    mbedtls_mpi_mod_mpi(&n.n, &n.n, &order.n);
}

inline Point Group::get_generator() {
    Point res(this);
    int ret = mbedtls_ecp_copy(&res.point, &ec_group.G);
    if(ret != 0) error("ECC GEN");
    return res;
}

inline Point Group::mul_gen(const BigInt &m) {
    Point res(this);
    int ret = mbedtls_ecp_mul(&ec_group, &res.point, &m.n, &ec_group.G, mbedtls_ctr_drbg_random, &ctr_drbg);
    if(ret != 0) error("ECC GEN MUL");
    return res;
}
}
#endif