// RPM command

#include "Disk.h"
#include "Image.h"
#include "SAMdisk.h"
#include <iomanip>
#include <memory>


bool DiskRpm(const std::string& path)
{
    auto disk = std::make_shared<Disk>();
    if (!ReadImage(path, disk))
        return false;

    // Default to using cyl 0 head 0, but allow the user to override it
    CylHead cylhead(opt.range.empty() ? 0 :
        opt.range.cyl_end + 1, opt.range.head_end);

    auto forever = opt.force && util::is_stdout_a_tty();
    opt.retries = opt.rescans = 0;

    // Display 5 revolutions, or run forever if forced
    for (auto i = 0; forever || i < 5; ++i)
    {
        auto& track = disk->read_track(cylhead, true);

        if (!track.tracktime)
        {
            if (i == 0)
                throw util::exception("not available for this disk type");

            break;
        }

        auto time_us = track.tracktime;
        auto rpm = 60'000'000.0f / track.tracktime;

        std::stringstream ss;
        ss << std::setw(6) << time_us << " = " <<
            std::setprecision(2) << std::fixed << rpm << " rpm";

        if (forever)
            util::cout << "\r" << ss.str() << "  (Ctrl-C to stop)";
        else
            util::cout << ss.str() << "\n";

        util::cout.screen->flush();
    }

    return true;
}
