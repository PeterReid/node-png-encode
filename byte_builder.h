#ifndef BYTE_BUILDER_H
#define BYTE_BUILDER_H

class ByteBuilder 
{
public:

    uint8_t *buffer;
    size_t length;
    bool alloc_failure;

private:
    size_t alloc_length;

public:
    ByteBuilder(size_t initial_length) {
        buffer = (uint8_t *)malloc(initial_length);
        alloc_failure = !buffer;
        length = 0;
        alloc_length = initial_length;
    }
    
    void append(const uint8_t *data, size_t data_length) {
        if (alloc_failure) return;

        if (length + data_length > alloc_length) {
            size_t larger_alloc_length = ((length + data_length) << 1);
            uint8_t *larger_buffer = (uint8_t *)realloc(buffer, larger_alloc_length);
            if (!larger_buffer) {
                alloc_failure = true;
                return;
            }
            buffer = larger_buffer;
            alloc_length = larger_alloc_length;
        }
        
        memcpy(buffer + length, data, data_length);
        length += data_length;
    }

    void copy_to(char *destination) {
        memcpy(destination, buffer, length);
    }

    ~ByteBuilder() {
        free(buffer);
    }

private:
    ByteBuilder(const ByteBuilder &);
    ByteBuilder &operator=(const ByteBuilder &);
};


#endif
