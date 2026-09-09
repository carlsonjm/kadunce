/*
    SPDX-FileCopyrightText: 2026 Warbler Studio contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Effect.h"

namespace Kadunce
{
Q_NAMESPACE

KWIN_EFFECT_FACTORY_SUPPORTED(Effect, "metadata.json", return Effect::supported();)
}

#include "plugin.moc"
