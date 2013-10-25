/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2006 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2011-2013 UT-Battelle, LLC. All rights reserved.
 * Copyright (C) 2013      Mellanox Technologies Ltd. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "ocoms/platform/ocoms_config.h"

#include "ocoms/platform/ocoms_constants.h"
#include "ocoms/util/arch.h"

uint32_t ocoms_local_arch = 0xFFFFFFFF;
int ocoms_cache_line_size = 128;

static inline int32_t ocoms_arch_isbigendian ( void )
{
    const uint32_t value = 0x12345678;
    const char *ptr = (char*)&value;
    int x = 0;

    /* if( sizeof(int) == 8 ) x = 4; */
    if( ptr[x] == 0x12)  return 1; /* big endian, true */
    if( ptr[x] == 0x78 ) return 0; /* little endian, false */
    assert( 0 );  /* unknown architecture not little nor big endian */
    return -1;
}

/* we must find which representation of long double is used
 * intel or sparc. Both of them represent the long doubles using a close to
 * IEEE representation (seeeeeee..emmm...m) where the mantissa look like
 * 1.????. For the intel representaion the 1 is explicit, and for the sparc
 * the first one is implicit. If we take the number 2.0 the exponent is 1
 * and the mantissa is 1.0 (the sign of course should be 0). So if we check
 * for the first one in the binary representation of the number, we will
 * find the bit from the exponent, so the next one should be the begining
 * of the mantissa. If it's 1 then we have an intel representaion, if not
 * we have a sparc one. QED
 */
static inline int32_t ocoms_arch_ldisintel( void )
{
    long double ld = 2.0;
    int i, j;
    uint32_t* pui = (uint32_t*)(void*)&ld;

    j = LDBL_MANT_DIG / 32;
    i = (LDBL_MANT_DIG % 32) - 1;
    if( ocoms_arch_isbigendian() ) { /* big endian */
        j = (sizeof(long double) / sizeof(unsigned int)) - j;
        if( i < 0 ) {
            i = 31;
            j = j+1;
        }
    } else {
        if( i < 0 ) {
            i = 31;
            j = j-1;
        }
    }
    return (pui[j] & (1 << i) ? 1 : 0);
}

static inline void ocoms_arch_setmask ( uint32_t *var, uint32_t mask)
{
    *var |= mask;
}

int ocoms_arch_init(void)
{
    ocoms_local_arch = (OCOMS_ARCH_HEADERMASK | OCOMS_ARCH_UNUSEDMASK);

    /* Handle the size of long (can hold a pointer) */
    if( 8 == sizeof(long) )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LONGIS64 );

    /* sizeof bool */
    if (1 == sizeof(bool) ) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_BOOLIS8);
    } else if (2 == sizeof(bool)) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_BOOLIS16);
    } else if (4 == sizeof(bool)) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_BOOLIS32);
    }

    /* Note that fortran logical size is set later, to make
       abstractions a little less painful... */

    /* Initialize the information regarding the long double */
    if( 12 == sizeof(long double) )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LONGDOUBLEIS96 );
    else if( 16 == sizeof(long double) )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LONGDOUBLEIS128 );

    /* Big endian or little endian ? That's the question */
    if( ocoms_arch_isbigendian() )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_ISBIGENDIAN );

    /* What's the maximum exponent ? */
    if ( LDBL_MAX_EXP == 16384 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDEXPSIZEIS15 );

    /* How about the length in bits of the mantissa */
    if ( LDBL_MANT_DIG == 64 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDMANTDIGIS64 );
    else if ( LDBL_MANT_DIG == 105 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDMANTDIGIS105 );
    else if ( LDBL_MANT_DIG == 106 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDMANTDIGIS106 );
    else if ( LDBL_MANT_DIG == 107 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDMANTDIGIS107 );
    else if ( LDBL_MANT_DIG == 113 )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDMANTDIGIS113 );

    /* Intel data representation or Sparc ? */
    if( ocoms_arch_ldisintel() )
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LDISINTEL );

    return OCOMS_SUCCESS;
}

int32_t ocoms_arch_checkmask ( uint32_t *var, uint32_t mask )
{
    unsigned int tmpvar = *var;

    /* Check whether the headers are set correctly,
       or whether this is an erroneous integer */
    if( !((*var) & OCOMS_ARCH_HEADERMASK) ) {
        if( (*var) & OCOMS_ARCH_HEADERMASK2 ) {
            char* pcDest, *pcSrc;
            /* Both ends of this integer have the wrong settings,
               maybe its just the wrong endian-representation. Try
               to swap it and check again. If it looks now correct,
               keep this version of the variable
            */

            pcDest = (char *) &tmpvar;
            pcSrc  = (char *) var + 3;
            *pcDest++ = *pcSrc--;
            *pcDest++ = *pcSrc--;
            *pcDest++ = *pcSrc--;
            *pcDest++ = *pcSrc--;

            if( (tmpvar & OCOMS_ARCH_HEADERMASK) && (!(tmpvar & OCOMS_ARCH_HEADERMASK2)) ) {
                *var = tmpvar;
            } else
                return -1;
        } else
            return -1;
    }

    /* Here is the real evaluation of the bitmask */
    return ( ((*var) & mask) == mask );
}

int
ocoms_arch_set_fortran_logical_size(uint32_t size)
{
    if (1 == size) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LOGICALIS8);
    } else if (2 == size) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LOGICALIS16);
    } else if (4 == size) {
        ocoms_arch_setmask( &ocoms_local_arch, OCOMS_ARCH_LOGICALIS32);
    }

    return OCOMS_SUCCESS;
}
