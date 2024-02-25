#ifndef __PAGE_STRUCTURE_H__
#define __PAGE_STRUCTURE_H__
#include "Common.h"
#include "GlobalAddress.h"
#include <iostream>

extern const Value kValueNull;

class Header {
private:
  GlobalAddress leftmost_ptr;
  GlobalAddress sibling_ptr;
  uint8_t level;
  int16_t last_index;
  Key lowest;
  Key highest;

  friend class InternalPage;
  friend class LeafPage;
  friend class Tree;
  friend class IndexCache;
  friend class DpuProcessor;

public:
  Header() {
    leftmost_ptr = GlobalAddress::Null();
    sibling_ptr = GlobalAddress::Null();
    last_index = -1;
    lowest = kKeyMin;
    highest = kKeyMax;
  }

  void debug() const {
    std::cout << "leftmost=" << leftmost_ptr << ", "
              << "sibling=" << sibling_ptr << ", "
              << "level=" << (int)level << ","
              << "cnt=" << last_index + 1 << ","
              << "range=[" << lowest << " - " << highest << "]";
  }
} __attribute__((packed));
;

class InternalEntry {
public:
  Key key;
  GlobalAddress ptr;

  InternalEntry() {
    ptr = GlobalAddress::Null();
    key = 0;
  }
} __attribute__((packed));

class LeafEntry {
public:
  uint8_t f_version : 4;
  Key key;
  Value value;

  uint8_t r_version : 4;

  LeafEntry() {
    f_version = 0;
    r_version = 0;
    value = kValueNull;
    key = 0;
  }
} __attribute__((packed));

constexpr int kInternalCardinality = (kInternalPageSize - sizeof(Header) -
                                      sizeof(uint8_t) * 2 - sizeof(uint64_t)) /
                                     sizeof(InternalEntry);

constexpr int kLeafCardinality =
    (kLeafPageSize - sizeof(Header) - sizeof(uint8_t) * 2 - sizeof(uint64_t)) /
    sizeof(LeafEntry);

class InternalPage {
public:
  union {
    uint32_t crc;
    uint64_t embedding_lock;
    uint64_t index_cache_freq;
  };

  uint8_t front_version;
  Header hdr;
  InternalEntry records[kInternalCardinality];

  uint8_t padding[3];
  uint8_t rear_version;

  friend class Tree;
  friend class IndexCache;
  friend class DpuProcessor;

public:
  // this is called when tree grows
  InternalPage(GlobalAddress left, const Key &key, GlobalAddress right,
               uint32_t level = 0) {
    hdr.leftmost_ptr = left;
    hdr.level = level;
    records[0].key = key;
    records[0].ptr = right;
    records[1].ptr = GlobalAddress::Null();

    hdr.last_index = 0;

    front_version = 0;
    rear_version = 0;
  }

  InternalPage(uint32_t level = 0) {
    hdr.level = level;
    records[0].ptr = GlobalAddress::Null();

    front_version = 0;
    rear_version = 0;

    embedding_lock = 0;
  }

  void set_consistent() {
    front_version++;
    rear_version = front_version;
#ifdef CONFIG_ENABLE_CRC
    this->crc =
        CityHash32((char *)&front_version, (&rear_version) - (&front_version));
#endif
  }

  bool check_consistent() const {

    bool succ = true;
#ifdef CONFIG_ENABLE_CRC
    auto cal_crc =
        CityHash32((char *)&front_version, (&rear_version) - (&front_version));
    succ = cal_crc == this->crc;
#endif
    succ = succ && (rear_version == front_version);
    return succ;
  }

  void debug() const {
    std::cout << "InternalPage@ ";
    hdr.debug();
    std::cout << "version: [" << (int)front_version << ", " << (int)rear_version
              << "]" << std::endl;
  }

  void verbose_debug() const {
    this->debug();
    for (int i = 0; i < this->hdr.last_index + 1; ++i) {
      printf("[%lu %lu] ", this->records[i].key, this->records[i].ptr.val);
    }
    printf("\n");
  }

} __attribute__((packed));

class LeafPage {
private:
  union {
    uint32_t crc;
    uint64_t embedding_lock;
  };
  //uint8_t front_version;
  //Header hdr;
  //Key key_list[kLeafCardinality];
  //int16_t last_index;
  //uint8_t rear_version;

  uint8_t front_version;
  Header hdr;
  LeafEntry records[kLeafCardinality];

  // uint8_t padding[1];
  uint8_t rear_version;

  friend class Tree;
  friend class DpuProcessor;

public:
  LeafPage(uint32_t level = 0) {
    hdr.level = level;
    records[0].value = kValueNull;

    front_version = 0;
    rear_version = 0;

    embedding_lock = 0;
  }

  void set_consistent() {
    front_version++;
    rear_version = front_version;
#ifdef CONFIG_ENABLE_CRC
    this->crc =
        CityHash32((char *)&front_version, (&rear_version) - (&front_version));
#endif
  }

  bool check_consistent() const {

    bool succ = true;
#ifdef CONFIG_ENABLE_CRC
    auto cal_crc =
        CityHash32((char *)&front_version, (&rear_version) - (&front_version));
    succ = cal_crc == this->crc;
#endif

    succ = succ && (rear_version == front_version);

    return succ;
  }

  void debug() const {
    std::cout << "LeafPage@ ";
    hdr.debug();
    std::cout << "version: [" << (int)front_version << ", " << (int)rear_version
              << "]" << std::endl;
  }

} __attribute__((packed));

#endif