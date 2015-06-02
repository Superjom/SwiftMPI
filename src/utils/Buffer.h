//
//  Buffer.h
//  SwiftSnails
//
//  Created by Chunwei on 12/22/14.
//  Copyright (c) 2014 Chunwei. All rights reserved.
//
#ifndef SwiftSnails_utils_Buffer_h_
#define SwiftSnails_utils_Buffer_h_
#include "common.h"
#include "string.h"
//#include "../SwiftSnails/Message.h"
namespace swift_snails {

class BasicBuffer {
public:
    explicit BasicBuffer() {
        CHECK( capacity() > 0);
        _buffer = new char[_capacity];
        _cursor = _end = _buffer;
    }
    BasicBuffer(const BasicBuffer&) = delete;
    explicit BasicBuffer(BasicBuffer&& other) :
        _buffer(other._buffer),
        _cursor(other._cursor),
        _end(other._end),
        _capacity(other._capacity)
    {
        other.set_buffer(nullptr);  // to avoid free the common memory
        other.clear();  // clear all the status flags
    }

    void set(char* buffer, size_t size) {
        CHECK(size > 0);
        reserve(size);
        memcpy(_buffer, buffer, size);
        _cursor = _buffer;
        _end = _buffer + size;
    }

    BasicBuffer& operator=(const BasicBuffer&) = delete;
    BasicBuffer& operator=(BasicBuffer&& other) {
        if (this != &other) {
            free();     // clean original buffer
            clear();    // clear and init all the flags
            _buffer = other._buffer;
            _cursor = other._cursor;
            _end = other._end;
        }
        return *this;
    }
    /*
    BasicBuffer& operator=(Message&& m) {
    	CHECK(m.zmg());
    	free();
    	_buffer = &m.zmg();
    	_cursor = buffer();
    	_end = buffer() + m.size();
    	return *this;
    }
    */
    ~BasicBuffer() {
        //LOG(INFO) << "Buffer deconstruct!";
        free();
    }

    char* buffer() const {
        return _buffer;
    }
    char* cursor() const {
        return _cursor;
    }
    char* end() const {
        return _end;
    }
    size_t capacity() const {
        return _capacity;
    }   
    /*
     * get the actual size of data
     *
     * size() has no relation with cursor's position
     *
     * Attention: size() will not change duiring the read period
     */
    size_t size() const {
        return end() - buffer();
    }

    /*
     * get the size of the data that has been read
     */
    size_t read_size() const {
        return cursor() - buffer();
    }

    std::string status () const {
    	std::stringstream os;
        os << "BinaryBuffer Status" << std::endl;
        os << "buffer:\t" << _buffer << std::endl;
        os << "cursor:\t" << _cursor << std::endl;
        os << "size:\t" << size() << std::endl;
        os << "capacity:\t" << capacity() << std::endl;
        os << "end:\t" << _cursor << std::endl;
        return std::move(os.str());
    }
    
    void set_buffer(char* x) {
        _buffer = x;
    }
    void set_cursor(char* x) {
        CHECK(_cursor < end());
        _cursor = x;
    }
    void set_end(char* x) {
        _end = x;
    }
    /*
     * put the cursor to the begin of buffer
     */
    void reset_cursor() { 
        _cursor = buffer();
    }
    /*
     * check if finish readding 
     */
    bool read_finished() const {
        CHECK(cursor() <= end());
        return cursor() == end();
    }

    // free memory and reset flags include `buffer` and `capacity`
    void free() {   
        //LOG(INFO) << "free() is called";
        if(_buffer) {
            delete _buffer; 
            _buffer = nullptr;
            _capacity = 0;
        }
    }
    // clear data and read flags
    void clear() {
        _cursor = _buffer;
        _end = _buffer;
    }

protected:
    void reserve(size_t newcap) {
        CHECK(newcap > 0);
        //LOG(WARNING) << "reserve new memory:\t" << newcap;
        if(newcap > capacity()) {
            char* newbuf = new char[newcap];
            if (size() > 0) {
                memcpy(newbuf, buffer(), size());
            }
            _cursor = newbuf + (cursor() - buffer());
            _end = newbuf + (end() - buffer());
            free(); 
            _capacity = newcap;
            _buffer = newbuf;
        }
        //LOG(INFO) << "memory reserve ok!";
    }
protected:
    // used in read mod
    void cursor_preceed(size_t size) {
      //CHECK(cursor() + size < end());
      _cursor += size;
      //_end = _cursor + 1;
    }
    // used in write mod
    void end_preceed(size_t size) {
        _end += size;
    }

private:
    char* _buffer = nullptr;    // memory address
    char* _cursor = nullptr;    // read cursor
    char* _end = nullptr;       // the next byte of valid buffer's tail
    size_t _capacity = 1024;
};  // end class BasicBuffer


/*
 * Binary Buffer support
 *
 * flags:
 *
 *  buffer
 *  cursor
 *  end
 *
 *  `cursor` is used for reading, it records the read status
 *  `end` record the end position of the data saved in buffer
 *  and new data will be appended after `end`
 */
class BinaryBuffer  : public BasicBuffer {
public:
    // define << operator for basic types
    #define SS_REPEAT_PATTERN(T) \
    BasicBuffer& operator>>(T& x) { \
        get_raw(x); \
        return *this; \
    } \
    BasicBuffer& operator<<(const T& x) { \
        put_raw(x); \
        return *this; \
    }
    SS_REPEAT6(int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t)
    SS_REPEAT2(double, float)
    SS_REPEAT1(bool)
    SS_REPEAT1(byte_t)
    #undef SS_REPEAT_PATTERN

    template<typename T>
    T get() {
        T x;
        *this >> x;
        return std::move(x);
    }

protected:
    // T should be basic types
    template<typename T>
    void get_raw(T& x) {
        CHECK(! read_finished());
        memcpy(&x, cursor(), sizeof(T));
        cursor_preceed(sizeof(T));
        //set_cursor(cursor() + sizeof(T));
    }
    /*
     * read begin from the current position of the cursor
     * when the cursor reach the end of the buffer, the reading
     * will be end
     */
    template<typename T>
    T get_raw() {
        T x;
        CHECK(! read_finished());
        memcpy(&x, cursor(), sizeof(T));
        //set_cursor(cursor() + sizeof(T));
        cursor_preceed(sizeof(T));
        return std::move(x);
    }
    /*
     * append data from the current position of the curosr
     */
    template<typename T>
    void put_raw(T& x) {
        if( size() + sizeof(x) > capacity()) {
            size_t newcap = 2 * capacity();
            reserve(newcap);
        }
        memcpy(end(), &x, sizeof(T));
        //end_preceed(sizeof(T));
        end_preceed(sizeof(T));
        //_end += sizeof(T);
        //put_cursor_preceed(sizeof(T));
    }

};  // end class BinaryBuffer

/*
 * 以行为单位管理文本
 * 可以用于数据分发
 */
class TextBuffer    : public BasicBuffer {
public:
    static const std::string delimiter;     // to split numbers
    static const std::string cendl;         // to split records

    template<typename T>
    void put_math(T& x) {   // just copy data's string to buffer
        std::string x_ = std::to_string(x);
        *this << x_;
    }
    // ints
    #define SS_REPEAT_PATTERN(T) \
    void get_math(T& x) { \
        char *cend = nullptr; \
        x = (T)strtol(cursor(), &cend, 10); \
        set_cursor(cend); \
    }
    SS_REPEAT4(int16_t, int32_t, int64_t, bool)
    #undef SS_REPEAT_PATTERN

    // uints
    #define SS_REPEAT_PATTERN(T) \
    void get_math(T& x) { \
        char *cend = nullptr; \
        x = (T)strtoul(cursor(), &cend, 10); \
        set_cursor(cend); \
    }
    SS_REPEAT3(uint16_t, uint32_t, uint64_t)
    #undef SS_REPEAT_PATTERN

    // float double
    #define SS_REPEAT_PATTERN(T) \
    void get_math(T& x) { \
        char *cend = nullptr; \
        x = (T)strtod(cursor(), &cend); \
        set_cursor(cend); \
    }
    SS_REPEAT2(float, double)
    #undef SS_REPEAT_PATTERN

    TextBuffer &operator<< (const std::string &x) {
        //swift_snails::trim(x);
        if(x.size() + size() > capacity()) {
            size_t newcap = 2 * capacity();
            reserve(newcap);
        }
        std::cout << "x:\t" << x << std::endl;
        strncpy(end(), x.c_str(), x.size());
        end_preceed(x.size());
        //put_cursor_preceed(x.size());
        //set_end(cursor() + 1);
        return *this;
    }
    // do not insert the delimiter automatically
    // user should insert one manually
    #define SS_REPEAT_PATTERN(T) \
    TextBuffer &operator<< (T x) { \
        put_math(x); \
        return *this; \
    } 
    SS_REPEAT6(int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t)
    SS_REPEAT3(bool, double, float)
    #undef SS_REPEAT_PATTERN

    #define SS_REPEAT_PATTERN(T) \
    TextBuffer& operator>> (T &x) { \
        get_math(x); \
        return *this; \
    }
    SS_REPEAT6(int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t)
    SS_REPEAT3(bool, double, float)
    #undef SS_REPEAT_PATTERN

    std::string getline() {
        char *pos = cursor();
        for(; !std::isspace(*pos); ++pos){};
        std::string tmps(cursor(), pos-cursor());
        set_cursor(pos);
        return std::move(tmps);
    }

};  // end class TextBuffer
const std::string TextBuffer::delimiter = " ";
const std::string TextBuffer::cendl = "\n";

};  // end namespace swift_snails
#endif
