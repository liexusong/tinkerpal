/* Copyright (c) 2013, Eyal Birger
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __TNUM_H__
#define __TNUM_H__

#include "util/tstr.h"

typedef struct {
#define NUMERIC_FLAG_FP 0x01
    int flags;
    union {
        int i;
        double fp;
    } value;
} tnum_t;

#define NUMERIC_IS_FP(x) ((x).flags & NUMERIC_FLAG_FP)
#define NUMERIC_INT(x) ((x).value.i)
#define NUMERIC_FP(x) ((x).value.fp)

/** @brief Initialize tnum from tstr_t
 *
 * @param ret destination tnum_t instance
 * @param s input tstr_t
 * @return 0 on success, non-zero otherwise
 */
int tstr_to_tnum(tnum_t *ret, const tstr_t *s);

/** @brief Initialize tstr_t from integer
 *
 * @param i input integer
 * @param base radix to use for conversion
 * @return tstr_t instance
 */
tstr_t _int_to_tstr(int i, int base);

/** @brief Initialize tstr_t from integer, base 10
 *
 * @param i input integer
 * @return tstr_t instance
 */
static inline tstr_t int_to_tstr(int i)
{
    return _int_to_tstr(i, 10);
}

/** @brief Initialize tstr_t from floating point number
 *
 * @param d input number
 * @return tstr_t instance
 */
tstr_t double_to_tstr(double d);

/** @brief Initialize tstr_t from tnum_t instance
 *
 * @param n input tnum_t
 * @return tstr_t instance
 */
static inline tstr_t tnum_to_tstr(const tnum_t *n)
{
    return NUMERIC_IS_FP(*n) ? double_to_tstr(NUMERIC_FP(*n)) :
        int_to_tstr(NUMERIC_INT(*n));
}

#endif
