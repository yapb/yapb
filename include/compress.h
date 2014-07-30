//
// Copyright (c) 2014, by YaPB Development Team. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Version: $Id:$
//

#ifndef COMPRESS_INCLUDED
#define COMPRESS_INCLUDED

const int N = 4096, F = 18, THRESHOLD = 2, NIL = N;

class Compressor
{
protected:
   unsigned long int m_textSize;
   unsigned long int m_codeSize;

   byte m_textBuffer[N + F - 1];
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

      byte *key = &m_textBuffer[node];
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
   }

   ~Compressor (void)
   {
      m_textSize = 0;
      m_codeSize = 0;
   }

   int InternalEncode (char *fileName, byte *header, int headerSize, byte *buffer, int bufferSize)
   {
      int i, bit, length, node, strPtr, lastMatchLength, codeBufferPtr, bufferPtr = 0;
      byte codeBuffer[17], mask;

      File fp (fileName, "wb");

      if (!fp.IsValid ())
         return -1;

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
            codeBuffer[codeBufferPtr++] = (unsigned char) m_matchPosition;
            codeBuffer[codeBufferPtr++] = (unsigned char) (((m_matchPosition >> 4) & 0xf0) | (m_matchLength - (THRESHOLD + 1)));
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

   int InternalDecode (char *fileName, int headerSize, byte *buffer, int bufferSize)
   {
      int i, j, k, node, bit;
      unsigned int flags;
      int bufferPtr = 0;

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
            if ((bit = fp.GetChar ()) == EOF)
               break;
            flags = bit | 0xff00;
         } 

         if (flags & 1)
         {
            if ((bit = fp.GetChar ()) == EOF)
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
   static int Uncompress (char *fileName, int headerSize, byte *buffer, int bufferSize)
   {
      static Compressor compressor = Compressor ();
      return compressor.InternalDecode (fileName, headerSize, buffer, bufferSize);
   }

   // external encoder
   static int Compress(char *fileName, byte *header, int headerSize, byte *buffer, int bufferSize)
   {
      static Compressor compressor = Compressor ();
      return compressor.InternalEncode (fileName, header, headerSize, buffer, bufferSize);
   }
};

#endif // COMPRESS_INCLUDED
