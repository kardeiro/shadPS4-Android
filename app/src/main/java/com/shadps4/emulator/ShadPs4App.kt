/*
 * shadPS4 Android - UI prototype for a PS4 emulator on Android.
 * Copyright (C) 2025 shadPS4 Android contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Based on the upstream shadPS4 project (https://github.com/shadps4-emu/shadPS4).
 *
 * NOTE: This is a UI-only prototype. No PS4 game execution is implemented yet.
 *       The native emulator core (C++) is not yet wired up to this Android shell.
 */

package com.shadps4.emulator

import android.app.Application

class ShadPs4App : Application() {
    override fun onCreate() {
        super.onCreate()
        instance = this
    }

    companion object {
        lateinit var instance: ShadPs4App
            private set
    }
}
