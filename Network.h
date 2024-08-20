#pragma once
#include <iostream>
#include <Windows.h>
#include "Network.h"

//-------------------------------------------------------------
// define 및 전역 변수
//-------------------------------------------------------------


#define SERVER_PORT 25000
#define WM_SOCKET (WM_USER + 1)

#define HEADER_SIZE 2
#define PAYLOAD_SIZE 16
#define MESSAGE_SIZE 18


#pragma pack(push, 1)
struct stHeader
{
    unsigned short Len;
};


struct st_DRAW_PACKET
{
    int startX;
    int startY;
    int endX;
    int endY;
};


struct ALL_PACKET
{
    unsigned short Len;
    int startX;
    int startY;
    int endX;
    int endY;
};
#pragma pack(pop)


class RingBuffer
{
public:
    RingBuffer(int capacity);
    ~RingBuffer();

    int getSize() { return _size; }
    int getCapacity() const { return _capacity; }
    int Enqueue(const char* data, int bytes);
    int Dequeue(char* data, int bytes);


    int peek(char* data, int bytes);

private:
    int _begin_index;
    int _end_index;
    int _size;
    int _capacity;
    char* buffer;
};

RingBuffer::RingBuffer(int capacity)
    : _begin_index(0)
    , _end_index(0)
    , _size(0)
    , _capacity(capacity)
{
    buffer = new char[capacity];
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

int RingBuffer::Enqueue(const char* data, int bytes)
{
    if (bytes == 0) return 0;

    int capacity = _capacity;
    int bytes_to_write = min(bytes, capacity - _size); // Free Size와 비교

    // 덮어쓰기가 발생하는지 확인
    if (_size + bytes_to_write > capacity) {
        // 덮어쓰기가 발생할 가능성이 있는 경우, 쓰기를 제한
        bytes_to_write = capacity - _size;
    }


    // 만약 쓰려고 하는 바이트가, ( Free Size ) 보다 작거나 같다면
    if (bytes_to_write <= capacity - _end_index)
    {
        memcpy(buffer + _end_index, data, bytes_to_write);
        _end_index += bytes_to_write;
        if (_end_index == capacity) _end_index = 0;
    }
    // 넘었으면 ....
    else
    {
        int Use_Size = capacity - _end_index;  // Use Size
        memcpy(buffer + _end_index, data, Use_Size);  // 사용 가능한 만큼 쓰고

        int Remaining_Byte = bytes_to_write - Use_Size;  // 일단 쓴 바이트 빼고, 남은 바이트 사이즈
        memcpy(buffer, data + Use_Size, Remaining_Byte); // 앞에다가 씀

        _end_index = Remaining_Byte;
    }

    _size += bytes_to_write;
    return bytes_to_write;
}

int RingBuffer::Dequeue(char* data, int bytes)
{
    if (bytes == 0) return 0;

    int capacity = _capacity;
    int bytes_to_read = min(bytes, _size);

    // Read in a single step
    if (bytes_to_read <= capacity - _begin_index)
    {
        memcpy(data, buffer + _begin_index, bytes_to_read);
        _begin_index += bytes_to_read;
        if (_begin_index == capacity) _begin_index = 0;
    }
    // Read in two steps
    else
    {
        int size_1 = capacity - _begin_index;
        memcpy(data, buffer + _begin_index, size_1);
        int size_2 = bytes_to_read - size_1;
        memcpy(data + size_1, buffer, size_2);
        _begin_index = size_2;
    }

    _size -= bytes_to_read;
    return bytes_to_read;
}


int RingBuffer::peek(char* data, int bytes)
{
    if (bytes == 0) return 0;

    int capacity = _capacity;
    int bytes_to_read = min(bytes, _size);

    // Read in a single step
    if (bytes_to_read <= capacity - _begin_index)
    {
        memcpy(data, buffer + _begin_index, bytes_to_read);
    }
    // Read in two steps
    else
    {
        int size_1 = capacity - _begin_index;
        memcpy(data, buffer + _begin_index, size_1);
        int size_2 = bytes_to_read - size_1;
        memcpy(data + size_1, buffer, size_2);
    }

    return bytes_to_read;
}


