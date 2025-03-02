#pragma once

#include <jni.h>
#include "elf_parser.hpp"

bool InitClassLoaders(elf_parser::Elf &art);
