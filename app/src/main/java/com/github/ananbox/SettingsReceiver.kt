package com.github.ananbox

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class SettingsReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        context.startActivity(Intent(context, SettingsActivity::class.java))
    }
}