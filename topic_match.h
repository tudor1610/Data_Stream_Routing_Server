#pragma once
#ifndef MATCH_H
#define MATCH_H

#include <sstream>
#include <string>
#include <vector>
#include "client.h"

/*  Matches recuresively two strings, word by word.
    "*" - match one or more words

    "+" - match 1 word
*/
bool match_recursive(const std::vector<std::string>& str, size_t i,
                    const std::vector<std::string>& top, size_t j);

/*  Splits two strings into words; Calls match_recursive() 
    to check if the strings match.
*/
bool split_and_match(std::string str, std::string topic);

/*  Returns TRUE if any of the client's subscribed topics
    matches the given topic. FALSE if not.
*/
bool match_client_subscription(struct client *client, std::string top);

#endif