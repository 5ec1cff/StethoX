#include "include/elf_parser.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "xz.h"
#include "logging.h"

#ifndef ELF_ST_TYPE
#define ELF_ST_TYPE(x) (((unsigned int)x) & 0xf)
#endif

#if defined(__arm__)
#define ELF_R_GENERIC_JUMP_SLOT R_ARM_JUMP_SLOT  //.rel.plt
#define ELF_R_GENERIC_GLOB_DAT R_ARM_GLOB_DAT    //.rel.dyn
#define ELF_R_GENERIC_ABS R_ARM_ABS32            //.rel.dyn
#elif defined(__aarch64__)
#define ELF_R_GENERIC_JUMP_SLOT R_AARCH64_JUMP_SLOT
#define ELF_R_GENERIC_GLOB_DAT R_AARCH64_GLOB_DAT
#define ELF_R_GENERIC_ABS R_AARCH64_ABS64
#elif defined(__i386__)
#define ELF_R_GENERIC_JUMP_SLOT R_386_JMP_SLOT
#define ELF_R_GENERIC_GLOB_DAT R_386_GLOB_DAT
#define ELF_R_GENERIC_ABS R_386_32
#elif defined(__x86_64__)
#define ELF_R_GENERIC_JUMP_SLOT R_X86_64_JUMP_SLOT
#define ELF_R_GENERIC_GLOB_DAT R_X86_64_GLOB_DAT
#define ELF_R_GENERIC_ABS R_X86_64_64
#endif

#if defined(__LP64__)
#define ELF_R_SYM(info) ELF64_R_SYM(info)
#define ELF_R_TYPE(info) ELF64_R_TYPE(info)
#else
#define ELF_R_SYM(info) ELF32_R_SYM(info)
#define ELF_R_TYPE(info) ELF32_R_TYPE(info)
#endif

using namespace std::string_view_literals;

namespace {
    bool VerifyElfHeader(ElfW(Ehdr) *header) {
        if (0 != memcmp(header->e_ident, ELFMAG, SELFMAG))
            return false;

#if defined(__LP64__)
        if (ELFCLASS64 != header->e_ident[EI_CLASS])
            return false;
#else
        if (ELFCLASS32 != header->e_ident[EI_CLASS])
          return false;
#endif
        // check endian (little/big)
        if (ELFDATA2LSB != header->e_ident[EI_DATA])
            return false;

        // check version
        if (EV_CURRENT != header->e_ident[EI_VERSION])
            return false;

        // check type
        if (ET_EXEC != header->e_type && ET_DYN != header->e_type)
            return false;

        // check machine
#if defined(__arm__)
        if (EM_ARM != header->e_machine)
          return false;
#elif defined(__aarch64__)
        if (EM_AARCH64 != header->e_machine)
          return false;
#elif defined(__i386__)
        if (EM_386 != header->e_machine)
          return false;
#elif defined(__x86_64__)
        if (EM_X86_64 != header->e_machine)
            return false;
#else
        return false;
#endif
        // check version
        if (EV_CURRENT != header->e_version)
            return false;

        return true;
    }

    std::tuple<uintptr_t, size_t> OpenLibrary(const std::string_view path) {
        int fd = open(path.data(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            PLOGE("elf_parser: open %s", path.data());
            return {0, 0};
        }

        struct stat st{};
        if (fstat(fd, &st) < 0) {
            PLOGE("elf_parser: stat %s", path.data());
            close(fd);
            return {0, 0};
        }
        auto parse_base_ = reinterpret_cast<uintptr_t>(
                mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0));

        close(fd);

        if (parse_base_ == reinterpret_cast<uintptr_t>(MAP_FAILED)) {
            PLOGE("elf_parser: mmap %s", path.data());
            return {0, 0};
        }
        return {parse_base_, st.st_size};
    }

    template<typename T>
    inline constexpr auto offsetOf(ElfW(Ehdr) *head, ElfW(Off) off) {
        return reinterpret_cast<std::conditional_t<std::is_pointer_v<T>, T, T *>>(
                reinterpret_cast<uintptr_t>(head) + off);
    }

    template <typename T>
    inline constexpr auto setByOffset(T &ptr, ElfW(Addr) base, ElfW(Addr) bias, ElfW(Addr) off) {
        if (auto val = bias + off; val > base) {
            ptr = reinterpret_cast<T>(val);
            return true;
        }
        ptr = 0;
        return false;
    }

    std::vector<uint8_t> unxz(const uint8_t *data, size_t size) {
        std::vector<uint8_t> out;
        uint8_t buf[8192];
        xz_crc32_init();
        xz_crc64_init();
        struct xz_dec *dec = xz_dec_init(XZ_DYNALLOC, 1 << 26);
        struct xz_buf b = {.in = data,
                .in_pos = 0,
                .in_size = size,
                .out = buf,
                .out_pos = 0,
                .out_size = sizeof(buf)};
        enum xz_ret ret;
        do {
            ret = xz_dec_run(dec, &b);
            if (ret != XZ_OK && ret != XZ_STREAM_END) {
                LOGE("unxz error: %d", ret);
                out.clear();
                return out;
            }
            out.insert(out.end(), buf, buf + b.out_pos);
            b.out_pos = 0;
        } while (b.in_pos != size);
        return out;
    }
} // namespace

namespace elf_parser {
    bool Elf::Init(uintptr_t load_base, uintptr_t parse_base, size_t size) {
        if (parse_base == 0) {
            LOGE("parse base 0");
            return false;
        }

        if (size != 0 && size < sizeof(*header_)) {
            LOGE("wrong size");
            return false;
        }

        load_base_ = load_base;
        parse_base_ = parse_base;
        parse_size_ = size;

        auto header = reinterpret_cast<decltype(header_)>(parse_base);

        if (!VerifyElfHeader(header)) {
            LOGE("verify elf header");
            return false;
        }
        header_ = header;

        // gnu debugdata maybe has no phdr
        if (header->e_phoff) {
            auto phdr = reinterpret_cast<ElfW(Phdr) *>(GetParseBase() + header->e_phoff);
            for (auto i = 0; i < header_->e_phnum; i++) {
                auto &hdr = phdr[i];
                if (hdr.p_type == PT_LOAD) {
                    if (vaddr_min_ > hdr.p_vaddr) vaddr_min_ = hdr.p_vaddr;
                } else if (hdr.p_type == PT_DYNAMIC) {
                    dynamic_off_ = hdr.p_vaddr;
                    dynamic_size_ = hdr.p_memsz;
                }
            }
        }

        valid_ = true;
        return true;
    }
    
    bool Elf::LoadSymbols() {
        if (!valid_) return false;
        if (for_dynamic_) return LoadSymbolsForDynamic();
        else return LoadSymbolsForFull();
    }
    
    bool Elf::LoadSymbolsForDynamic() {
        if (dynamic_off_ == 0 || dynamic_size_ == 0) return false;
        auto dynamic = reinterpret_cast<ElfW(Dyn)*>(GetLoadBias() + dynamic_off_);

        for (auto *dynamic_end = dynamic + (dynamic_size_ / sizeof(dynamic[0]));
             dynamic < dynamic_end; ++dynamic) {
            switch (dynamic->d_tag) {
                case DT_NULL:
                    // the end of the dynamic-section
                    dynamic = dynamic_end;
                    break;
                case DT_STRTAB: {
                    if (!setByOffset(dynstr_, load_base_, GetLoadBias(), dynamic->d_un.d_ptr)) return false;
                    break;
                }
                case DT_SYMTAB: {
                    if (!setByOffset(dynsym_, load_base_, GetLoadBias(), dynamic->d_un.d_ptr)) return false;
                    break;
                }
                case DT_PLTREL:
                    // use rel or rela?
                    is_use_rela_ = dynamic->d_un.d_val == DT_RELA;
                    break;
                case DT_JMPREL: {
                    if (!setByOffset(rel_plt_, load_base_, GetLoadBias(), dynamic->d_un.d_ptr)) return false;
                    break;
                }
                case DT_PLTRELSZ:
                    rel_plt_size_ = dynamic->d_un.d_val;
                    break;
                case DT_REL:
                case DT_RELA: {
                    if (!setByOffset(rel_dyn_, load_base_, GetLoadBias(), dynamic->d_un.d_ptr)) return false;
                    break;
                }
                case DT_RELSZ:
                case DT_RELASZ:
                    rel_dyn_size_ = dynamic->d_un.d_val;
                    break;
                case DT_ANDROID_REL:
                case DT_ANDROID_RELA: {
                    if (!setByOffset(rel_android_, load_base_, GetLoadBias(), dynamic->d_un.d_ptr)) return false;
                    break;
                }
                case DT_ANDROID_RELSZ:
                case DT_ANDROID_RELASZ:
                    rel_android_size_ = dynamic->d_un.d_val;
                    break;
                case DT_HASH: {
                    // ignore DT_HASH when ELF contains DT_GNU_HASH hash table
                    if (gnu_bloom_filter_) continue;
                    auto *raw = reinterpret_cast<ElfW(Word) *>(GetLoadBias() + dynamic->d_un.d_ptr);
                    nbucket_ = raw[0];
                    bucket_ = raw + 2;
                    chain_ = bucket_ + nbucket_;
                    break;
                }
                case DT_GNU_HASH: {
                    auto *raw = reinterpret_cast<ElfW(Word) *>(GetLoadBias() + dynamic->d_un.d_ptr);
                    gnu_nbucket_ = raw[0];
                    gnu_symndx_ = raw[1];
                    gnu_bloom_size_ = raw[2];
                    gnu_shift2_ = raw[3];
                    gnu_bloom_filter_ = reinterpret_cast<decltype(gnu_bloom_filter_)>(raw + 4);
                    gnu_bucket_ = reinterpret_cast<decltype(bucket_)>(gnu_bloom_filter_ + gnu_bloom_size_);
                    gnu_chain_ = gnu_bucket_ + gnu_nbucket_ - gnu_symndx_;
                    break;
                }
                default:
                    break;
            }
        }
        
        return true;
    }

    bool Elf::LoadSymbolsForFull() {
        auto *section_header = offsetOf<ElfW(Shdr) *>(header_, header_->e_shoff);

        auto shoff = reinterpret_cast<uintptr_t>(section_header);
        char *section_str =
                offsetOf<char *>(header_, section_header[header_->e_shstrndx].sh_offset);

        for (int i = 0; i < header_->e_shnum; i++, shoff += header_->e_shentsize) {
            auto *section = reinterpret_cast<ElfW(Shdr) *>(shoff);
            char *sname = section->sh_name + section_str;
            auto entsize = section->sh_entsize;
            switch (section->sh_type) {
                case SHT_DYNSYM: {
                    dynsym_ = offsetOf<decltype(dynsym_)>(header_, section->sh_offset);
                    break;
                }
                case SHT_SYMTAB: {
                    if (strcmp(sname, ".symtab") == 0) {
                        symtab_count_ = section->sh_size / entsize;
                        symtab_ = offsetOf<decltype(symtab_)>(header_, section->sh_offset);
                    }
                    break;
                }
                case SHT_STRTAB: {
                    if (strcmp(sname, ".dynstr") == 0) {
                        dynstr_ = offsetOf<decltype(dynstr_)>(header_, section->sh_offset);
                    } else if (strcmp(sname, ".strtab") == 0) {
                        symstr_ = section->sh_offset;
                    }
                    break;
                }
                case SHT_PROGBITS: {
                    if (sname == ".gnu_debugdata"sv) {
                        auto gnu_debugdata = unxz(
                                offsetOf<uint8_t *>(header_, section->sh_offset), section->sh_size);
                        auto elf = std::make_unique<Elf>();
                        if (elf->InitFromData(std::move(gnu_debugdata)) && elf->LoadSymbols()) {
                            elf->SetLoadBase(GetLoadBase());
                            elf->vaddr_min_ = vaddr_min_;
                            gnu_debugdata_elf_ = std::move(elf);
                        } else {
                            LOGE("failed to initialize gnu_debugdata");
                        }
                    }
                    break;
                }
                case SHT_HASH: {
                    auto *d_un = offsetOf<ElfW(Word)>(header_, section->sh_offset);
                    nbucket_ = d_un[0];
                    bucket_ = d_un + 2;
                    chain_ = bucket_ + nbucket_;
                    break;
                }
                case SHT_GNU_HASH: {
                    auto *d_buf = reinterpret_cast<ElfW(Word) *>(((uintptr_t) header_) +
                                                                 section->sh_offset);
                    gnu_nbucket_ = d_buf[0];
                    gnu_symndx_ = d_buf[1];
                    gnu_bloom_size_ = d_buf[2];
                    gnu_shift2_ = d_buf[3];
                    gnu_bloom_filter_ =
                            reinterpret_cast<decltype(gnu_bloom_filter_)>(d_buf + 4);
                    gnu_bucket_ = reinterpret_cast<decltype(gnu_bucket_)>(gnu_bloom_filter_ +
                                                                          gnu_bloom_size_);
                    gnu_chain_ = gnu_bucket_ + gnu_nbucket_ - gnu_symndx_;
                    break;
                }
            }
        }
        
        return true;
    }

    ElfW(Sym)* Elf::ElfLookup(std::string_view name, uint32_t hash) const {
        if (auto idx = ElfLookupIdx(name, hash); idx) {
            return dynsym_ + idx;
        }
        return nullptr;
    }

    uint32_t Elf::ElfLookupIdx(std::string_view name, uint32_t hash) const {
        if (nbucket_ == 0)
            return 0;

        for (auto n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
            auto *sym = dynsym_ + n;
            if (name == dynstr_ + sym->st_name) {
                return n;
            }
        }
        return 0;
    }

    ElfW(Sym)* Elf::GnuLookup(std::string_view name, uint32_t hash) const {
        if (auto idx = GnuLookupIdx(name, hash); idx) {
            return dynsym_ + idx;
        }
        return nullptr;
    }

    uint32_t Elf::GnuLookupIdx(std::string_view name, uint32_t hash) const {
        static constexpr auto bloom_mask_bits = sizeof(ElfW(Addr)) * 8;

        if (gnu_nbucket_ == 0 || gnu_bloom_size_ == 0)
            return 0;

        auto bloom_word =
                gnu_bloom_filter_[(hash / bloom_mask_bits) % gnu_bloom_size_];
        uintptr_t mask = 0 | (uintptr_t) 1 << (hash % bloom_mask_bits) |
                         (uintptr_t) 1 << ((hash >> gnu_shift2_) % bloom_mask_bits);
        if ((mask & bloom_word) == mask) {
            auto sym_index = gnu_bucket_[hash % gnu_nbucket_];
            if (sym_index >= gnu_symndx_) {
                do {
                    auto *sym = dynsym_ + sym_index;
                    if (((gnu_chain_[sym_index] ^ hash) >> 1) == 0 &&
                        name == dynstr_ + sym->st_name) {
                        return sym_index;
                    }
                } while ((gnu_chain_[sym_index++] & 1) == 0);
            }
        }
        return 0;
    }

    void Elf::MayInitLinearMap() const {
        if (for_dynamic_) return;
        if (symtabs_.empty()) {
            if (symtab_ != nullptr && symstr_ != 0) {
                for (ElfW(Off) i = 0; i < symtab_count_; i++) {
                    unsigned int st_type = ELF_ST_TYPE(symtab_[i].st_info);
                    const char *st_name = offsetOf<const char *>(
                            header_, symstr_ + symtab_[i].st_name);
                    if ((st_type == STT_FUNC || st_type == STT_OBJECT) &&
                        symtab_[i].st_size) {
                        symtabs_.emplace(st_name, &symtab_[i]);
                    }
                }
            }
        }
    }

    ElfW(Sym)* Elf::LinearLookup(std::string_view name) const {
        MayInitLinearMap();
        if (auto i = symtabs_.find(name); i != symtabs_.end()) {
            return i->second;
        } else {
            return nullptr;
        }
    }

    std::vector<ElfW(Addr)> Elf::LinearRangeLookup(std::string_view name) const {
        MayInitLinearMap();
        std::vector<ElfW(Addr)> res;
        for (auto [i, end] = symtabs_.equal_range(name); i != end; ++i) {
            auto offset = i->second->st_value;
            res.emplace_back(offset);
        }
        if (gnu_debugdata_) {
            auto gnu_debugdata = gnu_debugdata_elf_->LinearRangeLookup(name);
            res.insert(res.end(), gnu_debugdata.begin(), gnu_debugdata.end());
        }
        return res;
    }

    ElfW(Sym)* Elf::PrefixLookupFirstSym(std::string_view prefix) const {
        MayInitLinearMap();
        if (auto i = symtabs_.lower_bound(prefix);
                i != symtabs_.end() && i->first.starts_with(prefix)) {
            return i->second;
        } else if (gnu_debugdata_elf_) {
            return gnu_debugdata_elf_->PrefixLookupFirstSym(prefix);
        } else {
            return 0;
        }
    }

    ElfW(Addr) Elf::PrefixLookupFirst(std::string_view prefix) const {
        MayInitLinearMap();
        if (auto sym = PrefixLookupFirstSym(prefix); sym != nullptr) {
            return sym->st_value;
        }
        return 0;
    }

    ElfW(Sym)* Elf::getSym(std::string_view name, uint32_t gnu_hash,
                                  uint32_t elf_hash) const {
        auto sym = GnuLookup(name, gnu_hash);
        if (!sym) sym = ElfLookup(name, elf_hash);
        if (!sym && gnu_debugdata_elf_) sym = gnu_debugdata_elf_->getSym(name, gnu_hash, elf_hash);
        if (!sym) sym = LinearLookup(name);
        return sym;
    }

    bool Elf::InitFromData(std::vector<uint8_t> &&gnu_debugdata) {
        auto data = reinterpret_cast<uintptr_t>(gnu_debugdata.data());
        if (!Init(data, data, 0)) return false;
        gnu_debugdata_ =
                std::make_unique<std::vector<uint8_t>>(std::move(gnu_debugdata));
        return true;
    }

    bool Elf::InitFromFile(std::string_view so_path, uintptr_t base_addr, bool init_sym) {
        auto [addr, size] = OpenLibrary(so_path);
        path = so_path;
        if (Elf::Init(base_addr, addr, size)) {
            return !init_sym || LoadSymbols();
        }
        return false;
    }

    bool Elf::InitFromMemory(void *addr, bool init_sym) {
        auto a = reinterpret_cast<uintptr_t>(addr);
        if (Elf::Init(a, a, 0)) {
            for_dynamic_ = true;
            return !init_sym || LoadSymbols();
        }
        return false;
    }

    Elf::~Elf() {
        if (parse_size_ != 0)
            munmap(reinterpret_cast<void *>(parse_base_), parse_size_);
    }

    uint32_t Elf::LinearLookupForDyn(std::string_view name) const {
        if (!for_dynamic_) return 0;
        if (!dynsym_ || !gnu_symndx_) return 0;
        for (uint32_t idx = 0; idx < gnu_symndx_; idx++) {
            auto *sym = dynsym_ + idx;
            if (name == dynstr_ + sym->st_name) {
                return idx;
            }
        }
        return 0;
    }

    ElfW(Sym)* Elf::getSym(std::string_view name) const {
        return getSym(name, GnuHash(name), ElfHash(name));
    }

    ElfW(Sym)* Elf::getSymByPrefix(std::string_view name) const {
        return PrefixLookupFirstSym(name);
    }

    std::vector<uintptr_t> Elf::FindPltAddr(std::string_view name) const {
        std::vector<uintptr_t> res;

        uint32_t idx = GnuLookupIdx(name, GnuHash(name));
        if (!idx) idx = ElfLookupIdx(name, ElfHash(name));
        if (!idx) idx = LinearLookupForDyn(name);
        if (!idx) return res;
        auto bias = GetLoadBias();

        auto looper = [&res, idx, bias]<typename T>(auto begin, auto size, bool is_plt) -> void {
            const auto *rel_end = reinterpret_cast<const T *>(begin + size);
            for (const auto *rel = reinterpret_cast<const T *>(begin); rel < rel_end; ++rel) {
                auto r_info = rel->r_info;
                auto r_offset = rel->r_offset;
                auto r_sym = ELF_R_SYM(r_info);
                auto r_type = ELF_R_TYPE(r_info);
                if (r_sym != idx) continue;
                if (is_plt && r_type != ELF_R_GENERIC_JUMP_SLOT) continue;
                if (!is_plt && r_type != ELF_R_GENERIC_ABS && r_type != ELF_R_GENERIC_GLOB_DAT) {
                    continue;
                }
                auto addr = bias + r_offset;
                if (addr > bias) res.emplace_back(addr);
                if (is_plt) break;
            }
        };

        for (const auto &[rel, rel_size, is_plt] :
                {std::make_tuple(rel_plt_, rel_plt_size_, true),
                 std::make_tuple(rel_dyn_, rel_dyn_size_, false),
                 std::make_tuple(rel_android_, rel_android_size_, false)}) {
            if (!rel) continue;
            if (is_use_rela_) {
                looper.template operator()<ElfW(Rela)>(rel, rel_size, is_plt);
            } else {
                looper.template operator()<ElfW(Rel)>(rel, rel_size, is_plt);
            }
        }

        return res;
    }

    void Elf::forEachSymbols(std::function<bool(const char*, const SymbolInfo& sym)> &&fn) const {
        MayInitLinearMap();
        for (auto &[name, sym]: symtabs_) {
            SymbolInfo info;
            info.name = offsetOf<const char *>(header_, symstr_ + sym->st_name);
            info.value = GetLoadBias() + sym->st_value;
            info.size = sym->st_size;
            if (!fn(name.data(), info)) break;
        }
        if (gnu_debugdata_elf_) gnu_debugdata_elf_->forEachSymbols(std::move(fn));
    }
} // namespace elf_parser
