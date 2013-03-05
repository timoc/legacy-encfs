/*****************************************************************************
 * Author:   Valient Gough <vgough@pobox.com>
 *
 *****************************************************************************
 * Copyright (c) 2002-2003, Valient Gough
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.  
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _Cipher_incl_
#define _Cipher_incl_

#include "cipher/CipherKey.h"
#include "base/Interface.h"
#include "base/Range.h"
#include "base/types.h"

#include <string>
#include <list>
#include <inttypes.h>

namespace encfs {

/*
    Mostly pure virtual interface defining operations on a cipher.

    Cipher's should register themselves so they can be instanciated via
    Cipher::New().
*/
class Cipher
{
public:
  // if no key length was indicated when cipher was registered, then keyLen
  // <= 0 will be used.
  typedef shared_ptr<Cipher> (*CipherConstructor)(
      const Interface &iface, int keyLenBits );

  struct CipherAlgorithm
  {
    std::string name;
    std::string description;
    Interface iface;
    Range keyLength;
    Range blockSize;
    bool hasStreamMode;
  };


  typedef std::list<CipherAlgorithm> AlgorithmList;
  static AlgorithmList GetAlgorithmList( bool includeHidden = false );


  static shared_ptr<Cipher> New( const Interface &iface, 
                                 int keyLen = -1);
  static shared_ptr<Cipher> New( const std::string &cipherName, 
                                 int keyLen = -1 );


  static bool Register(const char *cipherName, 
	                     const char *description, 
                       const Interface &iface,
                       CipherConstructor constructor,
                       bool hasStreamMode,
                       bool hidden = false);

  static bool Register(const char *cipherName, 
                       const char *description, 
                       const Interface &iface,
                       const Range &keyLength, const Range &blockSize,
                       CipherConstructor constructor,
                       bool hasStreamMode,
                       bool hidden = false);

  Cipher();
  virtual ~Cipher();

  virtual Interface interface() const =0;

  // create a new key based on a password
  // if iterationCount == 0, then iteration count will be determined
  // by newKey function and filled in.
  // If iterationCount == 0, then desiredFunctionDuration is how many
  // milliseconds the password derivation function should take to run.
  virtual CipherKey newKey(const char *password, int passwdLength,
                           int &iterationCount, long desiredFunctionDuration,
                           const byte *salt, int saltLen) =0;

  // deprecated - for backward compatibility
  virtual CipherKey newKey(const char *password, int passwdLength ) =0;

  // create a new random key
  virtual CipherKey newRandomKey() =0;

  // data must be len encodedKeySize()
  virtual CipherKey readKey(const byte *data, 
                            const CipherKey &encodingKey,
                            bool checkKey = true) =0;

  virtual void writeKey(const CipherKey &key, byte *data, 
                        const CipherKey &encodingKey) =0; 

  virtual std::string encodeAsString(const CipherKey &key,
                                     const CipherKey &encodingKey );

  // for testing purposes
  virtual bool compareKey( const CipherKey &A, const CipherKey &B ) const =0;

  // meta-data about the cypher
  virtual int keySize() const=0;
  virtual int encodedKeySize() const=0; // size 
  virtual int cipherBlockSize() const=0; // size of a cipher block

  virtual bool hasStreamMode() const;

  // fill the supplied buffer with random data
  // The data may be pseudo random and might not be suitable for key
  // generation.  For generating keys, uses newRandomKey() instead.
  // Returns true on success, false on failure.
  virtual bool randomize( byte *buf, int len,
                          bool strongRandom ) const =0;

  // 64 bit MAC of the data with the given key
  virtual uint64_t MAC_64( const byte *src, int len,
                           const CipherKey &key, uint64_t *chainedIV = 0 ) const =0;

  // based on reductions of MAC_64
  unsigned int MAC_32( const byte *src, int len,
                       const CipherKey &key, uint64_t *chainedIV = 0 ) const;
  unsigned int MAC_16( const byte *src, int len,
                       const CipherKey &key, uint64_t *chainedIV = 0 ) const;

  // functional interfaces
  /*
      Stream encoding of data in-place.  The stream data can be any length.
   */
  virtual bool streamEncode( byte *data, int len, 
                             uint64_t iv64, const CipherKey &key) const=0;
  virtual bool streamDecode( byte *data, int len, 
                             uint64_t iv64, const CipherKey &key) const=0;

  /*
      Block encoding of data in-place.  The data size should be a multiple of
      the cipher block size.
   */
  virtual bool blockEncode(byte *buf, int size, 
	                         uint64_t iv64, const CipherKey &key) const=0;
  virtual bool blockDecode(byte *buf, int size, 
	                         uint64_t iv64, const CipherKey &key) const=0;
};

}  // namespace encfs

#endif

