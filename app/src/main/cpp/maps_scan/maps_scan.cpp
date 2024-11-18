#include "maps_scan.hpp"

#include <sys/mman.h>
#include <sys/sysmacros.h>

#include <cinttypes>
#include <list>
#include <map>
#include <vector>

namespace maps_scan {

    [[maybe_unused]] void MapInfo::ForEach(const Callback &callback, std::string_view pid) {
        constexpr static auto kPermLength = 5;
        constexpr static auto kMapEntry = 7;
        auto path = "/proc/" + std::string{pid} + "/maps";
        auto maps = std::unique_ptr<FILE, decltype(&fclose)>{fopen(path.c_str(), "r"), &fclose};
        if (maps) {
            char *line = nullptr;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, maps.get())) > 0) {
                line[read - 1] = '\0';
                uintptr_t start = 0;
                uintptr_t end = 0;
                uintptr_t off = 0;
                ino_t inode = 0;
                unsigned int dev_major = 0;
                unsigned int dev_minor = 0;
                std::array<char, kPermLength> perm{'\0'};
                int path_off;
                if (sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %4s %" PRIxPTR " %x:%x %lu %n%*s", &start,
                           &end, perm.data(), &off, &dev_major, &dev_minor, &inode,
                           &path_off) != kMapEntry) {
                    continue;
                }
                while (path_off < read && isspace(line[path_off])) path_off++;
                uint8_t perms = 0;
                if (perm[0] == 'r') perms |= PROT_READ;
                if (perm[1] == 'w') perms |= PROT_WRITE;
                if (perm[2] == 'x') perms |= PROT_EXEC;
                MapInfo mi{start, end, perms, perm[3] == 'p', off,
                           static_cast<dev_t>(makedev(dev_major, dev_minor)),
                           inode, line + path_off};

                if (!callback(mi)) break;
            }
            free(line);
        }
    }

    [[maybe_unused]] std::vector<MapInfo> MapInfo::Scan(std::string_view pid, std::optional<const Filter> filter) {
        std::vector<MapInfo> info;

        ForEach([&](const auto &map) -> auto {
            if (!filter.has_value() || filter->operator()(map))
                info.emplace_back(map);
            return true;
        }, pid);

        return info;
    }
}
