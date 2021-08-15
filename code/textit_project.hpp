#ifndef TEXTIT_PROJECT_HPP
#define TEXTIT_PROJECT_HPP

struct Project
{
    Project *next;

    int associated_buffer_count;

    String root;
    Tag *tag_table[4096];
};

#endif /* TEXTIT_PROJECT_HPP */
