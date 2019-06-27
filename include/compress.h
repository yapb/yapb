//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// see https://github.com/encode84/ulz/
class FastLZ final : NonCopyable {
public:
   static constexpr int EXCESS = 16;
   static constexpr int WINDOW_BITS = 17;
   static constexpr int WINDOW_SIZE = cr::bit (WINDOW_BITS);
   static constexpr int WINDOW_MASK = WINDOW_SIZE - 1;

   static constexpr int MIN_MATCH = 4;
   static constexpr int MAX_CHAIN = cr::bit (5);

   static constexpr int HASH_BITS = 19;
   static constexpr int HASH_SIZE = cr::bit (HASH_BITS);
   static constexpr int NIL = -1;
   static constexpr int UNCOMPRESS_RESULT_FAILED = -1;

private:
   int *m_hashTable;
   int *m_prevTable;

public:
   FastLZ (void) {
      m_hashTable = new int[HASH_SIZE];
      m_prevTable = new int[WINDOW_SIZE];
   }

   ~FastLZ (void) {
      delete [] m_hashTable;
      delete [] m_prevTable;
   }

public:
   int compress (uint8 *in, int inLength, uint8 *out) {
      for (int i = 0; i < HASH_SIZE; i++) {
         m_hashTable[i] = NIL;
      }
      uint8 *op = out;

      int anchor = 0;
      int cur = 0;

      while (cur < inLength) {
         const int maxMatch = inLength - cur;

         int bestLength = 0;
         int dist = 0;

         if (maxMatch >= MIN_MATCH) {
            const int limit = cr::max (cur - WINDOW_SIZE, NIL);

            int chainLength = MAX_CHAIN;
            int lookup = m_hashTable[hash32 (&in[cur])];

            while (lookup > limit) {
               if (in[lookup + bestLength] == in[cur + bestLength] && load32 (&in[lookup]) == load32 (&in[cur])) {
                  int length = MIN_MATCH;

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
               lookup = m_prevTable[lookup & WINDOW_MASK];
            }
         }

         if (bestLength == MIN_MATCH && (cur - anchor) >= (7 + 128)) {
            bestLength = 0;
         }

         if (bestLength >= MIN_MATCH && bestLength < maxMatch && (cur - anchor) != 6) {
            const int next = cur + 1;
            const int targetLength = bestLength + 1;
            const int limit = cr::max (next - WINDOW_SIZE, NIL);

            int chainLength = MAX_CHAIN;
            int lookup = m_hashTable[hash32 (&in[next])];

            while (lookup > limit) {
               if (in[lookup + bestLength] == in[next + bestLength] && load32 (&in[lookup]) == load32 (&in[next])) {
                  int length = MIN_MATCH;

                  while (length < targetLength && in[lookup + length] == in[next + length]) {
                     length++;
                  }

                  if (length == targetLength) {
                     bestLength = 0;
                     break;
                  }
               }

               if (--chainLength == 0) {
                  break;
               }
               lookup = m_prevTable[lookup & WINDOW_MASK];
            }
         }

         if (bestLength >= MIN_MATCH) {
            const int length = bestLength - MIN_MATCH;
            const int token = ((dist >> 12) & 16) + cr::min (length, 15);

            if (anchor != cur) {
               const int run = cur - anchor;

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

               m_prevTable[cur & WINDOW_MASK] = m_hashTable[hash];
               m_hashTable[hash] = cur++;
            }
            anchor = cur;
         }
         else {
            const uint32 hash = hash32 (&in[cur]);

            m_prevTable[cur & WINDOW_MASK] = m_hashTable[hash];
            m_hashTable[hash] = cur++;
         }
      }

      if (anchor != cur) {
         const int run = cur - anchor;

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

   int uncompress (uint8 *in, int inLength, uint8 *out, int outLength) {
      uint8 *op = out;
      uint8 *ip = in;

      const uint8 *opEnd = op + outLength;
      const uint8 *ipEnd = ip + inLength;

      while (ip < ipEnd) {
         const int token = *ip++;

         if (token >= 32) {
            int run = token >> 5;

            if (run == 7) {
               run += decode (ip);
            }

            if ((opEnd - op) < run || (ipEnd - ip) < run) {
               return UNCOMPRESS_RESULT_FAILED;
            }
            copy (op, ip, run);

            op += run;
            ip += run;

            if (ip >= ipEnd) {
               break;
            }
         }
         int length = (token & 15) + MIN_MATCH;

         if (length == (15 + MIN_MATCH)) {
            length += decode (ip);
         }

         if ((opEnd - op) < length) {
            return UNCOMPRESS_RESULT_FAILED;
         }
         const int dist = ((token & 16) << 12) + load16 (ip);
         ip += 2;

         uint8 *cp = op - dist;

         if ((op - out) < dist) {
            return UNCOMPRESS_RESULT_FAILED;
         }

         if (dist >= 8) {
            copy (op, cp, length);
            op += length;
         }
         else
         {
            for (int i = 0; i < 4; i++) {
               *op++ = *cp++;
            }

            while (length-- != 4) {
               *op++ = *cp++;
            }
         }
      }
      return (ip == ipEnd) ? op - out : UNCOMPRESS_RESULT_FAILED;
   }

private:
   inline uint16 load16 (void *ptr) {
      return *reinterpret_cast <const uint16 *> (ptr);
   }

   inline uint32 load32 (void *ptr) {
      return *reinterpret_cast <const uint32 *> (ptr);
   }

   inline void store16 (void *ptr, int val) {
      *reinterpret_cast <uint16 *> (ptr) = static_cast <uint16> (val);
   }

   inline void copy64 (void *dst, void *src) {
      *reinterpret_cast <uint64 *> (dst) = *reinterpret_cast <const uint64 *> (src);
   }

   inline uint32 hash32 (void *ptr) {
      return (load32 (ptr) * 0x9E3779B9) >> (32 - HASH_BITS);
   }

   inline void copy (uint8 *dst, uint8 *src, int count) {
      copy64 (dst, src);

      for (int i = 8; i < count; i += 8) {
         copy64 (dst + i, src + i);
      }
   }

   inline void add (uint8 *&dst, int val) {
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

      for (int i = 0; i <= 21; i += 7) {
         const uint32 cur = *ptr++;
         val += cur << i;

         if (cur < 128) {
            break;
         }
      }
      return val;
   }
};