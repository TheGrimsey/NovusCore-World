#pragma once
#include <Database/DBConnection.h>
#include <Database/DBTypes.h>

struct DBSingleton
{
public:
    DBSingleton() : auth() { }

    DBConnection auth;
};