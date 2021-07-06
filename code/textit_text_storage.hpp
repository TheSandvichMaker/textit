#ifndef TEXTIT_TEXT_STORAGE_HPP
#define TEXTIT_TEXT_STORAGE_HPP

struct TextStorage
{
    int64_t count;
    int64_t committed;
    int64_t capacity;
    uint8_t *text;
};

#endif /* TEXTIT_TEXT_STORAGE_HPP */
