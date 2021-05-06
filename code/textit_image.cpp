static Bitmap
ParseBitmap(String buffer)
{
    Bitmap result = {};
    
    size_t size = buffer.size;
    uint8_t *memory = buffer.data;
    
    if (size && memory)
    {
        BitmapHeader *header = (BitmapHeader *)memory;
        
        if (header->size < sizeof(header))
        {
            platform->ReportError(PlatformError_Nonfatal, "Bitmap parse error: header too small");
            return result;
        }
        
        if (header->height < 0)
        {
            platform->ReportError(PlatformError_Nonfatal, "Bitmap parse error: has to be bottom-up");
            return result;
        }
        
        if (header->compression != 3)
        {
            platform->ReportError(PlatformError_Nonfatal, "Bitmap parse error: must be uncompressed 32 bit");
            return result;
        }
        
        uint32_t *pixels = (uint32_t *)((uint8_t *)header + header->bitmap_offset);
        result.w = header->width;
        result.h = header->height;
        result.pitch = result.w;
        
        uint32_t alpha_mask = header->alpha_mask;
        uint32_t red_mask   = header->red_mask;
        uint32_t green_mask = header->green_mask;
        uint32_t blue_mask  = header->blue_mask;
        
        // TODO: Would the alpha mask ever not be present?
        if (!alpha_mask) {
            alpha_mask = ~(red_mask | green_mask | blue_mask);
        }
        
        BitScanResult alpha_scan = FindLeastSignificantSetBit(alpha_mask);
        BitScanResult red_scan   = FindLeastSignificantSetBit(red_mask);
        BitScanResult green_scan = FindLeastSignificantSetBit(green_mask);
        BitScanResult blue_scan  = FindLeastSignificantSetBit(blue_mask);
        
        Assert(alpha_scan.found && red_scan.found && green_scan.found && blue_scan.found);
        
        int32_t alpha_shift_down = (int32_t)alpha_scan.index;
        int32_t red_shift_down   = (int32_t)red_scan.index;
        int32_t green_shift_down = (int32_t)green_scan.index;
        int32_t blue_shift_down  = (int32_t)blue_scan.index;
        
        int32_t alpha_shift_up = 24;
        int32_t red_shift_up   = 16;
        int32_t green_shift_up =  8;
        int32_t blue_shift_up  =  0;
        
        uint32_t *source_dest = pixels;
        for (int32_t i = 0; i < header->width*header->height; ++i)
        {
            uint32_t c = *source_dest;
            float texel_r = (float)((c & red_mask) >> red_shift_down);
            float texel_g = (float)((c & green_mask) >> green_shift_down);
            float texel_b = (float)((c & blue_mask) >> blue_shift_down);
            float texel_a = (float)((c & alpha_mask) >> alpha_shift_down);
            
            float rcp_255 = 1.0f/255.0f;
            texel_a = rcp_255*texel_a;
            texel_r = 255.0f*SquareRoot(Square(rcp_255*texel_r)*texel_a);
            texel_g = 255.0f*SquareRoot(Square(rcp_255*texel_g)*texel_a);
            texel_b = 255.0f*SquareRoot(Square(rcp_255*texel_b)*texel_a);
            texel_a *= 255.0f;
            
            *source_dest++ = ((uint32_t)(texel_a + 0.5f) << alpha_shift_up |
                              (uint32_t)(texel_r + 0.5f) << red_shift_up   |
                              (uint32_t)(texel_g + 0.5f) << green_shift_up |
                              (uint32_t)(texel_b + 0.5f) << blue_shift_up);
        }
        
        result.data = (Color *)pixels;
    }
    
    return result;
}
