// USB backend for SuperCard Pro device

#ifdef _WIN32
#include "PlatformConfig.h" // For disabling open, read, write, close deprecation.
#else
#include "config.h"
#endif
#include "SCP_USB.h"
#include "FileIO.h" // O_BINARY

#include <fcntl.h>
#include <string>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef _WIN32
#include "SCP_Win32.h"
#endif


/*static*/ std::unique_ptr<SuperCardPro> SuperCardProUSB::Open()
{
    std::string path;
#ifdef _WIN32
    path = SuperCardProWin32::GetDevicePath();
#elif defined(__linux__)
    path = "/dev/ttyUSB0";
#elif defined(__APPLE__)
    path = "/dev/cu.usbserial-SCP-JIM";
#endif

    if (!path.empty())
    {
        int fd = open(path.c_str(), O_RDWR | O_BINARY);
        if (fd > 0)
        {
#ifdef _WIN32
            DCB dcb = { sizeof(dcb) };
            if (BuildCommDCB("baud=9600 parity=N data=8 stop=1", &dcb))
            {
                HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
                SetCommState(h, &dcb);
            }
#endif
            return std::unique_ptr<SuperCardPro>(new SuperCardProUSB(fd));
        }
    }
    return std::unique_ptr<SuperCardPro>();
}


SuperCardProUSB::SuperCardProUSB(int fd)
    : m_fd(fd), m_error(0)
{
}

SuperCardProUSB::~SuperCardProUSB() /*override*/
{
    close(m_fd);
}

bool SuperCardProUSB::Read(void* p, int len, int* bytes_read)
{
    for (;;)
    {
        auto bytes = read(m_fd, p, len);
        if (bytes >= 0)
        {
            *bytes_read = bytes;
            return true;
        }

        if (errno != EAGAIN && errno != EINTR)
        {
            m_error = errno;
            return false;
        }
    }
}

bool SuperCardProUSB::Write(const void* p, int len, int* bytes_written)
{
    for (;;)
    {
        auto bytes = write(m_fd, p, len);
        if (bytes >= 0)
        {
            *bytes_written = bytes;
            return true;
        }

        if (errno != EAGAIN && errno != EINTR)
        {
            m_error = errno;
            return false;
        }
    }
}
