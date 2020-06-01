#include "find_bar.hpp"

FindBar::FindBar(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<FindBarEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    requireUIThread();

    eventHandler_ = eventHandler;
}

void FindBar::onMenuButtonPressed(weak_ptr<MenuButton> button) {
    requireUIThread();

    if(button.lock() == closeButton_) {
        postTask(eventHandler_, &FindBarEventHandler::onFindBarClose);
    }
}

void FindBar::afterConstruct_(shared_ptr<FindBar> self) {
    textField_ = TextField::create(self, self);
    downButton_ = MenuButton::create(EmptyIcon, self, self);
    upButton_ = MenuButton::create(EmptyIcon, self, self);
    closeButton_ = MenuButton::create(EmptyIcon, self, self);
}

void FindBar::widgetViewportUpdated_() {
    requireUIThread();

    ImageSlice viewport = getViewport();

    const int BtnWidth = MenuButton::Width;
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
        getViewport().subRect(0, Width - 3 * MenuButton::Width, 0, Height);
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
