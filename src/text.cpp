#include "text.hpp"

#include "globals.hpp"
#include "rect.hpp"

#include <freetype/fttypes.h>
#include <pango/pangoft2.h>

namespace {

// FreeType2 TrueType interpreter version can only be set using an environment
// variable, so we set it temporarily using this class
class FreeType2SetEnv {
public:
    FreeType2SetEnv() {
        char* oldValuePtr = getenv("FREETYPE_PROPERTIES");
        hasOldValue_ = oldValuePtr != nullptr;
        if(hasOldValue_) {
            oldValue_ = oldValuePtr;
        }

        CHECK(!setenv("FREETYPE_PROPERTIES", "truetype:interpreter-version=35", true));
    }

    ~FreeType2SetEnv() {
        if(hasOldValue_) {
            CHECK(!setenv("FREETYPE_PROPERTIES", oldValue_.c_str(), true));
        } else {
            CHECK(!unsetenv("FREETYPE_PROPERTIES"));
        }
    }

private:
    bool hasOldValue_;
    string oldValue_;
};

}

struct TextRenderContext::Impl {
    PangoFontMap* fontMap;
    PangoContext* pangoCtx;
    PangoFontDescription* fontDesc;

    Impl() {
        // Set interpreter version environment variable
        FreeType2SetEnv setEnv;

        fontMap = pango_ft2_font_map_new();
        CHECK(fontMap != nullptr);

        pango_ft2_font_map_set_default_substitute(
            (PangoFT2FontMap*)fontMap,
            [](FcPattern* pattern, void*) {
                FcValue value;
                value.type = FcTypeBool;
                value.u.b = FcFalse;
                FcPatternAdd(pattern, "antialias", value, FcFalse);
                value.u.b = FcFalse;
                FcPatternAdd(pattern, "autohint", value, FcFalse);
                value.u.b = FcTrue;
                FcPatternAdd(pattern, "hinting", value, FcFalse);
            },
            nullptr,
            [](void*) {}
        );

        pango_ft2_font_map_set_resolution((PangoFT2FontMap*)fontMap, 72, 72);

        pangoCtx = pango_font_map_create_context(fontMap);
        CHECK(pangoCtx != nullptr);

        pango_context_set_base_dir(pangoCtx, PANGO_DIRECTION_LTR);
        pango_context_set_language(pangoCtx, pango_language_from_string("en-US"));

        fontDesc = pango_font_description_from_string("Verdana 11");
        CHECK(fontDesc != nullptr);
    }

    ~Impl() {
        pango_font_description_free(fontDesc);
        g_object_unref(pangoCtx);
        g_object_unref(fontMap);
    }
};

namespace {

struct Graymap {
    int width;
    int height;
    vector<uint8_t> buffer;
    FT_Bitmap ftBitmap;

    Graymap(int pWidth, int pHeight) {
        width = pWidth;
        height = pHeight;

        CHECK(width >= 1);
        CHECK(height >= 1);

        const int Limit = INT_MAX / 9;
        CHECK(width < Limit / height);

        buffer.resize(width * height);
        ftBitmap.rows = height;
        ftBitmap.width = width;
        ftBitmap.pitch = width;
        ftBitmap.buffer = buffer.data();
        ftBitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    }

    Graymap(const Graymap&) = delete;
    Graymap& operator=(const Graymap&) = delete;

    Graymap(Graymap&&) = default;
    Graymap& operator=(Graymap&&) = default;
};

}

struct TextLayout::Impl {
    shared_ptr<TextRenderContext> ctx;
    PangoLayout* layout;

    string text;

    optional<Graymap> graymap;

    Impl(shared_ptr<TextRenderContext> ctx) : ctx(ctx) {
        layout = pango_layout_new(ctx->impl_->pangoCtx);
        CHECK(layout != nullptr);

        pango_layout_set_font_description(layout, ctx->impl_->fontDesc);
        pango_layout_set_auto_dir(layout, FALSE);
        pango_layout_set_single_paragraph_mode(layout, TRUE);
    }

    ~Impl() {
        g_object_unref(layout);
    }

    void setText(const string& newText) {
        graymap.reset();

        pango_layout_set_text(layout, newText.data(), newText.size());

        // Check that Pango agrees that the text is valid UTF-8
        text = pango_layout_get_text(layout);
        CHECK(text == newText);
    }

    void render(
        ImageSlice dest,
        int offsetX, int offsetY,
        uint8_t r, uint8_t g, uint8_t b
    ) {
        ensureGraymapRendered();

        Rect rect = Rect::intersection(
            Rect(0, graymap->width, 0, graymap->height),
            Rect::translate(
                Rect(0, dest.width(), 0, dest.height()),
                -offsetX, -offsetY
            )
        );

        if(!rect.isEmpty()) {
            for(int y = rect.startY; y < rect.endY; ++y) {
                uint8_t* graymapPos =
                    &graymap->buffer[y * graymap->width + rect.startX];
                uint8_t* destPos =
                    dest.getPixelPtr(rect.startX + offsetX, y + offsetY);

                for(int x = rect.startX; x < rect.endX; ++x) {
                    if(*graymapPos >= 128) {
                        *(destPos + 0) = b;
                        *(destPos + 1) = g;
                        *(destPos + 2) = r;
                    }
                    ++graymapPos;
                    destPos += 4;
                }
            }
        }
    }

    PangoRectangle getExtents() {
        PangoRectangle extents;
        pango_layout_get_pixel_extents(layout, nullptr, &extents);
        extents.width = max(extents.width, 1);
        extents.height = max(extents.height, 1);
        return extents;
    }

    void ensureGraymapRendered() {
        if(graymap) return;

        PangoRectangle extents = getExtents();
        graymap = Graymap(extents.width, extents.height);
        pango_ft2_render_layout(&graymap->ftBitmap, layout, -extents.x, -extents.y);
    }
};

TextRenderContext::TextRenderContext(CKey) {
    impl_ = make_unique<Impl>();
}

TextRenderContext::~TextRenderContext() {}

TextLayout::TextLayout(CKey, shared_ptr<TextRenderContext> ctx) {
    CEF_REQUIRE_UI_THREAD();
    impl_ = make_unique<Impl>(ctx);
}

TextLayout::TextLayout(CKey) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(globals);

    impl_ = make_unique<Impl>(globals->textRenderContext);
}

TextLayout::~TextLayout() {}

void TextLayout::setText(const string& text) {
    CEF_REQUIRE_UI_THREAD();
    impl_->setText(text);
}

const string& TextLayout::text() {
    CEF_REQUIRE_UI_THREAD();
    return impl_->text;
}

int TextLayout::width() {
    CEF_REQUIRE_UI_THREAD();
    return impl_->getExtents().width;
}

int TextLayout::height() {
    CEF_REQUIRE_UI_THREAD();
    return impl_->getExtents().height;
}

void TextLayout::render(
    ImageSlice dest, int x, int y, uint8_t r, uint8_t g, uint8_t b
) {
    CEF_REQUIRE_UI_THREAD();
    impl_->render(dest, x, y, r, g, b);
}

void TextLayout::render(
    ImageSlice dest, int x, int y, uint8_t rgb
) {
    render(dest, x, y, rgb, rgb, rgb);
}

OverflowTextLayout::OverflowTextLayout(CKey, shared_ptr<TextRenderContext> ctx) {
    CEF_REQUIRE_UI_THREAD();

    textLayout_ = TextLayout::create(ctx);
    width_ = 0;
    offset_ = 0;
}

OverflowTextLayout::OverflowTextLayout(CKey) {
    CEF_REQUIRE_UI_THREAD();

    textLayout_ = TextLayout::create();
    width_ = 0;
    offset_ = 0;
}

void OverflowTextLayout::setText(const string& text) {
    CEF_REQUIRE_UI_THREAD();

    textLayout_->setText(text);
    clampOffset_();
}

const string& OverflowTextLayout::text() {
    return textLayout_->text();
}

void OverflowTextLayout::setWidth(int width) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(width >= 0);

    width_ = width;
    clampOffset_();
}

int OverflowTextLayout::width() {
    CEF_REQUIRE_UI_THREAD();
    return width_;
}

int OverflowTextLayout::textWidth() {
    CEF_REQUIRE_UI_THREAD();
    return textLayout_->width();
}

int OverflowTextLayout::textHeight() {
    CEF_REQUIRE_UI_THREAD();
    return textLayout_->height();
}

void OverflowTextLayout::setOffset(int offset) {
    CEF_REQUIRE_UI_THREAD();

    offset_ = offset;
    clampOffset_();
}

int OverflowTextLayout::offset() {
    CEF_REQUIRE_UI_THREAD();
    return offset_;
}

void OverflowTextLayout::render(ImageSlice dest, uint8_t r, uint8_t g, uint8_t b) {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice subDest = dest.subRect(0, width_, 0, dest.height());
    textLayout_->render(subDest, -offset_, 0, r, g, b);
}

void OverflowTextLayout::render(ImageSlice dest, uint8_t rgb) {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice subDest = dest.subRect(0, width_, 0, dest.height());
    textLayout_->render(subDest, -offset_, 0, rgb);
}

void OverflowTextLayout::clampOffset_() {
    offset_ = min(offset_, textWidth() - width_);
    offset_ = max(offset_, 0);
}
