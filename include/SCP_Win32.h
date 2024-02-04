#pragma once

#ifdef _WIN32

#include "Platform.h"

#include "SuperCardPro.h"

class SuperCardProWin32 final : public SuperCardPro
{
public:
    ~SuperCardProWin32();

    static std::unique_ptr<SuperCardPro> Open();
    static std::string GetDevicePath();

private:
    explicit SuperCardProWin32(HANDLE hdev);

    bool Read(void* p, int len, int* bytes_read) override;
    bool Write(const void* p, int len, int* bytes_written) override;

    HANDLE m_hdev;
    DWORD m_dwError;
};

#endif // _WIN32
