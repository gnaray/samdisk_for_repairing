// FTDI backend for SuperCard Pro device

#include "SAMdisk.h"
#include "SCP_FTDI.h"

#ifdef HAVE_FTD2XX

/*static*/ std::unique_ptr<SuperCardPro> SuperCardProFTDI::Open ()
{
	FT_HANDLE hdev;
	auto serial = static_cast<const void *>("SCP-JIM");

	if (!CheckLibrary("ftdi2", "FT_OpenEx"))
		return nullptr;

	FT_STATUS status = FT_OpenEx(const_cast<PVOID>(serial), FT_OPEN_BY_SERIAL_NUMBER, &hdev);
	if (status != FT_OK)
		return nullptr;

	return std::unique_ptr<SuperCardPro>(new SuperCardProFTDI(hdev));
}

SuperCardProFTDI::SuperCardProFTDI (FT_HANDLE hdev)
	: m_hdev(hdev), m_status(FT_OK)
{
}

SuperCardProFTDI::~SuperCardProFTDI ()
{
	FT_Close(m_hdev);
}

bool SuperCardProFTDI::Read (void *buf, int len, int *bytes_read)
{
	DWORD dwBytesToRead = static_cast<DWORD>(len), dwBytesReturned = 0;
	m_status = FT_Read(m_hdev, buf, dwBytesToRead, &dwBytesReturned);
	if (m_status != FT_OK)
		return false;

	*bytes_read = static_cast<int>(dwBytesReturned);
	return true;
}

bool SuperCardProFTDI::Write (const void *buf, int len, int *bytes_written)
{
	DWORD dwBytesToWrite = static_cast<DWORD>(len), dwBytesWritten = 0;
	m_status = FT_Write(m_hdev, const_cast<LPVOID>(buf), dwBytesToWrite, &dwBytesWritten);
	if (m_status != FT_OK)
		return false;

	*bytes_written = static_cast<int>(dwBytesWritten);
	return true;
}

#endif // HAVE_FTD2XX
