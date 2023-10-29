#pragma once

#include <memory>
#include <stdio.h>
#include <mntent.h>

struct CFileDeleter {
    void operator()(FILE* c) {
        fclose(c);
    }
};

using UniqueFile = std::unique_ptr<FILE, CFileDeleter>;

struct CMntEntDeleter {
    void operator()(FILE* c) {
        endmntent(c);
    }
};

using UniqueMntent = std::unique_ptr<FILE, CMntEntDeleter>;