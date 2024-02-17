package com.github.ananbox

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.util.Log
import kotlin.concurrent.thread

class BinderReceiver : BroadcastReceiver() {
    companion object {
        var remoteBinder: IBinder? = null
    }
    private val tag = "BinderReceiver"

    override fun onReceive(context: Context, intent: Intent) {
        Log.d(tag, "onReceive()")
        val binder = intent.extras?.getBinder("binder")
        if (binder != null && remoteBinder == null) {
            if (binder.pingBinder()) {
                Log.d(tag, "receive remoteBinder")
                remoteBinder = binder
            }
            else {
                Log.e(tag,"dead remoteBinder")
            }
            return
        }
        else if (binder != null && remoteBinder != null) {
            Log.e(tag, "contextMgr has been set");
            return
        }
        thread {
            val localBinder = intent.extras?.getBinder("local")
            if (localBinder != null) {
                Log.d(tag, "receive localBinder")
                // `spinlock` ensures that remoteBinder isn't NULL
                while (remoteBinder == null) {
                }
                ILocalInterface.Stub.asInterface(localBinder).onReceiveBinder(remoteBinder)
                Log.d(tag, "remoteBinder sent");
            }
            else {
                Log.e(tag, "Empty broadcast");
            }
        }
    }
}