#include "find_bar.hpp"

namespace {

vector<string> downIconPattern = {
    "................",
    "................",
    "................",
    "................",
    "......55555.....",
    "......5XXX3.....",
    "......5X##3.....",
    "......5X##3.....",
    "......5X##3.....",
    "......5X##3.....",
    "...4555X##3553..",
    "....4XXX###s2...",
    ".....4####s2....",
    "......4##s2.....",
    ".......4s2......",
    "........3.......",
    "................",
    "................",
    "................"
};

vector<string> upIconPattern = {
    "................",
    "................",
    "................",
    "................",
    "........4.......",
    ".......5X3......",
    "......5X##3.....",
    ".....5X####3....",
    "....5X######3...",
    "...4444###3333..",
    "......5X##3.....",
    "......5X##3.....",
    "......5X##3.....",
    "......5X##3.....",
    "......5X##3.....",
    "......53333.....",
    "................",
    "................",
    "................"
};

vector<string> closeIconPattern = {
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....3.....3....",
    "....3X2...3X2...",
    "...3Xx#2.3X##2..",
    "....2###3X##1...",
    ".....2#####1....",
    "......3###2.....",
    ".....3X####2....",
    "....3X##2###2...",
    "...3X##1.2###2..",
    "....2#1...2#1...",
    ".....2.....2....",
    "................",
    "................",
    "................"
};

map<char, array<uint8_t, 3>> activeArrowColors = {
    {'.', {192, 192, 192}},
    {'#', {232, 232, 0}},
    {'X', {255, 255, 64}},
    {'5', {102, 102, 0}},
    {'4', {68, 68, 0}},
    {'3', {42, 42, 0}},
    {'2', {0, 0, 0}},
    {'s', {198, 198, 0}}
};

map<char, array<uint8_t, 3>> passiveArrowColors = {
    {'.', {192, 192, 192}},
    {'#', {204, 204, 204}},
    {'X', {224, 224, 224}},
    {'5', {90, 90, 90}},
    {'4', {56, 56, 56}},
    {'3', {34, 34, 34}},
    {'2', {0, 0, 0}},
    {'s', {174, 174, 174}}
};

MenuButtonIcon downIcon = {
    ImageSlice::createImageFromStrings(
        downIconPattern,
        activeArrowColors
    ),
    ImageSlice::createImageFromStrings(
        downIconPattern,
        passiveArrowColors
    )
};

MenuButtonIcon upIcon = {
    ImageSlice::createImageFromStrings(
        upIconPattern,
        activeArrowColors
    ),
    ImageSlice::createImageFromStrings(
        upIconPattern,
        passiveArrowColors
    )
};

MenuButtonIcon closeIcon = {
    ImageSlice::createImageFromStrings(
        closeIconPattern,
        {
            {'.', {192, 192, 192}},
            {'#', {255, 128, 128}},
            {'X', {255, 176, 176}},
            {'x', {255, 144, 144}},
            {'1', {0, 0, 0}},
            {'2', {46, 24, 24}},
            {'3', {90, 44, 44}}
        }
    ),
    ImageSlice::createImageFromStrings(
        closeIconPattern,
        {
            {'.', {192, 192, 192}},
            {'#', {204, 204, 204}},
            {'X', {224, 224, 224}},
            {'x', {224, 210, 210}},
            {'1', {0, 0, 0}},
            {'2', {40, 40, 40}},
            {'3', {80, 80, 80}}
        }
    )
};

const int BtnWidth = 19;

}

FindBar::FindBar(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<FindBarEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    requireUIThread();

    eventHandler_ = eventHandler;

    isOpen_ = false;
}

void FindBar::open() {
    requireUIThread();

    if(!isOpen_) {
        isOpen_ = true;
        text_.reset();
        textField_->setText("");
        lastDirForward_ = true;
    }
}

void FindBar::close() {
    requireUIThread();

    if(isOpen_) {
        isOpen_ = false;
        postTask(eventHandler_, &FindBarEventHandler::onStopFind, false);
        postTask(eventHandler_, &FindBarEventHandler::onFindBarClose);
    }
}

void FindBar::activate() {
    requireUIThread();
    textField_->activate();
}

void FindBar::findNext() {
    requireUIThread();

    if(isOpen_) {
        find_(textField_->text(), lastDirForward_);
    }
}

void FindBar::onTextFieldTextChanged() {
    requireUIThread();
    if(!isOpen_) return;

    updateText_(textField_->text());
}

void FindBar::onTextFieldSubmitted(string text) {
    requireUIThread();
    if(!isOpen_) return;

    find_(text, lastDirForward_);
}

void FindBar::onTextFieldEscKeyDown() {
    requireUIThread();
    close();
}

void FindBar::onMenuButtonPressed(weak_ptr<MenuButton> button) {
    requireUIThread();

    if(button.lock() == closeButton_) {
        close();
    }

    if(isOpen_) {
        if(button.lock() == downButton_) {
            find_(textField_->text(), true);
        }
        if(button.lock() == upButton_) {
            find_(textField_->text(), false);
        }
    }
}

void FindBar::onMenuButtonEnterKeyDown() {
    requireUIThread();

    if(isOpen_) {
        find_(textField_->text(), lastDirForward_);
    }
}

void FindBar::onMenuButtonEscKeyDown() {
    requireUIThread();
    close();
}

void FindBar::afterConstruct_(shared_ptr<FindBar> self) {
    textField_ = TextField::create(self, self);
    textField_->setRemoveCaretOnSubmit(false);

    downButton_ = MenuButton::create(downIcon, self, self);
    upButton_ = MenuButton::create(upIcon, self, self);
    closeButton_ = MenuButton::create(closeIcon, self, self);
}

bool FindBar::updateText_(string text) {
    CHECK(isOpen_);

    if(text.empty()) {
        if(text_) {
            postTask(eventHandler_, &FindBarEventHandler::onStopFind, true);
            text_.reset();
        }
        return true;
    } else {
        if(text_ && *text_ == text) {
            return false;
        } else {
            postTask(
                eventHandler_,
                &FindBarEventHandler::onFind,
                text,
                true,
                false
            );
            text_ = text;
            return true;
        }
    }
}

void FindBar::find_(string text, bool forward) {
    CHECK(isOpen_);

    lastDirForward_ = forward;

    if(!updateText_(text)) {
        postTask(
            eventHandler_,
            &FindBarEventHandler::onFind,
            text,
            forward,
            true
        );
    }
}

void FindBar::widgetViewportUpdated_() {
    requireUIThread();

    ImageSlice viewport = getViewport();

    textField_->setViewport(
        viewport.subRect(4, Width - 3 * BtnWidth - 4, 2, Height - 4)
    );
    downButton_->setViewport(
        viewport.subRect(Width - 3 * BtnWidth, Width - 2 * BtnWidth, 0, Height)
    );
    upButton_->setViewport(
        viewport.subRect(Width - 2 * BtnWidth, Width - BtnWidth, 0, Height)
    );
    closeButton_->setViewport(
        viewport.subRect(Width - BtnWidth, Width, 0, Height)
    );
}

void FindBar::widgetRender_() {
    requireUIThread();

    // Text field border and background
    ImageSlice viewport =
        getViewport().subRect(0, Width - 3 * BtnWidth, 0, Height);
    int width = viewport.width();
    viewport.fill(0, width - 1, 0, 1, 128);
    viewport.fill(0, 1, 1, Height - 1, 128);
    viewport.fill(0, width, Height - 1, Height, 255);
    viewport.fill(width - 1, width, 0, Height - 1, 255);
    viewport.fill(1, width - 2, 1, 2, 0);
    viewport.fill(1, 2, 2, Height - 2, 0);
    viewport.fill(1, width - 1, Height - 2, Height - 1, 192);
    viewport.fill(width - 2, width - 1, 1, Height - 2, 192);
    viewport.fill(2, width - 2, 2, Height - 2, 255);
}

vector<shared_ptr<Widget>> FindBar::widgetListChildren_() {
    return {textField_, downButton_, upButton_, closeButton_};
}
