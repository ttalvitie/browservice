#pragma once

#include "image_slice.hpp"

namespace browservice {

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
    void setText(string text);

    string text();

    // The logical size of the current text when rendered.
    int width();
    int height();

    // Get the byte index of the character boundary closest to given X
    // coordinate.
    int xCoordToIndex(int x);

    // Return the x coordinate of a character boundary given as byte index.
    int indexToXCoord(int idx);

    // Returns the previous/next visual character boundary from given byte
    // index. The movement is clamped to the beginning/end indices.
    int visualMoveIdx(int idx, bool forward);

    // Render the text with color (r, g, b) to given image slice. The
    // coordinates (x, y) offset the position of the text. If both are zero, the
    // bottom left corners of the logical text rectangle and the image slice are
    // aligned.
    void render(ImageSlice dest, int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void render(ImageSlice dest, int x, int y, uint8_t rgb = 0);

private:
    struct Impl;
    unique_ptr<Impl> impl_;
};

class OverflowTextLayout {
SHARED_ONLY_CLASS(OverflowTextLayout);
public:
    OverflowTextLayout(CKey, shared_ptr<TextRenderContext> ctx);

    // Uses the global context globals->textRenderContext
    OverflowTextLayout(CKey);

    void setText(string text);
    string text();

    // Set/get the width to which the text is clamped
    void setWidth(int width);
    int width();

    // The logical size of text without clamping
    int textWidth();
    int textHeight();

    // Set/get the current text offset (nonnegative number, clamped to suitable
    // range)
    void setOffset(int offset);
    int offset();

    // Adjust the offset such that given character boundary (given as byte
    // index) is visible
    void makeVisible(int idx);

    // Adaptations of TextLayout functions that take offset into account
    int xCoordToIndex(int x);
    int indexToXCoord(int idx);

    int visualMoveIdx(int idx, bool forward);

    void render(ImageSlice dest, uint8_t r, uint8_t g, uint8_t b);
    void render(ImageSlice dest, uint8_t rgb = 0);

private:
    void clampOffset_();

    shared_ptr<TextLayout> textLayout_;
    int width_;
    int offset_;
};

}
