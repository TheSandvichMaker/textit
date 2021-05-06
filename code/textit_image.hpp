#ifndef TEXTIT_IMAGE_HPP
#define TEXTIT_IMAGE_HPP

#pragma pack(push, 1)
struct BitmapHeader
{
    uint16_t file_type;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t bitmap_offset;
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    
    uint32_t compression;
    uint32_t size_of_bitmap;
    int32_t horz_resolution;
    int32_t vert_resolution;
    uint32_t colors_used;
    uint32_t colors_important;
    
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
};
#pragma pack(pop)

#endif /* TEXTIT_IMAGE_HPP */
