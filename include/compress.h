//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

static const int MAXBUF = 4096, PADDING = 18, THRESHOLD = 2, NIL = MAXBUF;

class Compress {
protected:
   unsigned long int m_textSize;
   unsigned long int m_codeSize;

   uint8 m_textBuffer[MAXBUF + PADDING - 1];
   int m_matchPosition;
   int m_matchLength;

   int m_left[MAXBUF + 1];
   int m_right[MAXBUF + 257];
   int m_parent[MAXBUF + 1];

private:
   void initTrees (void) {
      for (int i = MAXBUF + 1; i <= MAXBUF + 256; i++)
         m_right[i] = NIL;

      for (int j = 0; j < MAXBUF; j++)
         m_parent[j] = NIL;
   }

   void insert (int node) {
      int i;

      int compare = 1;

      uint8 *key = &m_textBuffer[node];
      int temp = MAXBUF + 1 + key[0];

      m_right[node] = m_left[node] = NIL;
      m_matchLength = 0;

      for (;;) {
         if (compare >= 0) {
            if (m_right[temp] != NIL)
               temp = m_right[temp];
            else {
               m_right[temp] = node;
               m_parent[node] = temp;
               return;
            }
         }
         else {
            if (m_left[temp] != NIL)
               temp = m_left[temp];
            else {
               m_left[temp] = node;
               m_parent[node] = temp;
               return;
            }
         }

         for (i = 1; i < PADDING; i++)
            if ((compare = key[i] - m_textBuffer[temp + i]) != 0)
               break;

         if (i > m_matchLength) {
            m_matchPosition = temp;

            if ((m_matchLength = i) >= PADDING)
               break;
         }
      }

      m_parent[node] = m_parent[temp];
      m_left[node] = m_left[temp];
      m_right[node] = m_right[temp];
      m_parent[m_left[temp]] = node;
      m_parent[m_right[temp]] = node;

      if (m_right[m_parent[temp]] == temp)
         m_right[m_parent[temp]] = node;
      else
         m_left[m_parent[temp]] = node;

      m_parent[temp] = NIL;
   }

   void erase (int node) {
      int temp;

      if (m_parent[node] == NIL)
         return; // not in tree

      if (m_right[node] == NIL)
         temp = m_left[node];

      else if (m_left[node] == NIL)
         temp = m_right[node];

      else {
         temp = m_left[node];

         if (m_right[temp] != NIL) {
            do
               temp = m_right[temp];
            while (m_right[temp] != NIL);

            m_right[m_parent[temp]] = m_left[temp];
            m_parent[m_left[temp]] = m_parent[temp];
            m_left[temp] = m_left[node];
            m_parent[m_left[node]] = temp;
         }

         m_right[temp] = m_right[node];
         m_parent[m_right[node]] = temp;
      }

      m_parent[temp] = m_parent[node];

      if (m_right[m_parent[node]] == node)
         m_right[m_parent[node]] = temp;
      else
         m_left[m_parent[node]] = temp;

      m_parent[node] = NIL;
   }

public:
   Compress (void) {
      m_textSize = 0;
      m_codeSize = 0;

      m_matchPosition = 0;
      m_matchLength = 0;

      memset (m_textBuffer, 0, sizeof (m_textBuffer));
      memset (m_right, 0, sizeof (m_right));
      memset (m_left, 0, sizeof (m_left));
      memset (m_parent, 0, sizeof (m_parent));
   }

   ~Compress (void) {
      m_textSize = 0;
      m_codeSize = 0;

      m_matchPosition = 0;
      m_matchLength = 0;

      memset (m_textBuffer, 0, sizeof (m_textBuffer));
      memset (m_right, 0, sizeof (m_right));
      memset (m_left, 0, sizeof (m_left));
      memset (m_parent, 0, sizeof (m_parent));
   }

   int encode_ (const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize) {
      int i, length, node, strPtr, lastMatchLength, codeBufferPtr, bufferPtr = 0;
      uint8 codeBuffer[17], mask;

      File fp (fileName, "wb");

      if (!fp.isValid ())
         return -1;

      uint8 bit;

      fp.write (header, headerSize, 1);
      initTrees ();

      codeBuffer[0] = 0;
      codeBufferPtr = mask = 1;
      strPtr = 0;
      node = MAXBUF - PADDING;

      for (i = strPtr; i < node; i++)
         m_textBuffer[i] = ' ';

      for (length = 0; (length < PADDING) && (bufferPtr < bufferSize); length++) {
         bit = buffer[bufferPtr++];
         m_textBuffer[node + length] = bit;
      }

      if ((m_textSize = length) == 0)
         return -1;

      for (i = 1; i <= PADDING; i++)
         insert (node - i);
      insert (node);

      do {
         if (m_matchLength > length)
            m_matchLength = length;

         if (m_matchLength <= THRESHOLD) {
            m_matchLength = 1;
            codeBuffer[0] |= mask;
            codeBuffer[codeBufferPtr++] = m_textBuffer[node];
         }
         else {
            codeBuffer[codeBufferPtr++] = (uint8)m_matchPosition;
            codeBuffer[codeBufferPtr++] = (uint8) (((m_matchPosition >> 4) & 0xf0) | (m_matchLength - (THRESHOLD + 1)));
         }

         if ((mask <<= 1) == 0) {
            for (i = 0; i < codeBufferPtr; i++)
               fp.putch (codeBuffer[i]);

            m_codeSize += codeBufferPtr;
            codeBuffer[0] = 0;
            codeBufferPtr = mask = 1;
         }
         lastMatchLength = m_matchLength;

         for (i = 0; (i < lastMatchLength) && (bufferPtr < bufferSize); i++) {
            bit = buffer[bufferPtr++];
            erase (strPtr);

            m_textBuffer[strPtr] = bit;

            if (strPtr < PADDING - 1)
               m_textBuffer[strPtr + MAXBUF] = bit;

            strPtr = (strPtr + 1) & (MAXBUF - 1);
            node = (node + 1) & (MAXBUF - 1);
            insert (node);
         }

         while (i++ < lastMatchLength) {
            erase (strPtr);

            strPtr = (strPtr + 1) & (MAXBUF - 1);
            node = (node + 1) & (MAXBUF - 1);

            if (length--)
               insert (node);
         }
      } while (length > 0);

      if (codeBufferPtr > 1) {
         for (i = 0; i < codeBufferPtr; i++)
            fp.putch (codeBuffer[i]);

         m_codeSize += codeBufferPtr;
      }
      fp.close ();

      return m_codeSize;
   }

   int decode_ (const char *fileName, int headerSize, uint8 *buffer, int bufferSize) {
      int i, j, k, node;
      unsigned int flags;
      int bufferPtr = 0;

      uint8 bit;

      File fp (fileName, "rb");

      if (!fp.isValid ())
         return -1;

      fp.seek (headerSize, SEEK_SET);

      node = MAXBUF - PADDING;
      for (i = 0; i < node; i++)
         m_textBuffer[i] = ' ';

      flags = 0;

      for (;;) {
         if (((flags >>= 1) & 256) == 0) {
            int read = fp.getch ();

            if (read == EOF)
               break;

            bit = static_cast<uint8> (read);

            flags = bit | 0xff00;
         }

         if (flags & 1) {
            int read = fp.getch ();

            if (read == EOF)
               break;

            bit = static_cast<uint8> (read);
            buffer[bufferPtr++] = bit;

            if (bufferPtr > bufferSize)
               return -1;

            m_textBuffer[node++] = bit;
            node &= (MAXBUF - 1);
         }
         else {
            if ((i = fp.getch ()) == EOF)
               break;

            if ((j = fp.getch ()) == EOF)
               break;

            i |= ((j & 0xf0) << 4);
            j = (j & 0x0f) + THRESHOLD;

            for (k = 0; k <= j; k++) {
               bit = m_textBuffer[(i + k) & (MAXBUF - 1)];
               buffer[bufferPtr++] = bit;

               if (bufferPtr > bufferSize)
                  return -1;

               m_textBuffer[node++] = bit;
               node &= (MAXBUF - 1);
            }
         }
      }
      fp.close ();

      return bufferPtr;
   }

   // external decoder
   static int decode (const char *fileName, int headerSize, uint8 *buffer, int bufferSize) {
      static Compress compressor = Compress ();
      return compressor.decode_ (fileName, headerSize, buffer, bufferSize);
   }

   // external encoder
   static int encode (const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize) {
      static Compress compressor = Compress ();
      return compressor.encode_ (fileName, header, headerSize, buffer, bufferSize);
   }
};
