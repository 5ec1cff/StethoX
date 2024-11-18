project(elf_parser)

add_library(elf_parser STATIC
        elf_parser.cc
        xz-embedded/xz_crc32.c
        xz-embedded/xz_crc64.c
        xz-embedded/xz_dec_lzma2.c
        xz-embedded/xz_dec_stream.c
)
target_include_directories(elf_parser PUBLIC include)
target_include_directories(elf_parser PRIVATE xz-embedded)
