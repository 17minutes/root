// @(#)root/io:$Id$
// Author: Jakob Blomer

/*************************************************************************
 * Copyright (C) 1995-2018, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RConfig.h>
#include <ROOT/RRawFile.hxx>
#ifdef _WIN32
#include <ROOT/RRawFileWin.hxx>
#else
#include <ROOT/RRawFileUnix.hxx>
#endif

#include "TError.h"
#include "TPluginManager.h"
#include "TROOT.h"

#include <algorithm>
#include <cctype> // for towlower
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
const char *kTransportSeparator = "://";
// Corresponds to ELineBreaks
#ifdef _WIN32
const char *kLineBreakTokens[] = {"", "\r\n", "\n", "\r\n"};
constexpr unsigned int kLineBreakTokenSizes[] = {0, 2, 1, 2};
#else
const char *kLineBreakTokens[] = {"", "\n", "\n", "\r\n"};
constexpr unsigned int kLineBreakTokenSizes[] = {0, 1, 1, 2};
#endif
constexpr unsigned int kLineBuffer = 128; // On Readln, look for line-breaks in chunks of 128 bytes
} // anonymous namespace

size_t ROOT::Experimental::Detail::RRawFile::RBlockBuffer::CopyTo(void *buffer, size_t nbytes, std::uint64_t offset)
{
   if (offset < fBufferOffset)
      return 0;

   size_t copiedBytes = 0;
   std::uint64_t offsetInBuffer = offset - fBufferOffset;
   if (offsetInBuffer < static_cast<std::uint64_t>(fBufferSize)) {
      size_t bytesInBuffer = std::min(nbytes, static_cast<size_t>(fBufferSize - offsetInBuffer));
      memcpy(buffer, fBuffer + offsetInBuffer, bytesInBuffer);
      copiedBytes = bytesInBuffer;
   }
   return copiedBytes;
}

ROOT::Experimental::Detail::RRawFile::RRawFile(std::string_view url, ROptions options)
   : fBlockBufferIdx(0), fBufferSpace(nullptr), fFileSize(kUnknownFileSize), fIsOpen(false), fUrl(url),
     fOptions(options), fFilePos(0)
{
}

ROOT::Experimental::Detail::RRawFile::~RRawFile()
{
   delete[] fBufferSpace;
}

ROOT::Experimental::Detail::RRawFile *
ROOT::Experimental::Detail::RRawFile::Create(std::string_view url, ROptions options)
{
   std::string transport = GetTransport(url);
   if (transport == "file") {
#ifdef _WIN32
      return new RRawFileWin(url, options);
#else
      return new RRawFileUnix(url, options);
#endif
   }
   if (transport == "http" || transport == "https") {
      if (TPluginHandler *h = gROOT->GetPluginManager()->FindHandler("ROOT::Experimental::Detail::RRawFile")) {
         if (h->LoadPlugin() == 0) {
            return reinterpret_cast<RRawFile *>(h->ExecPlugin(2, &url, &options));
         }
         throw std::runtime_error("Cannot load plugin handler for RRawFileDavix");
      }
      throw std::runtime_error("Cannot find plugin handler for RRawFileDavix");
   }
   throw std::runtime_error("Unsupported transport protocol: " + transport);
}

void *ROOT::Experimental::Detail::RRawFile::DoMap(size_t /* nbytes */, std::uint64_t /* offset */,
   std::uint64_t& /* mapdOffset */)
{
   throw std::runtime_error("Memory mapping unsupported");
}

void ROOT::Experimental::Detail::RRawFile::DoUnmap(void * /* region */, size_t /* nbytes */)
{
   throw std::runtime_error("Memory mapping unsupported");
}

std::string ROOT::Experimental::Detail::RRawFile::GetLocation(std::string_view url)
{
   auto idx = url.find(kTransportSeparator);
   if (idx == std::string_view::npos)
      return std::string(url);
   return std::string(url.substr(idx + strlen(kTransportSeparator)));
}

std::uint64_t ROOT::Experimental::Detail::RRawFile::GetSize()
{
   if (!fIsOpen)
      DoOpen();
   fIsOpen = true;

   if (fFileSize == kUnknownFileSize)
      fFileSize = DoGetSize();
   return fFileSize;
}

std::string ROOT::Experimental::Detail::RRawFile::GetTransport(std::string_view url)
{
   auto idx = url.find(kTransportSeparator);
   if (idx == std::string_view::npos)
      return "file";
   std::string transport(url.substr(0, idx));
   std::transform(transport.begin(), transport.end(), transport.begin(), ::tolower);
   return transport;
}

void *ROOT::Experimental::Detail::RRawFile::Map(size_t nbytes, std::uint64_t offset, std::uint64_t &mapdOffset)
{
   if (!fIsOpen)
      DoOpen();
   fIsOpen = true;
   return DoMap(nbytes, offset, mapdOffset);
}

size_t ROOT::Experimental::Detail::RRawFile::Read(void *buffer, size_t nbytes)
{
   size_t res = ReadAt(buffer, nbytes, fFilePos);
   fFilePos += res;
   return res;
}

size_t ROOT::Experimental::Detail::RRawFile::ReadAt(void *buffer, size_t nbytes, std::uint64_t offset)
{
   if (!fIsOpen)
      DoOpen();
   R__ASSERT(fOptions.fBlockSize >= 0);
   fIsOpen = true;

   // "Large" reads are served directly, bypassing the cache
   if (nbytes > static_cast<unsigned int>(fOptions.fBlockSize))
      return DoReadAt(buffer, nbytes, offset);

   if (fBufferSpace == nullptr) {
      fBufferSpace = new unsigned char[kNumBlockBuffers * fOptions.fBlockSize];
      for (unsigned int i = 0; i < kNumBlockBuffers; ++i)
         fBlockBuffers[i].fBuffer = fBufferSpace + i * fOptions.fBlockSize;
   }

   size_t totalBytes = 0;
   size_t copiedBytes = 0;
   /// Try to serve as many bytes as possible from the block buffers
   for (unsigned int idx = fBlockBufferIdx; idx < fBlockBufferIdx + kNumBlockBuffers; ++idx) {
      copiedBytes = fBlockBuffers[idx % kNumBlockBuffers].CopyTo(buffer, nbytes, offset);
      buffer = reinterpret_cast<unsigned char *>(buffer) + copiedBytes;
      nbytes -= copiedBytes;
      offset += copiedBytes;
      totalBytes += copiedBytes;
      if (copiedBytes > 0)
         fBlockBufferIdx = idx;
      if (nbytes == 0)
         return totalBytes;
   }
   fBlockBufferIdx++;

   /// The request was not fully satisfied and fBlockBufferIdx now points to the previous shadow buffer

   /// The remaining bytes populate the newly promoted main buffer
   RBlockBuffer *thisBuffer = &fBlockBuffers[fBlockBufferIdx % kNumBlockBuffers];
   size_t res = DoReadAt(thisBuffer->fBuffer, fOptions.fBlockSize, offset);
   thisBuffer->fBufferOffset = offset;
   thisBuffer->fBufferSize = res;
   size_t remainingBytes = std::min(res, nbytes);
   memcpy(buffer, thisBuffer->fBuffer, remainingBytes);
   totalBytes += remainingBytes;
   return totalBytes;
}

bool ROOT::Experimental::Detail::RRawFile::Readln(std::string &line)
{
   if (fOptions.fLineBreak == ELineBreaks::kAuto) {
      // Auto-detect line breaks according to the break discovered in the first line
      fOptions.fLineBreak = ELineBreaks::kUnix;
      bool res = Readln(line);
      if ((line.length() > 0) && (*line.rbegin() == '\r')) {
         fOptions.fLineBreak = ELineBreaks::kWindows;
         line.resize(line.length() - 1);
      }
      return res;
   }

   line.clear();
   char buffer[kLineBuffer];
   size_t nbytes;
   do {
      nbytes = Read(buffer, sizeof(buffer));
      std::string_view bufferView(buffer, nbytes);
      auto idx = bufferView.find(kLineBreakTokens[static_cast<int>(fOptions.fLineBreak)]);
      if (idx != std::string_view::npos) {
         // Line break found, return the string and skip the linebreak itself
         line.append(buffer, idx);
         fFilePos -= nbytes - idx;
         fFilePos += kLineBreakTokenSizes[static_cast<int>(fOptions.fLineBreak)];
         return true;
      }
      line.append(buffer, nbytes);
   } while (nbytes > 0);

   return !line.empty();
}

void ROOT::Experimental::Detail::RRawFile::Seek(std::uint64_t offset)
{
   fFilePos = offset;
}

void ROOT::Experimental::Detail::RRawFile::Unmap(void *region, size_t nbytes)
{
   if (!fIsOpen)
      throw std::runtime_error("Cannot unmap, file not open");
   DoUnmap(region, nbytes);
}
