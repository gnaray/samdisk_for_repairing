// Quazar Trinity (ethernet adapter) helper functions

#include "Trinity.h"
#include "HDD.h"
#include "utils.h"
#include "Format.h"

#ifdef _WIN32
#include "Util.h"
#endif

#include <algorithm>

std::unique_ptr<Trinity> Trinity::Open()
{
    std::unique_ptr<Trinity> trinity;
    trinity.reset(new Trinity());
    return trinity;
}

// Get a list of TrinLoad IP addresses using a network broadcast
Trinity::Trinity()
{
#ifdef _WIN32
    // Winsock2 is delay-loaded, just in case it's not present
    if (!CheckLibrary("winsock2", "WSAStartup"))
        throw util::exception("winsock2 (ws2_32.dll) is required for network access");

    WSADATA wsadata = {};
    if (WSAStartup(WINSOCK_VERSION, &wsadata) != 0)
        throw util::exception("winsock2 initialisation failed");
#endif

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (static_cast<int>(m_socket) == -1)
        throw util::exception("failed to bind to local UDP port");

    // Enable broadcast and re-use of closing sockets
    int enable = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&enable), sizeof(enable));
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enable), sizeof(enable));

    // Allow 250ms for responses to arrive
    int timeout = 250;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    // TODO: determine broadcast addresses for all network interfaces.
    // For now try 255.255.255.255, 192.168.0.255, 10.0.0.255 (usually treated as Class C).
    VectorX<uint32_t> broadcast_addrs{ INADDR_BROADCAST, 0xc0a800ff, 0x0a0000ff };

    for (auto& broadcast_addr : broadcast_addrs)
    {
        m_addr_to.sin_family = AF_INET;
        m_addr_to.sin_port = htons(TRINLOAD_UDP_PORT);
        m_addr_to.sin_addr.s_addr = htonl(broadcast_addr);

        // Send a single "?" to locate TrinLoad devices
        Send("?", 1);

        while (m_devices.empty())
        {
            try
            {
                char ab[1024];
                int len = Recv(ab, sizeof(ab));
                if (len >= 1 && ab[0] == '!')
                    m_devices.push_back(inet_ntoa(static_cast<in_addr>(m_addr_from.sin_addr)));
            }
            catch (...)
            {
                break;
            }
        }

        if (!m_devices.empty())
            break;
    }

    if (m_devices.empty())
        throw util::exception("no TrinLoad devices found");

    // Select the first device
    m_addr_to.sin_addr.s_addr = inet_addr(m_devices[0].c_str());
}

Trinity::~Trinity()
{
    if (m_socket != -1)
    {
        cls();
        closesocket(m_socket);
    }
}


const VectorX<std::string> Trinity::devices() const
{
    return m_devices;
}


int Trinity::Send(const void* pv, int len)
{
    assert(len > 0);

    auto addr_to = reinterpret_cast<const sockaddr*>(&m_addr_to);

    auto ret = sendto(m_socket, reinterpret_cast<const char*>(pv), static_cast<size_t>(len), 0, addr_to, sizeof(m_addr_to));
    return std::max(static_cast<int>(ret), 0);
}

int Trinity::Recv(void* pv, int len)
{
    assert(len > 0);

    socklen_t addr_from_len = sizeof(m_addr_from);
    auto addr_from = reinterpret_cast<sockaddr*>(&m_addr_from);
    auto pch = reinterpret_cast<char*>(pv);

    auto ret = recvfrom(m_socket, pch, static_cast<size_t>(len), 0, addr_from, &addr_from_len);
    if (ret < 0)
        throw util::exception("network timeout waiting for response");

    return lossless_static_cast<int>(ret);
}


void Trinity::cls()
{
    uint8_t cmd[1];
    cmd[0] = 'C';

    Send(cmd, sizeof(cmd));
}

void Trinity::select_record(int record)
{
    uint8_t cmd[3];
    cmd[0] = 'R';
    cmd[1] = static_cast<uint8_t>(record);
    cmd[2] = static_cast<uint8_t>(record >> 8);

    Send(cmd, sizeof(cmd));

    uint8_t ab[3];
    Recv(ab, sizeof(ab));
}

Data Trinity::read_sector(int cyl, int head, int sector)
{
    // Form an execute command for the execution address
    uint8_t cmd[3];
    cmd[0] = 'S';
    cmd[1] = static_cast<uint8_t>(sector);
    cmd[2] = static_cast<uint8_t>(((head & 1) << 7) | (cyl & 0x7f));

    // Send the sector read request
    Send(cmd, sizeof(cmd));

    Data data;
    while (data.size() != SECTOR_SIZE)
    {
        uint8_t ab[1472];
        auto len = Recv(ab, sizeof(ab));
        data.insert(data.end(), ab + sizeof(cmd), ab + len);
    }

    return data;
}

Data Trinity::read_track(int cyl, int head)
{
    Format fmt{ RegularFormat::MGT };

    // Form an execute command for the execution address
    uint8_t cmd[2];
    cmd[0] = 'T';
    cmd[1] = static_cast<uint8_t>(((head & 1) << 7) | (cyl & 0x7f));

    // Send the sector read request
    Send(cmd, sizeof(cmd));

    Data data;
    data.reserve(fmt.track_size());

    while (static_cast<int>(data.size()) != fmt.track_size())
    {
        uint8_t ab[1472];
        auto len = Recv(ab, sizeof(ab));
        data.insert(data.end(), ab + sizeof(cmd), ab + len);
    }

    return data;
}

void Trinity::send_file(const void* pv_, int len, int start_addr, int exec_addr)
{
    char ab[1024];

    auto data_offset = 0;
    auto page = static_cast<uint8_t>((start_addr - 16384) / 16384);
    auto offset = start_addr & 0x3fff;

    // Loop until we've sent all the data
    while (data_offset < len)
    {
        // Calculate the data size to send, which is capped at 1468 bytes:
        //  1500 Ethernet size - IP header (20) - UDP header (8) - our header (4)
        auto chunk = std::min(len - data_offset, (1500 - 20 - 8 - 4));
        const char* pb = reinterpret_cast<const char*>(pv_) + data_offset;

        Data cmd;
        cmd.reserve(4 + chunk);

        // Form a data command, with page number and offset to write it at
        cmd.push_back('@');
        cmd.push_back(page);
        cmd.push_back(static_cast<uint8_t>(offset));
        cmd.push_back(static_cast<uint8_t>(offset >> 8));
        cmd.insert(cmd.end(), pb, pb + chunk);

        // Send the data packet
        Send(cmd.data(), cmd.size());

        // Expect a response to acknowledge the received data
        Recv(ab, sizeof(ab));

        // Advance past the chunk we've just sent, scrolling the paging window if we're beyond 16K
        data_offset += chunk;
        offset += chunk;
        if (offset >= 0x4000)
        {
            offset &= 0x3fff;
            ++page;
        }
    }

    // Form an execute command for the execution address
    uint8_t cmd[4];
    cmd[0] = 'X';
    cmd[1] = (exec_addr < 0x10000) ? 1 : static_cast<uint8_t>((exec_addr - 16384) / 16384);
    cmd[2] = (exec_addr & 0x00ff);
    cmd[3] = (exec_addr < 0x10000) ? static_cast<uint8_t>(exec_addr >> 8) : (exec_addr & 0x3fff) >> 8;

    // Send the execute command
    Send(cmd, sizeof(cmd));

    // Expect a short response to acknowledge the execute
    Recv(ab, sizeof(ab));
}
