#pragma once

#include "image_slice.hpp"

// Common text rendering library context for multiple TextLayout objects.
// You typically need only one; you should use the one in
// globals->textRenderContext.
class TextRenderContext {
SHARED_ONLY_CLASS(TextRenderContext);
public:
    TextRenderContext(CKey);
    ~TextRenderContext();

private:
    struct Impl;
    unique_ptr<Impl> impl_;

    friend class TextLayout;
};

class TextLayout {
SHARED_ONLY_CLASS(TextLayout);
public:
    TextLayout(CKey, shared_ptr<TextRenderContext> ctx);

    // Uses the global context globals->textRenderContext
    TextLayout(CKey);
    ~TextLayout();

    // Set the text to be laid out. Must be valid UTF-8.
    void setText(const string& text);

    const string& text();

    // The logical size of the current text when rendered.
    int width();
    int height();

    // Render the text with color (r, g, b) to given image slice. The
    // coordinates (x, y) offset the position of the text. If both are zero, the
    // top left corners of the logical text rectangle and the image slice are
    // aligned.
    void render(ImageSlice dest, int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void render(ImageSlice dest, int x, int y, uint8_t rgb = 0);

private:
    struct Impl;
    unique_ptr<Impl> impl_;
};
