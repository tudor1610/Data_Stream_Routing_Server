#include <cstddef>
#include <sstream>
#include <string>
#include <vector>
#include "client.h"


bool match_recursive(const std::vector<std::string>& str, size_t i,
                    const std::vector<std::string>& top, size_t j) {
    // All the words matched
    if (i == str.size() && j == top.size())
        return true;
    // The topic is longer than the subscription (no wildcards)
    if (i == str.size())
        return false;
    // The topic is shorter than the subscription
    if (j == top.size() && str[i] != "*")
        return false;

    // The words match; check the next two words
    if (str[i] == "+" || str[i] == top[j])
        return match_recursive(str, i + 1, top, j + 1);

    // Wildcard; Check for match with all the remaining words
    // in topic (can skip one or more words)
    if (str[i] == "*") {
        size_t k = 0;
        while (j + k <= top.size()) {
            if (match_recursive(str, i + 1, top, j + k)) {
                return true;
            }
            k++;
        }
    }
    return false;
}

// Splits the subscription and the topic word by word
// and stores them in two arrays
bool split_and_match(std::string str, std::string topic) {

    std::vector<std::string> string_parts;
    std::vector<std::string> topic_parts;

    std::stringstream s1(str);
    std::string token;
    while (std::getline(s1, token, '/')) {
        string_parts.push_back(token);
    }

    std::stringstream s2(topic);
    std::string token2;
    while (std::getline(s2, token2, '/')) {
        topic_parts.push_back(token2);
    }

    return match_recursive(string_parts, 0, topic_parts, 0);
}

// For every client subscription, check if it matches the given topic
bool match_client_subscription(struct client *client, std::string top)
{
    if (client->topics.count(top))
        return true;
    for (const std::string& sub : client->topics) {
        if (split_and_match(sub, top))
            return true;
    }
    return false;
}
