#pragma once

#include "TrackSectorIds.h"
#include "Header.h"
#include "IBMPCBase.h"
#include "Interval.h"
#include "VectorX.h"

//////////////////////////////////////////////////////////////////////////////

using DataList = VectorX<Data>;

//////////////////////////////////////////////////////////////////////////////

class DataReadStats
{
public:
    DataReadStats() = default;
    DataReadStats(int read_count)
        : m_read_count(read_count)
    {
    }

    int ReadCount() const
    {
        return m_read_count;
    }

    DataReadStats& operator +=(const DataReadStats& rhs)
    {
        m_read_count += rhs.m_read_count;
        return *this;
    }

    DataReadStats operator +(const DataReadStats& rhs) const
    {
        DataReadStats result = *this;
        return result += rhs;
    }

private:
    int m_read_count = 0; // Amount of reading (good or bad) data of the owner sector (provided only by not constant (real) disks).
};
typedef VectorX<DataReadStats> DataReadStatsList;

//////////////////////////////////////////////////////////////////////////////

class Sector
{
public:
    enum class Merge { Unchanged, Matched, Improved, NewData };

private:
    void process_merge_result(const Merge& ret, int new_read_attempts, const DataReadStats& new_data_read_stats,
        bool readstats_counter_mode, int affected_data_index, const DataReadStats& improved_data_read_stats);

public:
    Sector(DataRate datarate, Encoding encoding, const Header& header = Header(), int gap3 = 0);
    Sector CopyWithoutData(bool keepReadAttempts = true) const;
    bool operator==(const Sector& sector) const;

    Merge merge(Sector&& sector);

    bool has_data() const;
    bool has_good_data(bool consider_checksummable_8K = false, bool consider_normal_disk = false) const;
    bool has_gapdata() const;
    bool has_shortdata() const;
    bool has_normaldata() const;
    bool has_good_normaldata() const;
    inline bool has_badidcrc() const
    {
        return m_bad_id_crc;
    }

    inline bool has_baddatacrc() const
    {
        return m_bad_data_crc;
    }

    inline bool is_deleted() const
    {
        return dam == IBM_DAM_DELETED || dam == IBM_DAM_DELETED_ALT;
    }

    inline bool is_altdam() const
    {
        return dam == IBM_DAM_ALT;
    }

    inline bool is_rx02dam() const
    {
        return dam == IBM_DAM_RX02;
    }

    bool is_8k_sector() const;
    bool is_checksummable_8k_sector() const;

    void set_badidcrc(bool bad = true);
    void set_baddatacrc(bool bad = true);
    void erase_data(int instance);
    void resize_data(int count);
    void remove_data();
    void remove_gapdata(bool keep_crc = false);
    bool has_stable_data() const;
    bool are_copies_full(int max_copies) const;
    void limit_copies(int max_copies);
    bool is_sector_tolerated_same(const Sector& sector, const int byte_tolerance_of_time, const int tracklen) const;
    void normalise_datarate(const DataRate& datarate_target);
    bool has_same_record_properties(const Sector& other_sector, const int other_tracklen) const;

    int size() const;
    int data_size() const;

    const DataList& datas() const;
    const Data& data_copy(int copy = 0) const;
    Data& data_copy(int copy = 0);
    const Data& data_best_copy() const;
    Data& data_best_copy();
    const DataReadStats& data_copy_read_stats(int instance = 0) const;
    DataReadStats& data_copy_read_stats(int instance = 0);
    const DataReadStats& data_best_copy_read_stats() const;
    DataReadStats& data_best_copy_read_stats();
    int get_data_best_copy_index() const;
    int read_attempts() const;
    void set_read_attempts(int read_attempts);
    void add_read_attempts(int read_attempts);
    bool is_constant_disk() const;
    void set_constant_disk(bool constant_disk);
    void fix_readstats();

    Merge add(Data&& data, bool bad_crc = false, uint8_t dam = IBM_DAM, int* affected_data_index = nullptr, DataReadStats* improved_data_read_stats = nullptr);
    Merge add_with_readstats(Data&& new_data, bool new_bad_crc = false, uint8_t new_dam = IBM_DAM,
    void assign(Data&& data);
        int new_read_attempts = 1, const DataReadStats& new_data_read_stats = DataReadStats(1), bool readstats_counter_mode = true, bool update_this_read_attempts = true);
    int copies() const;
    void add_read_stats(int instance, DataReadStats&& data_read_stats);
    void set_read_stats(int instance, DataReadStats&& data_read_stats);

    // Map a size code to how it's treated by the uPD765 FDC on the PC
    static constexpr int SizeCodeToRealSizeCode(int size)
    {
        // Sizes above 8 are treated as 8 (32K)
        return (size <= 7) ? size : 8;
    }

    // Return the sector length for a given sector size code
    static constexpr int SizeCodeToLength(int size)
    {
        // 2 ^ (7 + size)
        return 128 << SizeCodeToRealSizeCode(size);
    }

    // Return the sector length for a given sector size code as treated by the uPD765 FDC on the PC.
    static constexpr int SizeCodeToRealLength(int size)
    {
        return SizeCodeToLength(SizeCodeToRealSizeCode(size));
    }

    int FindParentSectorIdByOffset(const IdAndOffsetVector& sectorIdsAndOffsets) const;

    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const Sector& sector, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << sector.ToString(onlyRelevantData);
        return ss.str();
    }

public:
    Header header{ 0,0,0,0 };               // cyl, head, sector, size
    DataRate datarate = DataRate::Unknown;  // 250Kbps
    Encoding encoding = Encoding::Unknown;  // MFM
    int offset = 0;                         // bitstream offset from index, in bits
    int revolution = 0;                     // the nth floppy disk spin when this sector was read (i.e. multioffset / tracklen), usually 0. Currently not saved in RDSK.
    int gap3 = 0;                           // inter-sector gap size
    uint8_t dam = IBM_DAM;                  // data address mark

private:
    bool m_bad_id_crc = false;
    bool m_bad_data_crc = false;
    DataList m_data{};         // copies of sector data
    DataReadStatsList m_data_read_stats{}; // Readstats of copies of sector data.
    int m_read_attempts = 0; // Amount of reading data attempts of this sector (provided only by real disks).
    bool m_constant_disk = true; // If this sector is part of disk image then true, else it comes from physical device so false.
};

inline std::ostream& operator<<(std::ostream& os, const Sector& sector) { return os << to_string(sector); }

//////////////////////////////////////////////////////////////////////////////

class Sectors : public VectorX<Sector>
{
public:
    using VectorX<Sector>::VectorX;
    using VectorX<Sector>::push_back;

    void push_back(const Sectors& sectors);
    bool HasIdSequence(const int first_id, const int length) const;
    const std::set<int> NotContainedIds(const Interval<int> &id_interval) const;
    class Headers GoodHeaders() const;

    bool Contains(const Sector& other_sector, const int other_tracklen) const;
    std::string SectorIdsToString() const;
    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const Sectors& sectors, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << sectors.ToString(onlyRelevantData);
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const Sectors& sectors) { return os << to_string(sectors); }

//////////////////////////////////////////////////////////////////////////////

// A lhs sector is less than a rhs sector if its header is less or if the headers are same and its offset is less.
struct SectorPreciseLess
{
    constexpr bool operator()(const Sector& lhs, const Sector& rhs) const
    {
        return lhs.header < rhs.header || (!(lhs.header > rhs.header) && lhs.offset < rhs.offset);
    }
};

// Unique sectors based on the SectorPreciseLess method.
class UniqueSectors : public std::set<Sector, SectorPreciseLess>
{
public:
    using std::set<Sector, SectorPreciseLess>::set;

    const UniqueSectors StableSectors() const;
    bool Contains(const Sector& other_sector, const int other_tracklen) const;
    bool AnyIdsNotContainedInThis(const Interval<int>& id_interval) const;
    std::string SectorIdsToString() const;
    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const UniqueSectors& sectors, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << sectors.ToString(onlyRelevantData);
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const UniqueSectors& sectors) { return os << to_string(sectors); }

//////////////////////////////////////////////////////////////////////////////
