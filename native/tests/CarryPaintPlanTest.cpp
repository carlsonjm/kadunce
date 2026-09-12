/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CarryPaintPlan.h"
#include "NativeLanding.h"
#include <cassert>
#include <limits>
using namespace Kadunce;
int main()
{
    const QRectF pickup(10,10,1000,700), tablet(0,0,1280,800), monitor(1280,0,1920,1080);
    auto a = carryPaintPlan(pickup,{1100,50},tablet);
    auto b = carryPaintPlan(pickup,{1100,50},monitor);
    assert(a && b && a->target == b->target);
    assert(a->target.size() == pickup.size());
    assert(a->clip == QRectF(1100,50,180,700));
    assert(b->clip == QRectF(1280,50,820,700));
    assert(a->clip.intersected(b->clip).isEmpty());
    auto left = carryPaintPlan(pickup,{-100.25,-20.5},QRectF(-1920,-200,1920,1080));
    assert(left && left->target.topLeft() == QPointF(-100.25,-20.5));
    assert(left->target.size() == pickup.size());
    auto outside = carryPaintPlan(pickup,{4000,0},tablet);
    assert(outside && outside->clip.isEmpty());
    assert(!carryPaintPlan(QRectF(),{},tablet));
    assert(!carryPaintPlan(pickup,{},QRectF()));
    assert(!carryPaintPlan(pickup,{std::numeric_limits<double>::infinity(),0},tablet));
    assert(pickup == QRectF(10,10,1000,700));
    const QRectF area(1280,0,1280,750);
    const auto landed = safeNativeLanding(QRectF(1700,720,700,500), area);
    assert(landed == QRectF(1700,240,700,500));
    assert(safeNativeLanding(landed, area) == landed);
    const auto oversized = safeNativeLanding(QRectF(1500,700,1500,1000), area);
    assert(oversized.topLeft() == QPointF(1290,10));
    assert(oversized.size() == QSizeF(1500,1000));
}
