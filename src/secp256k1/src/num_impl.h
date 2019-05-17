/**********************************************************************
 * Copyright (c) 2013, 2014 Pieter Wuille                             *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http:
 **********************************************************************/

#ifndef SECP256K1_NUM_IMPL_H
#define SECP256K1_NUM_IMPL_H

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#include "num.h"

#if defined(USE_NUM_GMP)
#include "num_gmp_impl.h"
#elif defined(USE_NUM_NONE)

#else
#error "Please select num implementation"
#endif

#endif 
