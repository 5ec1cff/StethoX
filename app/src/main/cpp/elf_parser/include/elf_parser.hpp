#pragma once

#include <link.h>
#include <map>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>
#include <string>
#include <functional>

namespace elf_parser {
    struct SymbolInfo {
        uintptr_t value;
        size_t size;
        const char* name;
    };

    class Elf {
        bool valid_{false};
        bool for_dynamic_{false};

        uintptr_t load_base_{0};
        uintptr_t parse_base_{0};
        uintptr_t vaddr_min_{UINTPTR_MAX};
        size_t parse_size_{0};
        ElfW(Ehdr) *header_ = nullptr;
        ElfW(Addr) dynamic_off_{0};
        size_t dynamic_size_{0};

        const char *dynstr_ = nullptr;
        ElfW(Sym) *dynsym_ = nullptr;
        ElfW(Sym) *symtab_ = nullptr;
        ElfW(Off) symtab_count_ = 0;
        ElfW(Off) symstr_ = 0;

        uint32_t nbucket_{};
        uint32_t *bucket_ = nullptr;
        uint32_t *chain_ = nullptr;

        uint32_t gnu_nbucket_{};
        uint32_t gnu_symndx_{};
        uint32_t gnu_bloom_size_{};
        uint32_t gnu_shift2_{};
        uintptr_t *gnu_bloom_filter_{};
        uint32_t *gnu_bucket_{};
        uint32_t *gnu_chain_{};

        // for plt
        bool is_use_rela_ = false;

        ElfW(Addr) rel_plt_ = 0;  //.rel.plt or .rela.plt
        ElfW(Word) rel_plt_size_ = 0;

        ElfW(Addr) rel_dyn_ = 0;  //.rel.dyn or .rela.dyn
        ElfW(Word) rel_dyn_size_ = 0;

        ElfW(Addr) rel_android_ = 0;  // android compressed rel or rela
        ElfW(Word) rel_android_size_ = 0;

        std::unique_ptr<std::vector<uint8_t>> gnu_debugdata_{nullptr};
        std::unique_ptr<Elf> gnu_debugdata_elf_{nullptr};

        mutable std::map<std::string_view, ElfW(Sym) *> symtabs_;

        std::string path;

        constexpr inline static uint32_t GnuHash(std::string_view name) {
            constexpr uint32_t kInitialHash = 5381;
            constexpr uint32_t kHashShift = 5;
            uint32_t hash = kInitialHash;
            for (unsigned char chr: name) {
                hash += (hash << kHashShift) + chr;
            }
            return hash;
        }

        constexpr inline static uint32_t ElfHash(std::string_view name) {
            constexpr uint32_t kHashMask = 0xf0000000;
            constexpr uint32_t kHashShift = 24;
            uint32_t hash = 0;
            for (unsigned char chr: name) {
                hash = (hash << 4) + chr;
                uint32_t tmp = hash & kHashMask;
                hash ^= tmp;
                hash ^= tmp >> kHashShift;
            }
            return hash;
        }

        bool Init(uintptr_t load_base, uintptr_t parse_base, size_t size);

        bool InitFromData(std::vector<uint8_t> &&data);

        ElfW(Sym)* getSym(std::string_view name, uint32_t gnu_hash,
                                 uint32_t elf_hash) const;

        ElfW(Sym)* ElfLookup(std::string_view name, uint32_t hash) const;

        ElfW(Sym)* GnuLookup(std::string_view name, uint32_t hash) const;

        uint32_t ElfLookupIdx(std::string_view name, uint32_t hash) const;

        uint32_t GnuLookupIdx(std::string_view name, uint32_t hash) const;

        void MayInitLinearMap() const;

        ElfW(Sym)* LinearLookup(std::string_view name) const;

        uint32_t LinearLookupForDyn(std::string_view name) const;

        std::vector<ElfW(Addr)> LinearRangeLookup(std::string_view name) const;

        ElfW(Addr) PrefixLookupFirst(std::string_view prefix) const;

        ElfW(Sym)* PrefixLookupFirstSym(std::string_view prefix) const;
        bool LoadSymbolsForDynamic();

        bool LoadSymbolsForFull();

    public:
        Elf() = default;

        bool InitFromFile(std::string_view so_path, uintptr_t base_addr = 0, bool init_sym = false);

        bool InitFromMemory(void* addr, bool init_sym = false);

        bool LoadSymbols();

        std::vector<uintptr_t> FindPltAddr(std::string_view name) const;

        ElfW(Sym)* getSym(std::string_view name) const;

        ElfW(Sym)* getSymByPrefix(std::string_view name) const;

        void forEachSymbols(std::function<bool(const char*, const SymbolInfo& sym)> &&fn) const;

        ~Elf();

        constexpr bool IsValid() const { return valid_; }

        inline auto GetLoadBias() const {
            return static_cast<off_t>(load_base_ - vaddr_min_);
        }

        template<typename T = void *>
        requires(std::is_pointer_v<T>)
        constexpr T getSymbAddress(std::string_view name) const {
            auto sym = getSym(name, GnuHash(name), ElfHash(name));
            if (sym == nullptr) return nullptr;
            auto offset = sym->st_value;
            return reinterpret_cast<T>(
                    static_cast<ElfW(Addr)>(GetLoadBias() + offset));
        }

        template<typename T = void *>
        requires(std::is_pointer_v<T>)
        constexpr T getSymbPrefixFirstAddress(std::string_view prefix) const {
            auto offset = PrefixLookupFirst(prefix);
            if (offset > 0 && load_base_ != 0) {
                return reinterpret_cast<T>(
                        static_cast<ElfW(Addr)>(GetLoadBias() + offset));
            } else {
                return nullptr;
            }
        }

        template<typename T = void *>
        requires(std::is_pointer_v<T>)
        std::vector<T> getAllSymbAddress(std::string_view name) const {
            auto offsets = LinearRangeLookup(name);
            std::vector<T> res;
            res.reserve(offsets.size());
            for (const auto &offset: offsets) {
                res.emplace_back(reinterpret_cast<T>(
                                         static_cast<ElfW(Addr)>(GetLoadBias() + offset)));
            }
            return res;
        }

        inline uintptr_t GetLoadBase() const {
            return load_base_;
        }

        inline void SetLoadBase(uintptr_t base) {
            load_base_ = base;
        }

        inline std::string GetPath() const {
            return path;
        }

        inline ElfW(Ehdr) *GetElfHeader() const {
            return header_;
        }

        inline uintptr_t GetParseBase() const {
            return parse_base_;
        }

        inline bool IsDynamic() const {
            if (!valid_) return false;
            return header_->e_type == ET_DYN;
        }
    };
} // namespace elf_parser
