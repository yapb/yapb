//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#pragma once

const int N = 4096, F = 18, THRESHOLD = 2, NIL = N;

class Compressor
{
protected:
   unsigned long int m_textSize;
   unsigned long int m_codeSize;

   uint8 m_textBuffer[N + F - 1];
   int m_matchPosition;
   int m_matchLength;

   int m_left[N + 1];
   int m_right[N + 257];
   int m_parent[N + 1];

private:
   void InitTree (void)
   {
      for (int i = N + 1; i <= N + 256; i++)
         m_right[i] = NIL;

      for (int j = 0; j < N; j++)
         m_parent[j] = NIL;
   }

   void InsertNode (int node)
   {
      int i;

      int compare = 1;

      uint8 *key = &m_textBuffer[node];
      int temp = N + 1 + key[0];

      m_right[node] = m_left[node] = NIL;
      m_matchLength = 0;

      for (;;)
      {
         if (compare >= 0)
         {
            if (m_right[temp] != NIL)
               temp = m_right[temp];
            else
            {
               m_right[temp] = node;
               m_parent[node] = temp;
               return;
            }
         }
         else
         {
            if (m_left[temp] != NIL)
               temp = m_left[temp];
            else
            {
               m_left[temp] = node;
               m_parent[node] = temp;
               return;
            }
         }

         for (i = 1; i < F; i++)
            if ((compare = key[i] - m_textBuffer[temp + i]) != 0)
               break;

         if (i > m_matchLength)
         {
            m_matchPosition = temp;
            
            if ((m_matchLength = i) >= F)
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


   void DeleteNode (int node)
   {
      int temp;

      if (m_parent[node] == NIL)
         return; // not in tree

      if (m_right[node] == NIL)
         temp = m_left[node];

      else if (m_left[node] == NIL) 
         temp = m_right[node];

      else
      {
         temp = m_left[node];

         if (m_right[temp] != NIL)
         {
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
   Compressor (void)
   {
      m_textSize = 0;
      m_codeSize = 0;

      m_matchPosition = 0;
      m_matchLength = 0;

      memset (m_textBuffer, 0, sizeof (m_textBuffer));
      memset (m_right, 0, sizeof (m_right));
      memset (m_left, 0, sizeof (m_left));
      memset (m_parent, 0, sizeof (m_parent));
   }

   ~Compressor (void)
   {
      m_textSize = 0;
      m_codeSize = 0;

      m_matchPosition = 0;
      m_matchLength = 0;

      memset (m_textBuffer, 0, sizeof (m_textBuffer));
      memset (m_right, 0, sizeof (m_right));
      memset (m_left, 0, sizeof (m_left));
      memset (m_parent, 0, sizeof (m_parent));
   }

   int InternalEncode (const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize)
   {
      int i, length, node, strPtr, lastMatchLength, codeBufferPtr, bufferPtr = 0;
      uint8 codeBuffer[17], mask;

      File fp (fileName, "wb");

      if (!fp.IsValid ())
         return -1;

      uint8 bit;

      fp.Write (header, headerSize, 1);
      InitTree ();

      codeBuffer[0] = 0;
      codeBufferPtr = mask = 1;
      strPtr = 0;
      node = N - F;

      for (i = strPtr; i < node; i++)
         m_textBuffer[i] = ' ';

      for (length = 0; (length < F) && (bufferPtr < bufferSize); length++)
      {
         bit = buffer[bufferPtr++];
         m_textBuffer[node + length] = bit;
      }

      if ((m_textSize = length) == 0)
         return -1;

      for (i = 1; i <= F; i++)
         InsertNode (node - i);
      InsertNode (node);

      do
      {
         if (m_matchLength > length)
            m_matchLength = length;

         if (m_matchLength <= THRESHOLD)
         {
            m_matchLength = 1;
            codeBuffer[0] |= mask;
            codeBuffer[codeBufferPtr++] = m_textBuffer[node];
         }
         else
         {
            codeBuffer[codeBufferPtr++] = (uint8) m_matchPosition;
            codeBuffer[codeBufferPtr++] = (uint8) (((m_matchPosition >> 4) & 0xf0) | (m_matchLength - (THRESHOLD + 1)));
         }

         if ((mask  <<= 1) == 0)
         {
            for (i = 0; i < codeBufferPtr; i++)
               fp.PutChar (codeBuffer[i]);

            m_codeSize += codeBufferPtr;
            codeBuffer[0] = 0;
            codeBufferPtr = mask = 1;
         }
         lastMatchLength = m_matchLength;

         for (i = 0; (i < lastMatchLength) && (bufferPtr < bufferSize); i++)
         {
            bit = buffer[bufferPtr++];
            DeleteNode (strPtr);

            m_textBuffer[strPtr] = bit;

            if (strPtr < F - 1)
               m_textBuffer[strPtr + N] = bit;

            strPtr = (strPtr + 1) & (N - 1);
            node = (node + 1) & (N - 1);
            InsertNode (node);
         }

         while (i++ < lastMatchLength)
         {
            DeleteNode (strPtr);

            strPtr = (strPtr + 1) & (N - 1);
            node = (node + 1) & (N - 1);

            if (length--)
               InsertNode (node);
         }
      } while (length > 0);

      if (codeBufferPtr > 1)
      {
         for (i = 0; i < codeBufferPtr; i++)
            fp.PutChar (codeBuffer[i]);

         m_codeSize += codeBufferPtr;
      }
      fp.Close ();

      return m_codeSize;
   }

   int InternalDecode (const char *fileName, int headerSize, uint8 *buffer, int bufferSize)
   {
      int i, j, k, node;
      unsigned int flags;
      int bufferPtr = 0;

      uint8 bit;

      File fp (fileName, "rb");

      if (!fp.IsValid ())
         return -1;

      fp.Seek (headerSize, SEEK_SET);

      node = N - F;
      for (i = 0; i < node; i++)
         m_textBuffer[i] = ' ';

      flags = 0;

      for (;;)
      {
         if (((flags >>= 1) & 256) == 0)
         {
            if ((bit = static_cast <uint8> (fp.GetChar ())) == EOF)
               break;
            flags = bit | 0xff00;
         } 

         if (flags & 1)
         {
            if ((bit = static_cast <uint8> (fp.GetChar ())) == EOF)
               break;
            buffer[bufferPtr++] = bit;

            if (bufferPtr > bufferSize)
               return -1;

            m_textBuffer[node++] = bit;
            node &= (N - 1);
         }
         else
         {
            if ((i = fp.GetChar ()) == EOF)
               break;

            if ((j = fp.GetChar ()) == EOF)
               break;

            i |= ((j & 0xf0) << 4);
            j = (j & 0x0f) + THRESHOLD;

            for (k = 0; k <= j; k++)
            {
               bit = m_textBuffer[(i + k) & (N - 1)];
               buffer[bufferPtr++] = bit;

               if (bufferPtr > bufferSize)
                  return -1;

               m_textBuffer[node++] = bit;
               node &= (N - 1);
            }
         }
      }
      fp.Close ();

      return bufferPtr;
   }

   // external decoder
   static int Uncompress (const char *fileName, int headerSize, uint8 *buffer, int bufferSize)
   {
      static Compressor compressor = Compressor ();
      return compressor.InternalDecode (fileName, headerSize, buffer, bufferSize);
   }

   // external encoder
   static int Compress(const char *fileName, uint8 *header, int headerSize, uint8 *buffer, int bufferSize)
   {
      static Compressor compressor = Compressor ();
      return compressor.InternalEncode (fileName, header, headerSize, buffer, bufferSize);
   }
};
