//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <crlib/cr-array.h>

CR_NAMESPACE_BEGIN

// see https://github.com/encode84/ulz/
class ULZ final : DenyCopying {
public:
   enum : int32 {
      Excess = 16,
      UncompressFailure = -1
   };

private:
   enum : int32 {
      WindowBits = 17,
      WindowSize = cr::bit (WindowBits),
      WindowMask = WindowSize - 1,

      MinMatch = 4,
      MaxChain = cr::bit (5),

      HashBits = 19,
      HashLength = cr::bit (HashBits),
      EmptyHash = -1,
   };


private:
   Array <int32, ReservePolicy::PlusOne> m_hashTable;
   Array <int32, ReservePolicy::PlusOne> m_prevTable;

public:
   ULZ () {
      m_hashTable.resize (HashLength);
      m_prevTable.resize (WindowSize);
   }
   ~ULZ () = default;

public:
   int32 compress (uint8 *in, int32 inputLength, uint8 *out) {
      for (auto &htb : m_hashTable) {
         htb = EmptyHash;
      }
      uint8 *op = out;

      int32 anchor = 0;
      int32 cur = 0;

      while (cur < inputLength) {
         const int32 maxMatch = inputLength - cur;

         int32 bestLength = 0;
         int32 dist = 0;

         if (maxMatch >= MinMatch) {
            const int32 limit = cr::max <int32> (cur - WindowSize, EmptyHash);

            int32 chainLength = MaxChain;
            int32 lookup = m_hashTable[hash32 (&in[cur])];

            while (lookup > limit) {
               if (in[lookup + bestLength] == in[cur + bestLength] && load32 (&in[lookup]) == load32 (&in[cur])) {
                  int32 length = MinMatch;

                  while (length < maxMatch && in[lookup + length] == in[cur + length]) {
                     length++;
                  }

                  if (length > bestLength) {
                     bestLength = length;
                     dist = cur - lookup;

                     if (length == maxMatch) {
                        break;
                     }
                  }
               }

               if (--chainLength == 0) {
                  break;
               }
               lookup = m_prevTable[lookup & WindowMask];
            }
         }

         if (bestLength == MinMatch && (cur - anchor) >= (7 + 128)) {
            bestLength = 0;
         }

         if (bestLength >= MinMatch && bestLength < maxMatch && (cur - anchor) != 6) {
            const int32 next = cur + 1;
            const int32 target = bestLength + 1;
            const int32 limit = cr::max <int32> (next - WindowSize, EmptyHash);

            int32 chainLength = MaxChain;
            int32 lookup = m_hashTable[hash32 (&in[next])];

            while (lookup > limit) {
               if (in[lookup + bestLength] == in[next + bestLength] && load32 (&in[lookup]) == load32 (&in[next])) {
                  int32 length = MinMatch;

                  while (length < target && in[lookup + length] == in[next + length]) {
                     length++;
                  }

                  if (length == target) {
                     bestLength = 0;
                     break;
                  }
               }

               if (--chainLength == 0) {
                  break;
               }
               lookup = m_prevTable[lookup & WindowMask];
            }
         }

         if (bestLength >= MinMatch) {
            const int32 length = bestLength - MinMatch;
            const int32 token = ((dist >> 12) & 16) + cr::min <int32> (length, 15);

            if (anchor != cur) {
               const int32 run = cur - anchor;

               if (run >= 7) {
                  add (op, (7 << 5) + token);
                  encode (op, run - 7);
               }
               else {
                  add (op, (run << 5) + token);
               }
               copy (op, &in[anchor], run);
               op += run;
            }
            else {
               add (op, token);
            }

            if (length >= 15) {
               encode (op, length - 15);
            }
            store16 (op, dist);
            op += 2;

            while (bestLength-- != 0) {
               const uint32 hash = hash32 (&in[cur]);

               m_prevTable[cur & WindowMask] = m_hashTable[hash];
               m_hashTable[hash] = cur++;
            }
            anchor = cur;
         }
         else {
            const uint32 hash = hash32 (&in[cur]);

            m_prevTable[cur & WindowMask] = m_hashTable[hash];
            m_hashTable[hash] = cur++;
         }
      }

      if (anchor != cur) {
         const int32 run = cur - anchor;

         if (run >= 7) {
            add (op, 7 << 5);
            encode (op, run - 7);
         }
         else {
            add (op, run << 5);
         }
         copy (op, &in[anchor], run);
         op += run;
      }
      return op - out;
   }

   int32 uncompress (uint8 *in, int32 inputLength, uint8 *out, int32 outLength) {
      uint8 *op = out;
      uint8 *ip = in;

      const uint8 *opEnd = op + outLength;
      const uint8 *ipEnd = ip + inputLength;

      while (ip < ipEnd) {
         const int32 token = *ip++;

         if (token >= 32) {
            int32 run = token >> 5;

            if (run == 7) {
               run += decode (ip);
            }

            if ((opEnd - op) < run || (ipEnd - ip) < run) {
               return UncompressFailure;
            }
            copy (op, ip, run);

            op += run;
            ip += run;

            if (ip >= ipEnd) {
               break;
            }
         }
         int32 length = (token & 15) + MinMatch;

         if (length == (15 + MinMatch)) {
            length += decode (ip);
         }

         if ((opEnd - op) < length) {
            return UncompressFailure;
         }
         const int32 dist = ((token & 16) << 12) + load16 (ip);
         ip += 2;

         uint8 *cp = op - dist;

         if ((op - out) < dist) {
            return UncompressFailure;
         }

         if (dist >= 8) {
            copy (op, cp, length);
            op += length;
         }
         else
         {
            for (int32 i = 0; i < 4; ++i) {
               *op++ = *cp++;
            }

            while (length-- != 4) {
               *op++ = *cp++;
            }
         }
      }
      return static_cast <int32> (ip == ipEnd) ? static_cast <int32> (op - out) : UncompressFailure;
   }

private:
   inline uint16 load16 (void *ptr) {
      return *reinterpret_cast <const uint16 *> (ptr);
   }

   inline uint32 load32 (void *ptr) {
      return *reinterpret_cast <const uint32 *> (ptr);
   }

   inline void store16 (void *ptr, int32 val) {
      *reinterpret_cast <uint16 *> (ptr) = static_cast <uint16> (val);
   }

   inline void copy64 (void *dst, void *src) {
      *reinterpret_cast <uint64 *> (dst) = *reinterpret_cast <const uint64 *> (src);
   }

   inline uint32 hash32 (void *ptr) {
      return (load32 (ptr) * 0x9E3779B9) >> (32 - HashBits);
   }

   inline void copy (uint8 *dst, uint8 *src, int32 count) {
      copy64 (dst, src);

      for (int32 i = 8; i < count; i += 8) {
         copy64 (dst + i, src + i);
      }
   }

   inline void add (uint8 *&dst, int32 val) {
      *dst++ = static_cast <uint8> (val);
   }

   inline void encode (uint8 *&ptr, uint32 val) {
      while (val >= 128) {
         val -= 128;

         *ptr++ = 128 + (val & 127);
         val >>= 7;
      }
      *ptr++ = static_cast <uint8> (val);
   }

   inline uint32 decode (uint8 *&ptr) {
      uint32 val = 0;

      for (int32 i = 0; i <= 21; i += 7) {
         const uint32 cur = *ptr++;
         val += cur << i;

         if (cur < 128) {
            break;
         }
      }
      return val;
   }
};



CR_NAMESPACE_END