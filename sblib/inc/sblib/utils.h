/*
 *  utils.h - Utility functions.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef sblib_utils_h
#define sblib_utils_h

#include <cstddef>
#include <sblib/types.h>
#include <sblib/libconfig.h>

#define HIGH_BYTE(x) ((uint8_t)(x >> 8))  ///\todo create new macro "secondByte" in bits.h

/**
 * Copy from src to dest with reversing the byte order.
 *
 * @param dest - the destination to copy to
 * @param src - the source to copy from
 * @param len - the number of bytes to copy
 */
void reverseCopy(byte* dest, const byte* src, int len);

/**
 * Call when a fatal application error happens. This function will never
 * return and the program/fatalError LED (can be changed with setFatalErrorPin)
 * will blink rapidly to indicate the error.
 */
void fatalError();

/**
 * Set the pin for fatalError().
 *
 * @param newPin - the new pin
 */
void setFatalErrorPin(int newPin);

/**
 * Set the KNX Tx pin for fatalError() and a hardware fault.
 *
 * @param newTxPin - the new KNX-Tx pin
 */
void setKNX_TX_Pin(int newTxPin);

/**
 * Optional internal helpers, kept public for compatibility.
 */
int getFatalErrorPin();
int getKNX_TX_Pin();

/**
 * Creates a len_hash wide hash of the uid.
 * Hash will be generated in provided hash buffer
 *
 * @param uid - LPC-serial (128bit GUID) returned by iapReadUID() which will be hashed
 * @param len_uid - size of uid  (normally 16 byte)
 * @param hash - buffer for generated hash
 * @param len_hash - size of provided hash buffer (normally 6byte/48bit for EIB)
 * @return True if hash successfully created, false if not.
 */
int hashUID(byte* uid, const int len_uid, byte* hash, const int len_hash);

/**
 * Get the offset of a field in a class, structure or type.
 *
 * @param type - the class, structure or type.
 * @param member - the member
 * @return The offset of the member.
 */
#define OFFSET_OF(type, field)  (offsetof(type, field))

#ifdef DEBUG
#  define IF_DEBUG(code) { code; }
#else
#  define IF_DEBUG(code)
#endif

#if defined(DUMP_MEM_OPS)
#   define DB_MEM_OPS(x) IF_DEBUG(x)
#else
#   define DB_MEM_OPS(x)
#endif

#if defined(DUMP_PROPERTIES)
#   define DB_PROPERTIES(code) IF_DEBUG(code)
#else
#   define DB_PROPERTIES(code)
#endif

#if defined(DUMP_COM_OBJ)
#   define DB_COM_OBJ(x) x
#else
#   define DB_COM_OBJ(x)
#endif

#if defined(DUMP_TELEGRAMS)
#   define DB_TELEGRAM(x) x
#else
#   define DB_TELEGRAM(x)
#endif

#if defined (DEBUG_BUS) || defined (DEBUG_BUS_BITLEVEL) || defined(DUMP_TELEGRAMS)
#   define DB_BUS(x) x
#else
#   define DB_BUS(x)
#endif

#if !defined(INCLUDE_SERIAL)
#   undef DB_MEM_OPS
#   define DB_MEM_OPS(x)
#   undef DB_PROPERTIES
#   define DB_PROPERTIES(x)
#   undef DB_COM_OBJ
#   define DB_COM_OBJ(x)
#   undef DB_TELEGRAM
#   define DB_TELEGRAM(x)
#   undef DB_BUS
#   define DB_BUS(x)
#endif

#define CPP_CONCAT(str1,str2)  str1 ## str2
#define CPP_CONCAT_EXPAND(str1,str2) CPP_CONCAT(str1,str2)
#define CPP_QUOTE(str) #str
#define CPP_QUOTE_EXPAND(str) CPP_QUOTE(str)

extern "C" void HardFault_Handler();

#endif /*sblib_utils_h*/