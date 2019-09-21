//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
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
   SmallArray <int32> m_hashTable;
   SmallArray <int32> m_prevTable;

public:
   explicit ULZ () {
      m_hashTable.resize (HashLength);
      m_prevTable.resize (WindowSize);
   }

   ~ULZ () = default;

public:
   int32 compress (uint8 *in, int32 inputLength, uint8 *out) {
      for (auto &htb : m_hashTable) {
         htb = EmptyHash;
      }
      auto op = out;

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
               if (in[lookup + bestLength] == in[cur + bestLength] && load <uint32> (&in[lookup]) == load <uint32> (&in[cur])) {
                  int32 length = MinMatch;

                  while (length < maxMatch && in[lookup + length] == in[cur + length]) {
                     ++length;
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
            const auto next = cur + 1;
            const auto target = bestLength + 1;
            const auto limit = cr::max <int32> (next - WindowSize, EmptyHash);

            int32 chainLength = MaxChain;
            int32 lookup = m_hashTable[hash32 (&in[next])];

            while (lookup > limit) {
               if (in[lookup + bestLength] == in[next + bestLength] && load <uint32> (&in[lookup]) == load <uint32> (&in[next])) {
                  int32 length = MinMatch;

                  while (length < target && in[lookup + length] == in[next + length]) {
                     ++length;
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
            const auto length = bestLength - MinMatch;
            const auto token = ((dist >> 12) & 16) + cr::min <int32> (length, 15);

            if (anchor != cur) {
               const auto run = cur - anchor;

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
               const auto hash = hash32 (&in[cur]);

               m_prevTable[cur & WindowMask] = m_hashTable[hash];
               m_hashTable[hash] = cur++;
            }
            anchor = cur;
         }
         else {
            const auto hash = hash32 (&in[cur]);

            m_prevTable[cur & WindowMask] = m_hashTable[hash];
            m_hashTable[hash] = cur++;
         }
      }

      if (anchor != cur) {
         const auto run = cur - anchor;

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
      auto op = out;
      auto ip = in;

      const auto opEnd = op + outLength;
      const auto ipEnd = ip + inputLength;

      while (ip < ipEnd) {
         const auto token = *ip++;

         if (token >= 32) {
            auto run = token >> 5;

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
         auto length = (token & 15) + MinMatch;

         if (length == (15 + MinMatch)) {
            length += decode (ip);
         }

         if ((opEnd - op) < length) {
            return UncompressFailure;
         }
         const auto dist = ((token & 16) << 12) + load <uint16> (ip);
         ip += 2;

         auto cp = op - dist;

         if ((op - out) < dist) {
            return UncompressFailure;
         }

         if (dist >= 8) {
            copy (op, cp, length);
            op += length;
         }
         else {
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
   template <typename U> U load (void *ptr) {
      U ret;
      memcpy (&ret, ptr, sizeof (U));

      return ret;
   }

   void store16 (void *ptr, int32 val) {
      memcpy (ptr, &val, sizeof (uint16));
   }

   void copy64 (void *dst, void *src) {
      memcpy (dst, src, sizeof (uint64));
   }

   uint32 hash32 (void *ptr) {
      return (load <uint32> (ptr) * 0x9e3779b9) >> (32 - HashBits);
   }

   void copy (uint8 *dst, uint8 *src, int32 count) {
      copy64 (dst, src);

      for (int32 i = 8; i < count; i += 8) {
         copy64 (dst + i, src + i);
      }
   }

   void add (uint8 *&dst, int32 val) {
      *dst++ = static_cast <uint8> (val);
   }

   void encode (uint8 *&ptr, uint32 val) {
      while (val >= 128) {
         val -= 128;

         *ptr++ = 128 + (val & 127);
         val >>= 7;
      }
      *ptr++ = static_cast <uint8> (val);
   }

   uint32 decode (uint8 *&ptr) {
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