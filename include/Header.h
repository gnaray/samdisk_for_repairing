#pragma once

#include "DiskConstants.h"
#include "utils.h"

#include <cassert>
#include <string>

enum class DataRate : int { Unknown = 0, _250K = 250'000, _300K = 300'000, _500K = 500'000, _1M = 1'000'000 };
enum class Encoding { Unknown, MFM, FM, RX02, Amiga, GCR, Ace, MX, Agat, Apple, Victor, Vista };

std::string to_string(const DataRate& datarate);
std::string to_string(const Encoding& encoding);
std::string short_name(const Encoding& encoding);
DataRate datarate_from_string(std::string str);
Encoding encoding_from_string(std::string str);

inline int bitcell_ns(DataRate datarate)
{
    switch (datarate)
    {
    case DataRate::Unknown: break;
    case DataRate::_250K:   return 2000;
    case DataRate::_300K:   return 1667;
    case DataRate::_500K:   return 1000;
    case DataRate::_1M:     return 500;
    }

    return 0;
}

inline int bits_per_second(DataRate datarate)
{
    return static_cast<int>(datarate);
}

inline std::ostream& operator<<(std::ostream& os, const DataRate dr) { return os << to_string(dr); }
inline std::ostream& operator<<(std::ostream& os, const Encoding e) { return os << to_string(e); }



struct CylHead
{
    CylHead() = default;
    CylHead(int cyl_, int head_) : cyl(cyl_), head(head_)
    {
        assert(cyl >= 0 && cyl < MAX_DISK_CYLS);
        assert(head >= 0 && head < MAX_DISK_HEADS);
    }

    operator int() const;

    std::string to_string() const
    {
#if 0   // ToDo
        if (opt_hex == 1) // TODO: former opt.hex is now opt_hex and not available, because Options.h should be included which includes Header.h, cyclic dependency.
            return util::format("cyl %02X head %u", cyl, head);
#endif

        return util::fmt("cyl %u head %u", cyl, head);
    }

    CylHead next_cyl()
    {
        CylHead cylhead(*this);
        ++cyl;
        assert(cyl < MAX_DISK_CYLS);
        return cylhead;
    }

    int cyl = -1, head = -1;
};

CylHead operator * (const CylHead& cylhead, int cyl_step);
inline std::ostream& operator<<(std::ostream& os, const CylHead& cylhead) { return os << cylhead.to_string(); }


class Header
{
public:
    Header() = default;
    Header(int cyl, int head, int sector, int size);
    Header(const CylHead& cylhead, int sector, int size);

    bool operator== (const Header& rhs) const;
    bool operator!= (const Header& rhs) const;
    operator CylHead() const;

    int sector_size() const;
    bool compare_chrn(const Header& rhs) const;
    bool compare_crn(const Header& rhs) const;

    int cyl = 0, head = 0, sector = 0, size = 0;
};

class Headers : public std::vector<Header>
{
public:
    Headers() = default;

    bool contains(const Header& header) const;
    std::string to_string() const;
    std::string sector_ids_to_string() const;
    bool has_id_sequence(const int first_id, const int up_to_id) const;
};
