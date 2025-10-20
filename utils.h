#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

#define MAX_CLIENTS 50
#define ID_LEN 10
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)


struct udp_packet {
  char topic[50];
  uint8_t type; 
  unsigned char payload[1500];
};

struct notification {
  char ip[16];
  uint16_t port;
  struct udp_packet packet;
};

#endif