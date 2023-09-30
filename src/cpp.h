#pragma once

class NoCopy {
public:
    NoCopy() = default;
    NoCopy& operator=(NoCopy&&) = default;
    NoCopy(NoCopy&&) = default;

    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

class NoMove {
public:
    NoMove() = default;
    NoMove(NoMove&&) = delete;
    NoMove& operator=(NoMove&&) = delete;
};