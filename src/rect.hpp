#pragma once

#include "common.hpp"

// Rectangle [startX, endX) x [startY, endY). Empty rectangle is represented by
// [0, 0) x [0, 0)
struct Rect {
    int startX;
    int endX;
    int startY;
    int endY;

    Rect() {
        startX = 0;
        endX = 0;
        startY = 0;
        endY = 0;
    }
    Rect(int pStartX, int pEndX, int pStartY, int pEndY) {
        startX = pStartX;
        endX = pEndX;
        startY = pStartY;
        endY = pEndY;

        if(startX >= endX || startY >= endY) {
            startX = 0;
            endX = 0;
            startY = 0;
            endY = 0;
        }
    }

    bool isEmpty() const {
        return startX >= endX || startY >= endY;
    }

    static Rect translate(Rect rect, int dx, int dy) {
        return Rect(
            rect.startX + dx,
            rect.endX + dx,
            rect.startY + dy,
            rect.endY + dy
        );
    }

    static Rect intersection(Rect rect1, Rect rect2) {
        return Rect(
            max(rect1.startX, rect2.startX),
            min(rect1.endX, rect2.endX),
            max(rect1.startY, rect2.startY),
            min(rect1.endY, rect2.endY)
        );
    }
};
