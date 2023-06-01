#include "freetypewrapper.hpp"

#include <math.h>
#include <limits>

#include "xlog.hpp"
#include "utf8_enc.hpp"

#define INT_TO_FP_26_6(integer) ((integer) * 64)
#define FP_26_6_TO_INT(FP26_6)  ((FP26_6) / 64)

typedef uint8_t uint8;
typedef uint32_t uint32;

#define MIN(a,b) std::min((a),(b))
#define MAX(a,b) std::max((a),(b))

// A simple 32-bit pixel.

struct Pixel32
{
    Pixel32(){}
    Pixel32(uint8 bi, uint8 gi, uint8 ri, uint8 ai = 255)
    {
        b = bi;
        g = gi;
        r = ri;
        a = ai;
    }

    // uint32 integer;
    uint8 a{0};
    uint8 r{0};
    uint8 g{0};
    uint8 b{0};
};

#if 0
union Pixel32
{
    Pixel32()
        : integer(0) {}
    Pixel32(uint8 bi, uint8 gi, uint8 ri, uint8 ai = 255)
    {
        b = bi;
        g = gi;
        r = ri;
        a = ai;
    }

    uint32 integer;

    struct
    {
#ifdef BIG_ENDIAN
        uint8 a, r, g, b;
#else  // BIG_ENDIAN
        uint8 b, g, r, a;
#endif // BIG_ENDIAN
    };
};
#endif 

struct Vec2
{
    Vec2() {}
    Vec2(float a, float b)
        : x(a), y(b) {}

    float x, y;
};

struct Rect
{
    Rect() {}
    Rect(float left, float top, float right, float bottom)
        : xmin(left), xmax(right), ymin(top), ymax(bottom) {}

    void Include(const Vec2 &r)
    {
        xmin = MIN(xmin, r.x);
        ymin = MIN(ymin, r.y);
        xmax = MAX(xmax, r.x);
        ymax = MAX(ymax, r.y);
    }

    float Width() const { return xmax - xmin + 1; }
    float Height() const { return ymax - ymin + 1; }

    float xmin, xmax, ymin, ymax;
};

// A horizontal pixel span generated by the FreeType renderer.

struct Span
{
    Span() {}
    Span(int _x, int _y, int _width, int _coverage)
        : x(_x), y(_y), width(_width), coverage(_coverage) {}

    int x, y, width, coverage;
};

typedef std::vector<Span> Spans;

// Each time the renderer calls us back we just push another span entry on
// our list.

void RasterCallback(const int y,
                    const int count,
                    const FT_Span *const spans,
                    void *const user)
{
    Spans *sptr = (Spans *)user;
    for (int i = 0; i < count; ++i)
        sptr->push_back(Span(spans[i].x, y, spans[i].len, spans[i].coverage));
}

// Set up the raster parameters and render the outline.

void RenderSpans(FT_Library &library,
                 FT_Outline *const outline,
                 Spans *spans)
{
    FT_Raster_Params params;
    memset(&params, 0, sizeof(params));
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.gray_spans = RasterCallback;
    params.user = spans;

    FT_Outline_Render(library, outline, &params);
}

FreeTypeWrapper::State::~State()
{
    if (ft_face)
    {
        FT_Done_Face(ft_face);
        ft_face = nullptr;
    }

    if (ft_library)
    {
        FT_Done_FreeType(ft_library);
        ft_library = nullptr;
    }
}

FreeTypeWrapper::FreeTypeWrapper(const std::string &font_path)
    :_font_path(font_path)
{
    _state = init();
}

FreeTypeWrapper::~FreeTypeWrapper()
{
    _state.reset();
}

int FreeTypeWrapper::drawString(const DrawInfo &info)
{
    int berror = false;

    do 
    {
        if (!info.utf8_str
            || !info.iv
            || info.x < 0
            || info.y < 0
            || info.font_size < 0
            || info.outline_width < 0.0)
        {
            xlog_err("inavlid arg");
            berror = true;
            break;
        }

        switch (info.mode)
        {
            case DrawMode::Normal:
            case DrawMode::Monochrome:
            {
                if (drawString_Normal_Monochrome(info) < 0)
                {
                    xlog_err("draw failed");
                    berror = true;
                }
                break;
            }
            case DrawMode::Outline:
            {
                if (drawString_Outline(info) < 0)
                {
                    xlog_err("draw failed");
                    berror = true;
                }
                break;
            }
            default:
            {
                xlog_err("unknown mode");
                berror = true;
                break;
            }
        }
    }
    while (0);

    return (berror ? -1 : 0);
}

int FreeTypeWrapper::drawString_Outline(const DrawInfo &info)
{
    if (!info.utf8_str || !info.iv)
    {
        return -1;
    }

    FT_Face &face = _state->ft_face;
    FT_Library &library = _state->ft_library;

    std::vector<uint32_t> utf32_str;
    utf32_str = enc_utf8_2_utf32(*info.utf8_str);

    double outlineWidth = info.outline_width;

    Pixel32 outlineCol = Pixel32(0, 0, 0);
    Pixel32 fontCol = Pixel32(255, 255, 255);
    
    int xoff = info.x;

    for (const auto & ref_utf32_c : utf32_str)
    {
        // Set the size to use.
        // if (FT_Set_Char_Size(face, size << 6, size << 6, 90, 90) == 0)
        if (FT_Set_Pixel_Sizes(_state->ft_face, 0, info.font_size) == 0)
        {
            // Load the glyph we are looking for.
            FT_UInt gindex = FT_Get_Char_Index(face, ref_utf32_c);
            if (FT_Load_Glyph(face, gindex, FT_LOAD_NO_BITMAP) == 0)
            {
                // Need an outline for this to work.
                if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                {
                    // Render the basic glyph to a span list.
                    Spans spans;
                    RenderSpans(library, &face->glyph->outline, &spans);

                    // Next we need the spans for the outline.
                    Spans outlineSpans;

                    // Set up a stroker.
                    FT_Stroker stroker;
                    FT_Stroker_New(library, &stroker);
                    FT_Stroker_Set(stroker,
                                (int)(outlineWidth * 64),
                                FT_STROKER_LINECAP_ROUND,
                                FT_STROKER_LINEJOIN_ROUND,
                                0);

                    FT_Glyph glyph;
                    if (FT_Get_Glyph(face->glyph, &glyph) == 0)
                    {
                        FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);
                        // Again, this needs to be an outline to work.
                        if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
                        {
                            // Render the outline spans to the span list
                            FT_Outline *o =
                                &reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
                            RenderSpans(library, o, &outlineSpans);
                        }

                        // Clean up afterwards.
                        FT_Stroker_Done(stroker);
                        FT_Done_Glyph(glyph);

                        // Now we need to put it all together.
                        if (!spans.empty())
                        {
                            // Figure out what the bounding rect is for both the span lists.
                            Rect rect(spans.front().x,
                                    spans.front().y,
                                    spans.front().x,
                                    spans.front().y);
                            for (Spans::iterator s = spans.begin();
                                s != spans.end(); ++s)
                            {
                                rect.Include(Vec2(s->x, s->y));
                                rect.Include(Vec2(s->x + s->width - 1, s->y));
                            }
                            for (Spans::iterator s = outlineSpans.begin();
                                s != outlineSpans.end(); ++s)
                            {
                                rect.Include(Vec2(s->x, s->y));
                                rect.Include(Vec2(s->x + s->width - 1, s->y));
                            }

                            // This is unused in this test but you would need this to draw
                            // more than one glyph.
                            float bearingX = face->glyph->metrics.horiBearingX >> 6;
                            float bearingY = face->glyph->metrics.horiBearingY >> 6;
                            float advance = face->glyph->advance.x >> 6;
                            float ascender = face->size->metrics.ascender >> 6;

                            // Get some metrics of our image.
                            int imgWidth = rect.Width(),
                                imgHeight = rect.Height(),
                                imgSize = imgWidth * imgHeight;

                            // Allocate data for our image and clear it out to transparent.
                            // Pixel32 *pxl = new Pixel32[imgSize];
                            // memset(pxl, 0xff, sizeof(Pixel32) * imgSize);
                            std::vector<Pixel32> pxl(imgSize);

                            // Loop over the outline spans and just draw them into the
                            // image.
                            for (Spans::iterator s = outlineSpans.begin(); s != outlineSpans.end(); ++s)
                            {
                                for (int w = 0; w < s->width; ++w)
                                {
                                    std::size_t offset = (imgHeight - 1 - (s->y - rect.ymin)) * imgWidth + s->x - rect.xmin + w;
                                    if (offset >= pxl.size())
                                    {
                                        xlog_err("offset error(%d,%d)", (int)offset, (int)pxl.size());
                                        continue;
                                    }
                                    pxl[offset] = Pixel32(outlineCol.r, outlineCol.g, outlineCol.b, s->coverage);
                                }
                            }

                            // Then loop over the regular glyph spans and blend them into
                            // the image.
                            for (Spans::iterator s = spans.begin(); s != spans.end(); ++s)
                            {
                                for (int w = 0; w < s->width; ++w)
                                {
                                    std::size_t offset = (imgHeight - 1 - (s->y - rect.ymin)) * imgWidth + s->x - rect.xmin + w;
                                    if (offset >= pxl.size())
                                    {
                                        xlog_err("offset error(%d,%d)", (int)offset, (int)pxl.size());
                                        continue;
                                    }
                                    Pixel32 &dst = pxl[offset];
                                    Pixel32 src = Pixel32(fontCol.r, fontCol.g, fontCol.b, s->coverage);
                                    dst.r = (int)(dst.r + ((src.r - dst.r) * src.a) / 255.0f);
                                    dst.g = (int)(dst.g + ((src.g - dst.g) * src.a) / 255.0f);
                                    dst.b = (int)(dst.b + ((src.b - dst.b) * src.a) / 255.0f);
                                    dst.a = MIN(255, dst.a + src.a);
                                }
                            }

                            // Dump the image to disk.
                            // WriteTGA(fileName, pxl, imgWidth, imgHeight);
                            {
                                xlog_trc("[%04x]: [w=%d,h=%d][bx=%d,by=%d,ad=%d,as=%d]",
                                    (unsigned int)ref_utf32_c,
                                    imgWidth, imgHeight,
                                    (int)bearingX,
                                    (int)bearingY,
                                    (int)advance,
                                    (int)ascender);

                                // here, 
                                // [255,255,255,255] -> forground
                                // [0,0,0,255] -> outline
                                // [x,x,x,0] -> background
                                for (int iy = 0; iy < imgHeight; ++iy)
                                {
                                    for (int ix = 0; ix < imgWidth; ++ix)
                                    {
                                        int offset = iy * imgWidth + ix;
                                        uint8_t color = pxl[offset].r;  // use red as color
                                        uint8_t alpha = pxl[offset].a;  //

                                        PixelColorPtr mid_fore_out = mid(info.outline, info.foreground, color);
                                        PixelColorPtr mid_foreout_back = mid(info.background, mid_fore_out, alpha);

                                        int realx = ix + xoff + bearingX;
                                        int realy = iy + info.y + ascender - bearingY;

                                        info.iv->drawPixels(realx, realy, *mid_foreout_back);
                                    }
                                }
                            }

                            // delete[] pxl;
                        }

                        // xoff += (face->glyph->advance.x >> 6) + ::ceil(2 * info.outline_width);
                        xoff += ((face->glyph->metrics.horiAdvance >> 6) + ::ceil(2 * info.outline_width));
                    }
                }
            }
        }
    }

    return 0;
}

int FreeTypeWrapper::drawString_Normal_Monochrome(const DrawInfo &info)
{
    int berror = false;
    FT_Error err{};
    int load_flags = 0;

    do
    {
        if (info.x < 0 || info.y < 0 || !info.iv || !info.utf8_str)
        {
            xlog_err("invalid args");
            berror = true;
            break;
        }

        if (!_state)
        {
            xlog_err("null");
            berror = true;
            break;
        }

        std::vector<uint32_t> utf32_str;

        utf32_str = enc_utf8_2_utf32(*info.utf8_str);

        err = FT_Set_Pixel_Sizes(_state->ft_face, 0, info.font_size);
        if (err)
        {
            xlog_err("FT_Set_Char_Size failed");
            berror = true;
            break;
        }

        FT_GlyphSlot slot = nullptr;
        FT_Vector pen = {0, 0};
        slot = _state->ft_face->glyph;

        // int iv_width = iv->width();
        int iv_height = info.iv->height();

        /* 注意：freetype 坐标系为笛卡尔坐标系 */
        
        pen.x = INT_TO_FP_26_6(info.x);
        pen.y = INT_TO_FP_26_6(iv_height - info.y);

        if (DrawMode::Normal == info.mode)
        {
            load_flags = FT_LOAD_RENDER;
        }
        else if (DrawMode::Monochrome == info.mode)
        {
            load_flags = FT_LOAD_RENDER | FT_LOAD_MONOCHROME;
        }
        else 
        {
            xlog_err("invalid mode");
            berror = true;
            break;
        }

        for (int i = 0; i < (int)utf32_str.size(); ++i)
        {
            FT_Set_Transform(_state->ft_face, nullptr, &pen);

            err = FT_Load_Char(_state->ft_face, utf32_str[i], load_flags);
            if (err)
            {
                xlog_err("FT_Load_Char failed");
                continue;
            }

            FT_Pos ascender = _state->ft_face->size->metrics.ascender >> 6;

            auto &metrics = _state->ft_face->size->metrics;

            xlog_trc("[left=%d,top=%d][ascender=%d, descender=%d, height=%d]",
                (int)slot->bitmap_left,
                (int)slot->bitmap_top,
                FP_26_6_TO_INT(metrics.ascender), 
                FP_26_6_TO_INT(metrics.descender),
                FP_26_6_TO_INT(metrics.height));

            /** 
             * 注：如果传入的图片高度为100，则bitmap_top为100+，即笛卡尔坐标系 
             * 下为图片左上角的上方。
             * 实际需要绘制到左上角的下方，因此作以下换算。
             * 实际测试，ascender的大小即为字符的高度。
             **/

            drawBitmap(info.iv, 
                slot->bitmap_left, 
                iv_height - 1 - slot->bitmap_top + ascender,
                &slot->bitmap, info.foreground, info.background);

            pen.x += slot->advance.x;
            pen.y += slot->advance.y;
        }
    }
    while (0);

    return (berror ? -1 : 0);
}

std::shared_ptr<FreeTypeWrapper::State> FreeTypeWrapper::init()
{
    std::shared_ptr<State> state;
    int berror = false;

    do
    {
        FT_Error err{};
        
        state = std::make_shared<State>();

        err = FT_Init_FreeType(&state->ft_library);
        if (err)
        {
            xlog_err("FT_Init_FreeType failed");
            berror = true;
            break;
        }

        err = FT_New_Face(state->ft_library, _font_path.c_str(), 0, &state->ft_face);
        if (err)
        {
            xlog_err("FT_New_Face failed");
            berror = true;
            break;
        }
    }
    while (0);

    if (berror)
    {
        state.reset();
    }

    return state;
}

/* 注：这里以左上角为原点 */
int FreeTypeWrapper::drawBitmap(std::shared_ptr<ImageView> iv, int x, int y, FT_Bitmap *bitmap, 
        std::shared_ptr<std::vector<uint8_t>> foreground,
        std::shared_ptr<std::vector<uint8_t>> background)
{
    xlog_trc("[x=%d,y=%d][w=%d, h=%d]", 
        x, y, 
        (int)bitmap->width, (int)bitmap->rows);

    int berror = false;

    if (!bitmap || !iv || !foreground || !background)
    {
        xlog_err("null");
        berror = true;
        return -1;
    }

    for (unsigned int j = 0; j < bitmap->rows; ++j)
    {
        for (unsigned int i = 0; i < bitmap->width; ++i)
        {
            int dst_x = x + i;
            int dst_y = y + j;

            if (dst_x < 0 || dst_x >= iv->width())
            {
                xlog_err("x over");
                continue;
            }

            if (dst_y < 0 || dst_y >= iv->height())
            {
                xlog_err("y over"); 
                continue;
            }

            uint8_t color = 0x0;

            switch (bitmap->pixel_mode)
            {
                case FT_PIXEL_MODE_MONO:
                {
                    int byte_index = (bitmap->pitch * j + i / 8);
                    int bit_index = i % 8;
                    if (bitmap->buffer[byte_index] & (0x80 >> bit_index))
                    {
                        color = 0xff;
                    }
                    else 
                    {
                        color = 0x0;
                    }
                    break;
                }
                case FT_PIXEL_MODE_GRAY:
                {
                    color = bitmap->buffer[j * bitmap->width + i];
                    break;
                }
                default:
                {
                    xlog_err("pixel mode not support");
                    berror = true;
                    break;
                }
            }

            if (berror)
            {
                break;
            }

            PixelColorPtr pixel = mid(background, foreground, color);
            if (!pixel)
            {
                xlog_err("mid failed");
                berror = true;
                break;
            }

            iv->drawPixels(dst_x, dst_y, *pixel);
        }
    }
    
    return (berror ? -1 : 0);
}

FreeTypeWrapper::PixelColorPtr FreeTypeWrapper::mid(PixelColorPtr background, PixelColorPtr foreground, uint8_t color)
{
    PixelColorPtr pixel;
    int berror = false;

    const std::uint8_t color_max = std::numeric_limits<uint8_t>().max();
    const std::uint8_t color_min = std::numeric_limits<uint8_t>().min();

    static_assert(color_max == 255, "unexpected");
    static_assert(color_min == 0, "unexpected");

    do 
    {
        if (!background || !foreground)
        {
            xlog_err("invalid arg");
            berror = true;
            break;
        }

        if (background->size() != foreground->size())
        {
            xlog_err("invalid arg");
            berror = true;
            break;
        }

        if (color_min == color)
        {
            pixel = background;
            break;
        }
        else if (color_max == color)
        {
            pixel = foreground;
            break;
        }

        std::size_t pixel_bytes = background->size();
        pixel = std::make_shared<PixelColor>(pixel_bytes);
        for (std::size_t i = 0; i < pixel_bytes; ++i)
        {
            int back = background->at(i);
            int fore = foreground->at(i);
            int middle = back + (fore - back) * (color - color_min) / (color_max - color_min);
            
            pixel->operator[](i) = (uint8_t)middle;
        }
    }
    while (0);

    return (berror ? nullptr : pixel);
}