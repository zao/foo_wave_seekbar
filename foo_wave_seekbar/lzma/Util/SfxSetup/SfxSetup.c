/* SfxSetup.c - 7z SFX Setup
2010-11-11 : Igor Pavlov : Public domain */

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifdef _CONSOLE
#include <stdio.h>
#endif

#include "../../7z.h"
#include "../../7zAlloc.h"
#include "../../7zCrc.h"
#include "../../7zFile.h"
#include "../../CpuArch.h"

#define k_EXE_ExtIndex 1

static const char *kExts[] =
{
  "bat",
  "exe",
  "inf",
  "msi",
  #ifdef UNDER_CE
  "cab",
  #endif
  "html",
  "htm"
};

static const char *kNames[] =
{
  "setup",
  "install",
  "run",
  "start"
};

static unsigned FindExt(const wchar_t *s, unsigned *extLen)
{
  unsigned len = (unsigned)wcslen(s);
  unsigned i;
  for (i = len; i > 0; i--)
  {
    if (s[i - 1] == '.')
    {
      *extLen = len - i;
      return i - 1;
    }
  }
  *extLen = 0;
  return len;
}

#define MAKE_CHAR_UPPER(c) ((((c) >= 'a' && (c) <= 'z') ? (c) -= 0x20 : (c)))

static unsigned FindItem(const char **items, unsigned num, const wchar_t *s, unsigned len)
{
  unsigned i;
  for (i = 0; i < num; i++)
  {
    const char *item = items[i];
    unsigned itemLen = (unsigned)strlen(item);
    unsigned j;
    if (len != itemLen)
      continue;
    for (j = 0; j < len; j++)
    {
      unsigned c = item[j];
      if (c != s[j] && MAKE_CHAR_UPPER(c) != s[j])
        break;
    }
    if (j == len)
      return i;
  }
  return i;
}

#ifdef _CONSOLE
static BOOL WINAPI HandlerRoutine(DWORD ctrlType)
{
  ctrlType = ctrlType;
  return TRUE;
}
#endif

static void PrintErrorMessage(const char *message)
{
  #ifdef _CONSOLE
  printf("\n7-Zip Error: %s\n", message);
  #else
  #ifdef UNDER_CE
  WCHAR messageW[256 + 4];
  unsigned i;
  for (i = 0; i < 256 && message[i] != 0; i++)
    messageW[i] = message[i];
  messageW[i] = 0;
  MessageBoxW(0, messageW, L"7-Zip Error", MB_ICONERROR);
  #else
  MessageBoxA(0, message, "7-Zip Error", MB_ICONERROR);
  #endif
  #endif
}

static WRes MyCreateDir(const WCHAR *name)
{
  return CreateDirectoryW(name, NULL) ? 0 : GetLastError();
}

#ifdef UNDER_CE
#define kBufferSize (1 << 13)
#else
#define kBufferSize (1 << 15)
#endif

#define kSignatureSearchLimit (1 << 22)

static Bool FindSignature(CSzFile *stream, UInt64 *resPos)
{
  Byte buf[kBufferSize];
  size_t numPrevBytes = 0;
  *resPos = 0;
  for (;;)
  {
    size_t numTests, pos;
    if (*resPos > kSignatureSearchLimit)
      return False;
    
    do
    {
      size_t processed = kBufferSize - numPrevBytes;
      if (File_Read(stream, buf + numPrevBytes, &processed) != 0)
        return False;
      if (processed == 0)
        return False;
      numPrevBytes += processed;
    }
    while (numPrevBytes <= k7zStartHeaderSize);
    
    numTests = numPrevBytes - k7zStartHeaderSize;
    for (pos = 0; pos < numTests; pos++)
    {
      for (; buf[pos] != '7' && pos < numTests; pos++);
      if (pos == numTests)
        break;
      if (memcmp(buf + pos, k7zSignature, k7zSignatureSize) == 0)
        if (CrcCalc(buf + pos + 12, 20) == GetUi32(buf + pos + 8))
        {
          *resPos += pos;
          return True;
        }
    }
    *resPos += numTests;
    numPrevBytes -= numTests;
    memmove(buf, buf + numTests, numPrevBytes);
  }
}

static Bool DoesFileOrDirExist(const WCHAR *path)
{
  WIN32_FIND_DATAW fd;
  HANDLE handle;
  handle = FindFirstFileW(path, &fd);
  if (handle == INVALID_HANDLE_VALUE)
    return False;
  FindClose(handle);
  return True;
}

static WRes RemoveDirWithSubItems(WCHAR *path)
{
  WIN32_FIND_DATAW fd;
  HANDLE handle;
  WRes res = 0;
  size_t len = wcslen(path);
  wcscpy(path + len, L"*");
  handle = FindFirstFileW(path, &fd);
  path[len] = L'\0';
  if (handle == INVALID_HANDLE_VALUE)
    return GetLastError();
  for (;;)
  {
    if (wcscmp(fd.cFileName, L".") != 0 &&
        wcscmp(fd.cFileName, L"..") != 0)
    {
      wcscpy(path + len, fd.cFileName);
      if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
      {
        wcscat(path, L"\\");
        res = RemoveDirWithSubItems(path);
      }
      else
      {
        SetFileAttributesW(path, 0);
        if (DeleteFileW(path) == 0)
          res = GetLastError();
      }
      if (res != 0)
        break;
    }
    if (!FindNextFileW(handle, &fd))
    {
      res = GetLastError();
      if (res == ERROR_NO_MORE_FILES)
        res = 0;
      break;
    }
  }
  path[len] = L'\0';
  FindClose(handle);
  if (res == 0)
  {
    if (!RemoveDirectoryW(path))
      res = GetLastError();
  }
  return res;
}

#ifdef _CONSOLE
int MY_CDECL main()
#else
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
  #ifdef UNDER_CE
  LPWSTR
  #else
  LPSTR
  #endif
  lpCmdLine, int nCmdShow)
#endif
{
  CFileInStream archiveStream;
  CLookToRead lookStream;
  CSzArEx db;
  SRes res = SZ_OK;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  WCHAR sfxPath[MAX_PATH + 2];
  WCHAR path[MAX_PATH * 3 + 2];
  size_t pathLen;
  DWORD winRes;
  const wchar_t *cmdLineParams;
  const char *errorMessage = NULL;
  Bool useShellExecute = True;

  #ifdef _CONSOLE
  SetConsoleCtrlHandler(HandlerRoutine, TRUE);
  #else
  hInstance = hInstance;
  hPrevInstance = hPrevInstance;
  lpCmdLine = lpCmdLine;
  nCmdShow = nCmdShow;
  #endif

  CrcGenerateTable();

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;

  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  FileInStream_CreateVTable(&archiveStream);
  LookToRead_CreateVTable(&lookStream, False);
 
  winRes = GetModuleFileNameW(NULL, sfxPath, MAX_PATH);
  if (winRes == 0 || winRes > MAX_PATH)
    return 1;
  {
    cmdLineParams = GetCommandLineW();
    #ifndef UNDER_CE
    {
      Bool quoteMode = False;
      for (;; cmdLineParams++)
      {
        wchar_t c = *cmdLineParams;
        if (c == L'\"')
          quoteMode = !quoteMode;
        else if (c == 0 || (c == L' ' && !quoteMode))
          break;
      }
    }
    #endif
  }

  {
    unsigned i;
    DWORD d;
    winRes = GetTempPathW(MAX_PATH, path);
    if (winRes == 0 || winRes > MAX_PATH)
      return 1;
    pathLen = wcslen(path);
    d = (GetTickCount() << 12) ^ (GetCurrentThreadId() << 14) ^ GetCurrentProcessId();
    for (i = 0;; i++, d += GetTickCount())
    {
      if (i >= 100)
      {
        res = SZ_ERROR_FAIL;
        break;
      }
      wcscpy(path + pathLen, L"7z");

      {
        wchar_t *s = path + wcslen(path);
        UInt32 value = d;
        unsigned k;
        for (k = 0; k < 8; k++)
        {
          unsigned t = value & 0xF;
          value >>= 4;
          s[7 - k] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
        }
        s[k] = '\0';
      }

      if (DoesFileOrDirExist(path))
        continue;
      if (CreateDirectoryW(path, NULL))
      {
        wcscat(path, L"\\");
        pathLen = wcslen(path);
        break;
      }
      if (GetLastError() != ERROR_ALREADY_EXISTS)
      {
        res = SZ_ERROR_FAIL;
        break;
      }
    }
    if (res != SZ_OK)
      errorMessage = "Can't create temp folder";
  }

  if (res != SZ_OK)
  {
    if (!errorMessage)
      errorMessage = "Error";
    PrintErrorMessage(errorMessage);
    return 1;
  }

  if (InFile_OpenW(&archiveStream.file, sfxPath) != 0)
  {
    errorMessage = "can not open input file";
    res = SZ_ERROR_FAIL;
  }
  else
  {
    UInt64 pos = 0;
    if (!FindSignature(&archiveStream.file, &pos))
      res = SZ_ERROR_FAIL;
    else if (File_Seek(&archiveStream.file, (Int64 *)&pos, SZ_SEEK_SET) != 0)
      res = SZ_ERROR_FAIL;
    if (res != 0)
      errorMessage = "Can't find 7z archive";
  }

  if (res == SZ_OK)
  {
    lookStream.realStream = &archiveStream.s;
    LookToRead_Init(&lookStream);
  }

  SzArEx_Init(&db);
  if (res == SZ_OK)
  {
    res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp);
  }
  if (res == SZ_OK)
  {
    UInt32 executeFileIndex = (UInt32)(Int32)-1;
    UInt32 minPrice = 1 << 30;
    UInt32 i;
    UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
    Byte *outBuffer = 0; /* it must be 0 before first call for each new archive. */
    size_t outBufferSize = 0;  /* it can have any value before first call (if outBuffer = 0) */
    
    for (i = 0; i < db.db.NumFiles; i++)
    {
      size_t offset = 0;
      size_t outSizeProcessed = 0;
      const CSzFileItem *f = db.db.Files + i;
      size_t len;
      WCHAR *temp;
      len = SzArEx_GetFileNameUtf16(&db, i, NULL);
      
      if (len >= MAX_PATH)
      {
        res = SZ_ERROR_FAIL;
        break;
      }
      
      temp = path + pathLen;
      
      SzArEx_GetFileNameUtf16(&db, i, temp);
      {
        res = SzArEx_Extract(&db, &lookStream.s, i,
          &blockIndex, &outBuffer, &outBufferSize,
          &offset, &outSizeProcessed,
          &allocImp, &allocTempImp);
        if (res != SZ_OK)
          break;
      }
      {
        CSzFile outFile;
        size_t processedSize;
        size_t j;
        size_t nameStartPos = 0;
        for (j = 0; temp[j] != 0; j++)
        {
          if (temp[j] == '/')
          {
            temp[j] = 0;
            MyCreateDir(path);
            temp[j] = CHAR_PATH_SEPARATOR;
            nameStartPos = j + 1;
          }
        }

        if (f->IsDir)
        {
          MyCreateDir(path);
          continue;
        }
        else
        {
          unsigned extLen;
          const WCHAR *name = temp + nameStartPos;
          unsigned len = (unsigned)wcslen(name);
          unsigned nameLen = FindExt(temp + nameStartPos, &extLen);
          unsigned extPrice = FindItem(kExts, sizeof(kExts) / sizeof(kExts[0]), name + len - extLen, extLen);
          unsigned namePrice = FindItem(kNames, sizeof(kNames) / sizeof(kNames[0]), name, nameLen);

          unsigned price = namePrice + extPrice * 64 + (nameStartPos == 0 ? 0 : (1 << 12));
          if (minPrice > price)
          {
            minPrice = price;
            executeFileIndex = i;
            useShellExecute = (extPrice != k_EXE_ExtIndex);
          }
         
          if (DoesFileOrDirExist(path))
          {
            errorMessage = "Duplicate file";
            res = SZ_ERROR_FAIL;
            break;
          }
          if (OutFile_OpenW(&outFile, path))
          {
            errorMessage = "Can't open output file";
            res = SZ_ERROR_FAIL;
            break;
          }
        }
        processedSize = outSizeProcessed;
        if (File_Write(&outFile, outBuffer + offset, &processedSize) != 0 || processedSize != outSizeProcessed)
        {
          errorMessage = "Can't write output file";
          res = SZ_ERROR_FAIL;
        }
        
        #ifdef USE_WINDOWS_FILE
        if (f->MTimeDefined)
        {
          FILETIME mTime;
          mTime.dwLowDateTime = f->MTime.Low;
          mTime.dwHighDateTime = f->MTime.High;
          SetFileTime(outFile.handle, NULL, NULL, &mTime);
        }
        #endif
        
        {
          SRes res2 = File_Close(&outFile);
          if (res != SZ_OK)
            break;
          if (res2 != SZ_OK)
          {
            res = res2;
            break;
          }
        }
        #ifdef USE_WINDOWS_FILE
        if (f->AttribDefined)
          SetFileAttributesW(path, f->Attrib);
        #endif
      }
    }

    if (res == SZ_OK)
    {
      if (executeFileIndex == (UInt32)(Int32)-1)
      {
        errorMessage = "There is no file to execute";
        res = SZ_ERROR_FAIL;
      }
      else
      {
        WCHAR *temp = path + pathLen;
        UInt32 j;
        SzArEx_GetFileNameUtf16(&db, executeFileIndex, temp);
        for (j = 0; temp[j] != 0; j++)
          if (temp[j] == '/')
            temp[j] = CHAR_PATH_SEPARATOR;
      }
    }
    IAlloc_Free(&allocImp, outBuffer);
  }
  SzArEx_Free(&db, &allocImp);

  File_Close(&archiveStream.file);

  if (res == SZ_OK)
  {
    HANDLE hProcess = 0;
    if (useShellExecute)
    {
      SHELLEXECUTEINFO ei;
      UINT32 executeRes;
      BOOL success;
      
      memset(&ei, 0, sizeof(ei));
      ei.cbSize = sizeof(ei);
      ei.lpFile = path;
      ei.fMask = SEE_MASK_NOCLOSEPROCESS
          #ifndef UNDER_CE
          | SEE_MASK_FLAG_DDEWAIT
          #endif
          /* | SEE_MASK_NO_CONSOLE */
          ;
      if (wcslen(cmdLineParams) != 0)
        ei.lpParameters = cmdLineParams;
      ei.nShow = SW_SHOWNORMAL; /* SW_HIDE; */
      success = ShellExecuteEx(&ei);
      executeRes = (UINT32)(UINT_PTR)ei.hInstApp;
      if (!success || (executeRes <= 32 && executeRes != 0))  /* executeRes = 0 in Windows CE */
        res = SZ_ERROR_FAIL;
      else
        hProcess = ei.hProcess;
    }
    else
    {
      STARTUPINFOW si;
      PROCESS_INFORMATION pi;
      WCHAR cmdLine[MAX_PATH * 3];

      wcscpy(cmdLine, path);
      wcscat(cmdLine, cmdLineParams);
      memset(&si, 0, sizeof(si));
      si.cb = sizeof(si);
      if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0)
        res = SZ_ERROR_FAIL;
      else
      {
        CloseHandle(pi.hThread);
        hProcess = pi.hProcess;
      }
    }
    if (hProcess != 0)
    {
      WaitForSingleObject(hProcess, INFINITE);
      CloseHandle(hProcess);
    }
  }

  path[pathLen] = L'\0';
  RemoveDirWithSubItems(path);

  if (res == SZ_OK)
    return 0;
  
  {
    if (res == SZ_ERROR_UNSUPPORTED)
      errorMessage = "Decoder doesn't support this archive";
    else if (res == SZ_ERROR_MEM)
      errorMessage = "Can't allocate required memory";
    else if (res == SZ_ERROR_CRC)
      errorMessage = "CRC error";
    else
    {
      if (!errorMessage)
        errorMessage = "ERROR";
    }
    if (errorMessage)
      PrintErrorMessage(errorMessage);
  }
  return 1;
}
