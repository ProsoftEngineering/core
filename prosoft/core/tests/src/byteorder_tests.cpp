// Copyright © 2012-2015, Prosoft Engineering, Inc. (A.K.A "Prosoft")
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Prosoft nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PROSOFT ENGINEERING, INC. BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string.h>

#include <byteorder.h>

#include "catch.hpp"

using namespace prosoft;

namespace {

union BT {
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64 = 0;
    unsigned char bytes[8];
};

union u16_swap {
    uint8_t bytes[2];
    uint16_t val;
};
union u32_swap {
    uint8_t bytes[4];
    uint32_t val;
};
union u64_swap {
    uint8_t bytes[8];
    uint64_t val;
};
}

TEST_CASE("byteorder") {
    BT b;
    b.bytes[0] = 0x00;
    b.bytes[1] = 0x11;
    b.bytes[2] = 0x22;
    b.bytes[3] = 0x33;
    b.bytes[4] = 0x44;
    b.bytes[5] = 0x55;
    b.bytes[6] = 0x66;
    b.bytes[7] = 0x77;

    SECTION("swap") {
        b.u64 = 0;

        b.i16 = 1;
        CHECK(byteswap16(b.u16) == 0x0100U);

        b.i32 = -2;
        CHECK(byteswap32(b.u32) == 0xfeffffffU);

        b.i64 = -27763923895328LL;
        CHECK(byteswap64(b.u64) == 0xe0bb2db5bfe6ffffULL);
    }

    SECTION("le16toh") {
        CHECK(le16_to_host(b.u16) == 0x1100U);
    }
    SECTION("le32toh") {
        CHECK(le32_to_host(b.u32) == 0x33221100U);
    }
    SECTION("le64toh") {
        CHECK(le64_to_host(b.u64) == 0x7766554433221100ULL);
    }

    SECTION("be16toh") {
        CHECK(be16_to_host(b.u16) == 0x0011U);
    }
    SECTION("be16toh") {
        CHECK(be32_to_host(b.u32) == 0x00112233U);
    }
    SECTION("be16toh") {
        CHECK(be64_to_host(b.u64) == 0x0011223344556677ULL);
    }

    SECTION("htole16") {
        CHECK(host_to_le16(b.u16) == 0x1100U);
    }
    SECTION("htole32") {
        CHECK(host_to_le32(b.u32) == 0x33221100U);
    }
    SECTION("htole64") {
        CHECK(host_to_le64(b.u64) == 0x7766554433221100ULL);
    }

    SECTION("htobe16") {
        CHECK(host_to_be16(b.u16) == 0x0011U);
    }
    SECTION("htobe32") {
        CHECK(host_to_be32(b.u32) == 0x00112233U);
    }
    SECTION("htobe64") {
        CHECK(host_to_be64(b.u64) == 0x0011223344556677ULL);
    }

    SECTION("letoh macros") {
        union u16_swap u16;
        union u32_swap u32;
        union u64_swap u64;
        uint16_t u16_val;
        uint32_t u32_val;
        uint64_t u64_val;
        u16.bytes[0] = 0xAA;
        u16.bytes[1] = 0xBB;
        u16_val = ps_le16_to_host(u16.val);
        CHECK(u16_val == 0xBBAA);
        u32.bytes[0] = 0xAA;
        u32.bytes[1] = 0xBB;
        u32.bytes[2] = 0xCC;
        u32.bytes[3] = 0xDD;
        u32_val = ps_le32_to_host(u32.val);
        CHECK(u32_val == 0xDDCCBBAA);
        u64.bytes[0] = 0xAA;
        u64.bytes[1] = 0xBB;
        u64.bytes[2] = 0xCC;
        u64.bytes[3] = 0xDD;
        u64.bytes[4] = 0xEE;
        u64.bytes[5] = 0xFF;
        u64.bytes[6] = 0x00;
        u64.bytes[7] = 0x11;
        u64_val = ps_le64_to_host(u64.val);
        CHECK(u64_val == 0x1100FFEEDDCCBBAAULL);
    }

    SECTION("betoh macros") {
        union u16_swap u16;
        union u32_swap u32;
        union u64_swap u64;
        uint16_t u16_val;
        uint32_t u32_val;
        uint64_t u64_val;
        u16.bytes[0] = 0xAA;
        u16.bytes[1] = 0xBB;
        u16_val = ps_be16_to_host(u16.val);
        CHECK(u16_val == 0xAABB);
        u32.bytes[0] = 0xAA;
        u32.bytes[1] = 0xBB;
        u32.bytes[2] = 0xCC;
        u32.bytes[3] = 0xDD;
        u32_val = ps_be32_to_host(u32.val);
        CHECK(u32_val == 0xAABBCCDD);
        u64.bytes[0] = 0xAA;
        u64.bytes[1] = 0xBB;
        u64.bytes[2] = 0xCC;
        u64.bytes[3] = 0xDD;
        u64.bytes[4] = 0xEE;
        u64.bytes[5] = 0xFF;
        u64.bytes[6] = 0x00;
        u64.bytes[7] = 0x11;
        u64_val = ps_be64_to_host(u64.val);
        CHECK(u64_val == 0xAABBCCDDEEFF0011ULL);
    }

    SECTION("htole macros") {
        uint16_t val16;
        uint8_t val16Bytes[2] = {0xBB, 0xAA};
        uint32_t val32;
        uint8_t val32Bytes[4] = {0xDD, 0xCC, 0xBB, 0xAA};
        uint64_t val64;
        uint8_t val64Bytes[8] = {0x11, 0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};
        val16 = ps_host_to_le16(0xAABB);
        CHECK(memcmp(&val16, val16Bytes, sizeof(val16)) == 0);
        val32 = ps_host_to_le32(0xAABBCCDD);
        CHECK(memcmp(&val32, val32Bytes, sizeof(val32)) == 0);
        val64 = ps_host_to_le64(0xAABBCCDDEEFF0011ULL);
        CHECK(memcmp(&val64, val64Bytes, sizeof(val64)) == 0);
    }

    SECTION("htobe macros") {
        uint16_t val16;
        uint8_t val16Bytes[2] = {0xAA, 0xBB};
        uint32_t val32;
        uint8_t val32Bytes[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint64_t val64;
        uint8_t val64Bytes[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
        val16 = ps_host_to_be16(0xAABB);
        CHECK(memcmp(&val16, val16Bytes, sizeof(val16)) == 0);
        val32 = ps_host_to_be32(0xAABBCCDD);
        CHECK(memcmp(&val32, val32Bytes, sizeof(val32)) == 0);
        val64 = ps_host_to_be64(0xAABBCCDDEEFF0011ULL);
        CHECK(memcmp(&val64, val64Bytes, sizeof(val64)) == 0);
    }
}
