#pragma once
#ifndef CLIENT_H
#define CLIENT_H

#include <set>
#include <string>
#include "utils.h"

struct client {
    std::string id;
    int fd;
    std::set<std::string> topics;
};

struct sub_unsub {
    int type;   // 0 -disconnect     1 - subscribe      2 - unsubscribe
    char topic[200];
};

#endif