/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "decoration.h"
#include "decoration_p.h"
#include "decoratedclient.h"
#include "private/decoratedclientprivate.h"
#include "private/decorationbridge.h"
#include "decorationbutton.h"
#include "decorationsettings.h"
#include "decorationshadow.h"

#include <QCoreApplication>
#include <QHoverEvent>

namespace KDecoration2
{

namespace {
DecorationBridge *findBridge(const QVariantList &args)
{
    for (const auto &arg: args) {
        if (auto bridge = arg.toMap().value(QStringLiteral("bridge")).value<DecorationBridge*>()) {
            return bridge;
        }
    }
    Q_UNREACHABLE();
}
}

Decoration::Private::Private(Decoration *deco, const QVariantList &args)
    : sectionUnderMouse(Qt::NoSection)
    , bridge(findBridge(args))
    , client(QSharedPointer<DecoratedClient>(new DecoratedClient(deco, bridge)))
    , opaque(false)
    , q(deco)
{
    Q_UNUSED(args)
}

void Decoration::Private::setSectionUnderMouse(Qt::WindowFrameSection section)
{
    if (sectionUnderMouse == section) {
        return;
    }
    sectionUnderMouse = section;
    emit q->sectionUnderMouseChanged(sectionUnderMouse);
}

void Decoration::Private::updateSectionUnderMouse(const QPoint &mousePosition)
{
    if (titleBar.contains(mousePosition)) {
        setSectionUnderMouse(Qt::TitleBarArea);
        return;
    }
    const QSize size = q->size();
    const int corner = 2*settings->largeSpacing();
    const bool left   = mousePosition.x() < borders.left();
    const bool top    = mousePosition.y() < borders.top();
    const bool bottom = size.height() - mousePosition.y() <= borders.bottom();
    const bool right  = size.width() - mousePosition.x() <= borders.right();
    if (left) {
        if (top && mousePosition.y() < titleBar.top() + corner) {
            setSectionUnderMouse(Qt::TopLeftSection);
        } else if (size.height() - mousePosition.y() <= borders.bottom() + corner && mousePosition.y() > titleBar.bottom()) {
            setSectionUnderMouse(Qt::BottomLeftSection);
        } else {
            setSectionUnderMouse(Qt::LeftSection);
        }
        return;
    }
    if (right) {
        if (top && mousePosition.y() < titleBar.top() + corner) {
            setSectionUnderMouse(Qt::TopRightSection);
        } else if (size.height() - mousePosition.y() <= borders.bottom() + corner && mousePosition.y() > titleBar.bottom()) {
            setSectionUnderMouse(Qt::BottomRightSection);
        } else {
            setSectionUnderMouse(Qt::RightSection);
        }
        return;
    }
    if (bottom) {
        if (mousePosition.y() > titleBar.bottom()) {
            if (mousePosition.x() < borders.left() + corner) {
                setSectionUnderMouse(Qt::BottomLeftSection);
            } else if (size.width() - mousePosition.x() <= borders.right() + corner) {
                setSectionUnderMouse(Qt::BottomRightSection);
            } else {
                setSectionUnderMouse(Qt::BottomSection);
            }
        } else {
            setSectionUnderMouse(Qt::TitleBarArea);
        }
        return;
    }
    if (top) {
        if (mousePosition.y() < titleBar.top()) {
            if (mousePosition.x() < borders.left() + corner) {
                setSectionUnderMouse(Qt::TopLeftSection);
            } else if (size.width() - mousePosition.x() <= borders.right() + corner) {
                setSectionUnderMouse(Qt::TopRightSection);
            } else {
                setSectionUnderMouse(Qt::TopSection);
            }
        } else {
            setSectionUnderMouse(Qt::TitleBarArea);
        }
        return;
    }
    setSectionUnderMouse(Qt::NoSection);
}

void Decoration::Private::addButton(DecorationButton *button)
{
    Q_ASSERT(!buttons.contains(button));
    buttons << button;
    QObject::connect(button, &QObject::destroyed, q,
        [this](QObject *o) {
            auto it = buttons.begin();
            while (it != buttons.end()) {
                if (*it == static_cast<DecorationButton*>(o)) {
                    it = buttons.erase(it);
                } else {
                    it++;
                }
            }
        }
    );
}

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : QObject(parent)
    , d(new Private(this, args))
{
    connect(this, &Decoration::bordersChanged, this, [this]{ update(); });
}

Decoration::~Decoration() = default;

void Decoration::init()
{
    Q_ASSERT(!d->settings.isNull());
}

QWeakPointer<DecoratedClient> Decoration::client() const
{
    return d->client.toWeakRef();
}

#define DELEGATE(name) \
void Decoration::name() \
{ \
    d->client->d->name(); \
}

DELEGATE(requestClose)
DELEGATE(requestContextHelp)
DELEGATE(requestMinimize)
DELEGATE(requestToggleOnAllDesktops)
DELEGATE(requestToggleShade)
DELEGATE(requestToggleKeepAbove)
DELEGATE(requestToggleKeepBelow)
DELEGATE(requestShowWindowMenu)

#undef DELEGATE

void Decoration::requestToggleMaximization(Qt::MouseButtons buttons)
{
    d->client->d->requestToggleMaximization(buttons);
}

#define DELEGATE(name, variableName, type, emitValue) \
void Decoration::name(type a) \
{ \
    if (d->variableName == a) { \
        return; \
    } \
    d->variableName = a; \
    emit variableName##Changed(emitValue); \
}

DELEGATE(setBorders, borders, const QMargins&, )
DELEGATE(setResizeOnlyBorders, resizeOnlyBorders, const QMargins&, )
DELEGATE(setTitleBar, titleBar, const QRect&, )
DELEGATE(setOpaque, opaque, bool, d->opaque)
DELEGATE(setShadow, shadow, const QSharedPointer<DecorationShadow> &, d->shadow)

#undef DELEGATE

#define DELEGATE(name, type) \
type Decoration::name() const \
{ \
    return d->name; \
}

DELEGATE(borders, QMargins)
DELEGATE(resizeOnlyBorders, QMargins)
DELEGATE(titleBar, QRect)
DELEGATE(sectionUnderMouse, Qt::WindowFrameSection)
DELEGATE(shadow, QSharedPointer<DecorationShadow>)

#undef DELEGATE

bool Decoration::isOpaque() const
{
    return d->opaque;
}

#define BORDER(name, Name) \
int Decoration::border##Name() const \
{ \
    return d->borders.name(); \
} \
int Decoration::resizeOnlyBorder##Name() const \
{ \
    return d->resizeOnlyBorders.name(); \
}

BORDER(left, Left)
BORDER(right, Right)
BORDER(top, Top)
BORDER(bottom, Bottom)
#undef BORDER

QSize Decoration::size() const
{
    const QMargins &b = d->borders;
    return QSize(d->client->width() + b.left() + b.right(),
                 d->client->height() + b.top() + b.bottom());
}

QRect Decoration::rect() const
{
    return QRect(QPoint(0, 0), size());
}

bool Decoration::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        hoverEnterEvent(static_cast<QHoverEvent*>(event));
        return true;
    case QEvent::HoverLeave:
        hoverLeaveEvent(static_cast<QHoverEvent*>(event));
        return true;
    case QEvent::HoverMove:
        hoverMoveEvent(static_cast<QHoverEvent*>(event));
        return true;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent*>(event));
        return true;
    default:
        return QObject::event(event);
    }
}

void Decoration::hoverEnterEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        QCoreApplication::instance()->sendEvent(button, event);
    }
    d->updateSectionUnderMouse(event->pos());
}

void Decoration::hoverLeaveEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        QCoreApplication::instance()->sendEvent(button, event);
    }
    d->setSectionUnderMouse(Qt::NoSection);
}

void Decoration::hoverMoveEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (!button->isEnabled() || !button->isVisible()) {
            continue;
        }
        const bool hovered = button->isHovered();
        const bool contains = button->geometry().contains(event->pos());
        if (!hovered && contains) {
            QHoverEvent e(QEvent::HoverEnter, event->posF(), event->oldPosF(), event->modifiers());
            QCoreApplication::instance()->sendEvent(button, &e);
        } else if (hovered && !contains) {
            QHoverEvent e(QEvent::HoverLeave, event->posF(), event->oldPosF(), event->modifiers());
            QCoreApplication::instance()->sendEvent(button, &e);
        } else if (hovered && contains) {
            QCoreApplication::instance()->sendEvent(button, event);
        }
    }
    d->updateSectionUnderMouse(event->pos());
}

void Decoration::mouseMoveEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isPressed()) {
            QCoreApplication::instance()->sendEvent(button, event);
            return;
        }
    }
    // not handled, take care ourselves
}

void Decoration::mousePressEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isHovered()) {
            if (button->acceptedButtons().testFlag(event->button())) {
                QCoreApplication::instance()->sendEvent(button, event);
            }
            event->setAccepted(true);
            return;
        }
    }
}

void Decoration::mouseReleaseEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isPressed() && button->acceptedButtons().testFlag(event->button())) {
            QCoreApplication::instance()->sendEvent(button, event);
            return;
        }
    }
    // not handled, take care ourselves
    d->updateSectionUnderMouse(event->pos());
}

void Decoration::wheelEvent(QWheelEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->geometry().contains(event->posF())) {
            QCoreApplication::instance()->sendEvent(button, event);
            event->setAccepted(true);
        }
    }
}

void Decoration::update(const QRect &r)
{
    d->bridge->update(this, r.isNull() ? rect() : r);
}

void Decoration::update()
{
    update(QRect());
}

void Decoration::setSettings(const QSharedPointer< DecorationSettings > &settings)
{
    d->settings = settings;
}

QSharedPointer< DecorationSettings > Decoration::settings() const
{
    return d->settings;
}

} // namespace
