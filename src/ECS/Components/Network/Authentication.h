#pragma once
#include <NovusTypes.h>
#include <Utils/srp.h>

struct Authentication
{
    std::string username = "";
    SRPVerifier srp;
};