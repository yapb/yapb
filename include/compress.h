//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

static constexpr int MAXBUF = 4096, PADDING = 18, THRESHOLD = 2, NIL = MAXBUF;

class Compress {
protected:
   unsigned long int m_csize;

   uint8 m_buffer[MAXBUF + PADDING - 1];
   int m_matchPos;
   int m_matchLen;

   int m_left[MAXBUF + 1];
   int m_right[MAXBUF + 257];
   int m_parent[MAXBUF + 1];

private:
   void initTrees (void) {
      for (int i = MAXBUF + 1; i <= MAXBUF + 256; i++) {
         m_right[i] = NIL;
      }

      for (int j = 0; j < MAXBUF; j++) {
         m_parent[j] = NIL;
      }
   }

   void insert (int node) {
      int i;

      int compare = 1;

      uint8 *key = &m_buffer[node];
      int temp = MAXBUF + 1 + key[0];

      m_right[node] = m_left[node] = NIL;
      m_matchLen = 0;

      for (;;) {
         if (compare >= 0) {
            if (m_right[temp] != NIL) {
               temp = m_right[temp];
            }
            else {
               m_right[temp] = node;
               m_parent[node] = temp;
               return;
            }
         }
         else {
            if (m_left[temp] != NIL) {
               temp = m_left[temp];
            }
            else {
               m_left[temp] = node;
               m_parent[node] = temp;

               return;
            }
         }

         for (i = 1; i < PADDING; i++) {
            if ((compare = key[i] - m_buffer[temp + i]) != 0) {
               break;
            }
         }

         if (i > m_matchLen) {
            m_matchPos = temp;

            if ((m_matchLen = i) >= PADDING) {
               break;
            }
         }
      }

      m_parent[node] = m_parent[temp];
      m_left[node] = m_left[temp];
      m_right[node] = m_right[temp];
      m_parent[m_left[temp]] = node;
      m_parent[m_right[temp]] = node;

      if (m_right[m_parent[temp]] == temp) {
         m_right[m_parent[temp]] = node;
      }
      else {
         m_left[m_parent[temp]] = node;
      }
      m_parent[temp] = NIL;
   }

   void erase (int node) {
      int temp;

      if (m_parent[node] == NIL) {
         return; // not in tree
      }

      if (m_right[node] == NIL) {
         temp = m_left[node];
      }
      else if (m_left[node] == NIL) {
         temp = m_right[node];
      }
      else {
         temp = m_left[node];

         if (m_right[temp] != NIL) {
            do {
               temp = m_right[temp];
            } while (m_right[temp] != NIL);

            m_right[m_parent[temp]] = m_left[temp];
            m_parent[m_left[temp]] = m_parent[temp];
            m_left[temp] = m_left[node];
            m_parent[m_left[node]] = temp;
         }

         m_right[temp] = m_right[node];
         m_parent[m_right[node]] = temp;
      }
      m_parent[temp] = m_parent[node];

      if (m_right[m_parent[node]] == node) {
         m_right[m_parent[node]] = temp;
      }
      else {
         m_left[m_parent[node]] = temp;
      }
      m_parent[node] = NIL;
   }

public:
   Compress (void) : m_csize (0), m_matchPos (0), m_matchLen (0) {
      memset (m_right, 0, sizeof (m_right));
      memset (m_left, 0, sizeof (m_left));
      memset (m_parent, 0, sizeof (m_parent));
      memset (m_buffer, 0, sizeof (m_buffer));
   }

   ~Compress (void) = default;

   int encode_ (const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize) {
      File fp (fileName, "wb");

      if (!fp.isValid ()) {
         return -1;
      }
      int i, length, node, ptr, last, cbp, bp = 0;
      uint8 cb[17] = {0, }, mask, bit;

      fp.write (header, headerSize, 1);
      initTrees ();

      cb[0] = 0;
      cbp = mask = 1;
      ptr = 0;
      node = MAXBUF - PADDING;

      for (i = ptr; i < node; i++)
         m_buffer[i] = ' ';

      for (length = 0; (length < PADDING) && (bp < bufferSize); length++) {
         bit = buffer[bp++];
         m_buffer[node + length] = bit;
      }

      if (length == 0) {
         return -1;
      }

      for (i = 1; i <= PADDING; i++) {
         insert (node - i);
      }
      insert (node);

      do {
         if (m_matchLen > length) {
            m_matchLen = length;
         }
         if (m_matchLen <= THRESHOLD) {
            m_matchLen = 1;

            cb[0] |= mask;
            cb[cbp++] = m_buffer[node];
         }
         else {
            cb[cbp++] = (uint8) (m_matchPos & 0xff);
            cb[cbp++] = (uint8) (((m_matchPos >> 4) & 0xf0) | (m_matchLen - (THRESHOLD + 1)));
         }

         if ((mask <<= 1) == 0) {
            for (i = 0; i < cbp; i++) {
               fp.putch (cb[i]);
            }
            m_csize += cbp;
            cb[0] = 0;
            cbp = mask = 1;
         }
         last = m_matchLen;

         for (i = 0; (i < last) && (bp < bufferSize); i++) {
            bit = buffer[bp++];
            erase (ptr);

            m_buffer[ptr] = bit;

            if (ptr < PADDING - 1) {
               m_buffer[ptr + MAXBUF] = bit;
            }
            ptr = (ptr + 1) & (MAXBUF - 1);
            node = (node + 1) & (MAXBUF - 1);
            insert (node);
         }

         while (i++ < last) {
            erase (ptr);

            ptr = (ptr + 1) & (MAXBUF - 1);
            node = (node + 1) & (MAXBUF - 1);

            if (length--) {
               insert (node);
            }
         }
      } while (length > 0);

      if (cbp > 1) {
         for (i = 0; i < cbp; i++) {
            fp.putch (cb[i]);
         }
         m_csize += cbp;
      }
      fp.close ();

      return m_csize;
   }

   int decode_ (const char *fileName, int headerSize, uint8 *buffer, int bufferSize) {
      int i, j, k, node;
      unsigned int flags;
      int bp = 0;

      uint8 bit;

      File fp (fileName, "rb");

      if (!fp.isValid ()) {
         return -1;
      }
      fp.seek (headerSize, SEEK_SET);

      node = MAXBUF - PADDING;

      for (i = 0; i < node; i++) {
         m_buffer[i] = ' ';
      }
      flags = 0;

      for (;;) {
         if (((flags >>= 1) & 256) == 0) {
            int read = fp.getch ();

            if (read == EOF) {
               break;
            }
            bit = static_cast <uint8> (read);
            flags = bit | 0xff00;
         }

         if (flags & 1) {
            int read = fp.getch ();

            if (read == EOF) {
               break;
            }
            bit = static_cast <uint8> (read);
            buffer[bp++] = bit;

            if (bp > bufferSize) {
               return -1;
            }
            m_buffer[node++] = bit;
            node &= (MAXBUF - 1);
         }
         else {
            if ((i = fp.getch ()) == EOF) {
               break;
            }

            if ((j = fp.getch ()) == EOF) {
               break;
            }

            i |= ((j & 0xf0) << 4);
            j = (j & 0x0f) + THRESHOLD;

            for (k = 0; k <= j; k++) {
               bit = m_buffer[(i + k) & (MAXBUF - 1)];
               buffer[bp++] = bit;

               if (bp > bufferSize) {
                  return -1;
               }
               m_buffer[node++] = bit;
               node &= (MAXBUF - 1);
            }
         }
      }
      fp.close ();

      return bp;
   }

   // external decoder
   static int decode (const char *fileName, int headerSize, uint8 *buffer, int bufferSize) {
      static Compress compressor;
      return compressor.decode_ (fileName, headerSize, buffer, bufferSize);
   }

   // external encoder
   static int encode (const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize) {
      static Compress compressor;
      return compressor.encode_ (fileName, header, headerSize, buffer, bufferSize);
   }
};
