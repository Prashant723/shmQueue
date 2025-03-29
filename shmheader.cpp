#pragma once

#include <cstdint>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <cstring>

#include "shmheader.hpp"


template <typename TData, typename TMeta = int32_t>
class ShmQueue
{
protected:
      struct Block
      {
              std::atomic<int64_t> m_seq;
              TData m_value;
    };

public:
    using MetatdataSanityFn = std::function<bool(TMeta)>;

    // Only 1 thread can call init() on an instance
    ShmQueue(int shmKey, uint64_t shmSize, [[maybe_unused]] MetatdataSanityFn sanityFn = {}) : m_numSlots(nextPow2(shmSize)), m_sizeMask(m_numSlots - 1)
    {
            // std::cout << "[DOC] " <<  __FILE__ << " init ShmQueue" << std::endl;
            std::cout << "ShmKey: " << shmKey << " ShmSize: " << shmSize << std::endl;
            auto dataBlockSize = sizeof(Block);
            auto shmId = shmget(shmKey, m_numSlots * dataBlockSize + sizeof(ShmHeader) + sizeof(TMeta), IPC_CREAT | IPC_EXCL | 0666);
            bool alreadyExists = false;
            if (shmId < 0)
            {
                    std::cerr << "Shm already exists" << std::endl;
                    alreadyExists = true;
            }
            shmId = shmget(shmKey, m_numSlots * dataBlockSize + sizeof(ShmHeader) + sizeof(TMeta), IPC_CREAT | 0666);
            if (shmId < 0)
            {
                    std::cerr << "Unable to create shm " << shmId << std::endl;
                    exit(-1);
            }

            m_header = (ShmHeader *)(((char *)shmat(shmId, 0, 0)) + sizeof(TMeta));
            m_data = (Block *)(((char *)m_header) + sizeof(ShmHeader));
            if (!alreadyExists)
            {
                    m_header->m_tail = 0;
                    for (uint64_t i = 0; i < m_numSlots; i++)
                    {
                            m_data[i].m_seq = -1;
                    }
            }

            m_readHead = 0;

            std::cerr << "Shm Tail : " << m_header->m_tail << std::endl;

            // return true;
    }

    void attachLatest()
    {
            m_readHead = m_header->m_tail;
    }

    void setReadPosition(uint64_t readPosition)
    {
            m_readHead = readPosition;
            std::cout << "read position set as: " << readPosition << std::endl;
    }

    uint64_t getNoOfSlots() const
    {
            return m_numSlots;
    }

    bool push(const TData &data)
    {
            auto seq = m_header->m_tail.fetch_add(1, std::memory_order_acq_rel);
            auto slot = seq & m_sizeMask;
            std::memcpy((char *)&m_data[slot].m_value, (char *)&data, sizeof(TData));
            // m_data[slot].m_value = data;
            m_data[slot].m_seq.store(seq, std::memory_order_release);
            return true;
    }

    std::pair<uint64_t, TData *> getWriteSlot()
    {
            auto seq = m_header->m_tail.fetch_add(1, std::memory_order_acq_rel);
            auto slot = seq & m_sizeMask;
            return std::make_pair(seq, &m_data[slot].m_value);
    }

    void commitWrite(uint64_t seq)
    {
            m_data[seq & m_sizeMask].m_seq.store(seq, std::memory_order_acq_rel);
    }

    bool ready() const
    {
            auto slot = m_readHead & m_sizeMask;
            return m_readHead < m_header->m_tail.load(std::memory_order_acquire) && m_data[slot].m_seq.load(std::memory_order_acquire) >= m_readHead;
    }

    int64_t getReadSlot()
    {
            return m_readHead & m_sizeMask;
    }

    TData *read()
    {
            auto slot = m_readHead & m_sizeMask;
            m_readHead = m_data[slot].m_seq.load(std::memory_order_acquire) + 1;
            return &(m_data[slot].m_value);
    }

    // void printAll() {
    //   for (int i = 0; i < m_header->m_tail -1; i++) {
    //     auto block = m_data + i;
    //     std::cout << block->m_value << " " << block->m_seq << std::endl;
    //   }
    // }

protected:
    ShmHeader *m_header = nullptr;
    Block *m_data = nullptr;
    const uint64_t m_numSlots;
    const uint64_t m_sizeMask;
    int64_t m_readHead = 0;

    uint64_t nextPow2(uint64_t x) const
    {
            uint64_t res = 1;
            while (res < x)
            {
                    res *= 2;
            }
            return res;
    }
};
