#pragma once

#include <sys/types.h>

#include <string>
#include <string_view>
#include <optional>
#include <functional>

namespace maps_scan {
    struct MapInfo;
    using Callback = std::function<bool(const MapInfo &)>;
    using Filter = Callback;
    /// \struct MapInfo
    /// \brief An entry that describes a line in /proc/self/maps. You can obtain a list of these entries
    /// by calling #Scan().
    struct MapInfo {
        /// \brief The start address of the memory region.
        uintptr_t start;
        /// \brief The end address of the memory region.
        uintptr_t end;
        /// \brief The permissions of the memory region. This is a bit mask of the following values:
        /// - PROT_READ
        /// - PROT_WRITE
        /// - PROT_EXEC
        uint8_t perms;
        /// \brief Whether the memory region is private.
        bool is_private;
        /// \brief The offset of the memory region.
        uintptr_t offset;
        /// \brief The device number of the memory region.
        /// Major can be obtained by #major()
        /// Minor can be obtained by #minor()
        dev_t dev;
        /// \brief The inode number of the memory region.
        ino_t inode;
        /// \brief The path of the memory region.
        std::string path;

        /// \brief Scans /proc/self/maps and returns a list of \ref MapInfo entries.
        /// This is useful to find out the inode of the library to hook.
        /// \param[in] pid The process id to scan. This is "self" by default.
        /// \return A list of \ref MapInfo entries.
        static std::vector<MapInfo> Scan(std::string_view pid = "self", std::optional<const Filter> filter = std::nullopt);

        static void ForEach(const Callback &callback, std::string_view pid = "self");

        static inline std::vector<MapInfo> ScanSelf(std::optional<const Filter> filter = std::nullopt) {
            return Scan("self", filter);
        }

        inline bool InRange(uintptr_t addr) const {
            return addr >= start && addr < end;
        }
    };
}
