#include "Options.h"
#include "CRC16.h"
#include "DiskUtil.h"
#include "Sector.h"

#include <cstring>

static auto& opt_fill = getOpt<int>("fill");
static auto& opt_maxcopies = getOpt<int>("maxcopies");

Sector::Sector(DataRate datarate_, Encoding encoding_, const Header& header_, int gap3_)
    : header(header_), datarate(datarate_), encoding(encoding_), gap3(gap3_)
{
}

bool Sector::operator== (const Sector& sector) const
{
    // Headers must match
    if (sector.header != header)
        return false;

    // If neither has data it's a match
    if (sector.m_data.size() == 0 && m_data.size() == 0)
        return true;

    // Both sectors must have some data
    if (sector.copies() == 0 || copies() == 0)
        return false;

    // Both first sectors must have at least the natural size to compare
    if (sector.data_size() < sector.size() || data_size() < size())
        return false;

    // The natural data contents must match
    return std::equal(data_copy().begin(), data_copy().begin() + size(), sector.data_copy().begin());
}

int Sector::size() const
{
    return header.sector_size();
}

int Sector::data_size() const
{
    return copies() ? static_cast<int>(m_data[0].size()) : 0;
}

const DataList& Sector::datas() const
{
    return m_data;
}

DataList& Sector::datas()
{
    return m_data;
}

const Data& Sector::data_copy(int copy/*=0*/) const
{
    copy = std::max(std::min(copy, static_cast<int>(m_data.size()) - 1), 0);
    return m_data[copy];
}

Data& Sector::data_copy(int copy/*=0*/)
{
    assert(m_data.size() != 0);
    copy = std::max(std::min(copy, static_cast<int>(m_data.size()) - 1), 0);
    return m_data[copy];
}

int Sector::get_best_data_index() const
{
    if (copies() == 0)
        return -1;
    return 0;
}

bool Sector::has_stable_data() const
{
    const auto best_data_index = get_best_data_index();
    if (best_data_index < 0)
        return false;
    const auto bad_crc = (!opt.normal_disk && is_checksummable_8k_sector()) ? false : has_baddatacrc();
    return !bad_crc;
}

int Sector::read_attempts() const
int Sector::copies() const
{
    return static_cast<int>(m_data.size());
}

Sector::Merge Sector::add(Data&& new_data, bool bad_crc, uint8_t new_dam)
{
    Merge ret = Merge::NewData;

    // If the sector has a bad header CRC, it can't have any data
    if (has_badidcrc())
        return Merge::Unchanged;

#ifdef _DEBUG
    // If there's enough data, check the CRC
    if ((encoding == Encoding::MFM || encoding == Encoding::FM) &&
        static_cast<int>(new_data.size()) >= (size() + 2))
    {
        CRC16 crc;
        if (encoding == Encoding::MFM) crc.init(CRC16::A1A1A1);
        crc.add(new_dam);
        auto bad_data_crc = crc.add(new_data.data(), size() + 2) != 0;
        if (bad_crc != bad_data_crc)
             util::cout << std::boolalpha << "Debug assert failed: New sector data has " << bad_crc
                << " CRC and shortening it to expected sector size it has " << bad_data_crc << " CRC\n";
    }
#endif

    // If the exising sector has good data, ignore supplied data if it's bad
    if (bad_crc && has_good_data())
        return Merge::Unchanged;

    // If the existing sector is bad, new good data will replace it all
    if (!bad_crc && has_baddatacrc())
    {
        remove_data();
        ret = Merge::Improved;
    }

    // 8K sectors always have a CRC error, but may include a secondary checksum
    if (is_8k_sector())
    {
        // Attempt to identify the 8K checksum method used by the new data
        // If it's recognised, replace any existing data with it
        if (!ChecksumMethods(new_data.data(), new_data.size()).empty())
        {
            remove_data();
            ret = Merge::Improved;
        }
        // Do we already have a copy?
        else if (copies() == 1)
        {
            // Can we identify the method used by the existing copy?
            if (!ChecksumMethods(m_data[0].data(), m_data[0].size()).empty())
            {
                // Keep the existing, ignoring the new data
                return Merge::Unchanged;
            }
        }
    }

    // DD 8K sectors are considered complete at 6K, everything else at natural size
    auto complete_size = is_8k_sector() ? 0x1800 : new_data.size();

    // Compare existing data with the new data, to avoid storing redundant copies.
    // The goal is keeping only 1 optimal sized data amongst those having matching content.
    // Optimal sized: complete size else smallest above complete size else biggest below complete size.
    const auto i_sup = static_cast<int>(m_data.size());
    for (auto i = 0; i < i_sup; i++)
    {
        const auto& data = m_data[i];
        const auto common_size = std::min({ data.size(), new_data.size(), complete_size });
        if (std::equal(data.begin(), data.begin() + common_size, new_data.begin()))
        {
            if (data.size() == new_data.size())
            {
                return Merge::Unchanged;
            }
            if (new_data.size() < data.size())
            {
                if (new_data.size() < complete_size)
                {
                    return Merge::Unchanged;
                }
                // The new shorter complete copy replaces the existing data.
                erase_data(i--);
                ret = Merge::Improved;
                break; // Not continuing. See the goal above.
            }
            else
            {
                if (data.size() >= complete_size)
                {
                    return Merge::Unchanged;
                }
                // The new longer complete copy replaces the existing data.
                erase_data(i--);
                ret = Merge::Improved;
                break; // Not continuing. See the goal above.
            }
        }
    }

    // Will we now have multiple copies?
    if (copies() > 0)
    {
        // Damage can cause us to see different DAM values for a sector.
        // Favour normal over deleted, and deleted over anything else.
        if (dam != new_dam &&
            (dam == 0xfb || (dam == 0xf8 && new_dam != 0xfb)))
        {
            return Merge::Unchanged;
        }

        // Multiple good copies mean a difference in the gap data after
        // a good sector, perhaps due to a splice. We just ignore it.
        if (!has_baddatacrc())
            return Merge::Unchanged;

        // Keep multiple copies the same size, whichever is shortest
        auto new_size = std::min(new_data.size(), m_data[0].size());
        new_data.resize(new_size);

        // Resize any existing copies to match
        for (auto& d : m_data)
            d.resize(new_size);
    }

    // Insert the new data copy.
    int copies = m_data.size();
    m_data.emplace_back(std::move(new_data));
    limit_copies(opt.maxcopies);
    // If copies amount is the same then the added new data is dismissed so return unchanged.
    if (copies == m_data.size())
        return Merge::Unchanged;

    // Update the data CRC state and DAM
    m_bad_data_crc = bad_crc;
    dam = new_dam;

    return ret;
}

Sector::Merge Sector::merge(Sector&& sector)
{
    Merge ret = Merge::Unchanged;

    // If the new header CRC is bad there's nothing we can use
    if (sector.has_badidcrc())
        return Merge::Unchanged;

    // Something is wrong if the new details don't match the existing one
    assert(sector.header == header);
    assert(sector.datarate == datarate);
    assert(sector.encoding == encoding);

    // If the existing header is bad, repair it
    if (has_badidcrc())
    {
        header = sector.header;
        set_badidcrc(false);
        ret = Merge::Improved;
    }

    // We can't repair good data with bad
    if (!has_baddatacrc() && sector.has_baddatacrc())
        return ret;

    // Add the new data snapshots
    for (Data& data : sector.m_data)
    {
        // Move the data into place, passing on the existing data CRC status and DAM
        auto add_ret = add(std::move(data), sector.has_baddatacrc(), sector.dam);
        if (add_ret == Merge::Improved || add_ret == Merge::NewData)
            ret = add_ret;
    }
    sector.m_data.clear();

    return ret;
}


bool Sector::has_data() const
{
    return copies() != 0;
}

bool Sector::has_good_data() const
{
    return has_data() && !has_baddatacrc() && !has_gapdata();
}

bool Sector::has_gapdata() const
{
    return data_size() > size();
}

bool Sector::has_shortdata() const
{
    return data_size() < size();
}

bool Sector::has_normaldata() const
{
    return has_data() && data_size() == size();
}

bool Sector::has_badidcrc() const
{
    return m_bad_id_crc;
}

bool Sector::has_baddatacrc() const
{
    return m_bad_data_crc;
}

bool Sector::is_deleted() const
{
    return dam == 0xf8 || dam == 0xf9;
}

bool Sector::is_altdam() const
{
    return dam == 0xfa;
}

bool Sector::is_rx02dam() const
{
    return dam == 0xfd;
}

bool Sector::is_8k_sector() const
{
    // +3 and CPC disks treat this as a virtual complete sector
    return datarate == DataRate::_250K && encoding == Encoding::MFM &&
        header.size == 6 && has_data();
}

bool Sector::is_checksummable_8k_sector() const
{
    if (is_8k_sector() && has_data())
    {
        const Data& data = data_copy();
        if (!ChecksumMethods(data.data(), data.size()).empty())
            return true;
    }
    return false;
}

void Sector::set_badidcrc(bool bad)
{
    m_bad_id_crc = bad;

    if (bad)
        remove_data();
}

void Sector::set_baddatacrc(bool bad)
{
    m_bad_data_crc = bad;

    if (!bad)
    {
        auto fill_byte = static_cast<uint8_t>((opt_fill >= 0) ? opt_fill : 0);

        if (!has_data())
            m_data.push_back(Data(size(), fill_byte));
        else if (copies() > 1)
        {
            m_data.resize(1);

            if (data_size() < size())
            {
                auto pad{ Data(size() - data_size(), fill_byte) };
                m_data[0].insert(m_data[0].end(), pad.begin(), pad.end());
            }
        }
    }
}

void Sector::erase_data(int instance)
{
    m_data.erase(m_data.begin() + instance);
}

void Sector::resize_data(int count)
{
    m_data.resize(count);
}

void Sector::remove_data()
{
    m_data.clear();
    m_bad_data_crc = false;
    dam = 0xfb;
}

void Sector::limit_copies(int max_copies)
{
    if (copies() > max_copies)
        m_data.resize(max_copies);
}

void Sector::remove_gapdata(bool keep_crc/*=false*/)
{
    if (!has_gapdata())
        return;

    for (auto& data : m_data)
    {
        // If requested, attempt to preserve CRC bytes on bad sectors.
        if (keep_crc && has_baddatacrc() && data.size() >= (size() + 2))
            data.resize(size() + 2);
        else
            data.resize(size());
    }
}

// Map a size code to how it's treated by the uPD765 FDC on the PC
int Sector::SizeCodeToRealSizeCode(int size)
{
    // Sizes above 8 are treated as 8 (32K)
    return (size <= 7) ? size : 8;
}

// Return the sector length for a given sector size code
int Sector::SizeCodeToLength(int size)
{
    // 2 ^ (7 + size)
    return 128 << SizeCodeToRealSizeCode(size);
}

//////////////////////////////////////////////////////////////////////////////

bool Sectors::has_id_sequence(const int first_id, const int up_to_id) const
{
    return this->headers().has_id_sequence(first_id, up_to_id);
}

Headers Sectors::headers() const
{
    Headers headers;
    for_each(begin(), end(), [&](const Sector& sector) {
        headers.push_back(sector.header);
    });
    return headers;
}
