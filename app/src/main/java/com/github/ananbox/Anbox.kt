package com.github.ananbox

import android.content.Context
import android.util.Log
import android.view.MotionEvent
import android.view.Surface
import android.os.Process
import android.view.View

class Anbox(context: Context): View.OnTouchListener {
    companion object {
        // Used to load the 'anbox' library on application startup.
        init {
            System.loadLibrary("anbox")
        }
    }

    external fun stringFromJNI(): String
    external fun startRuntime()
    external fun destroyWindow()
    external fun stopRuntime()
    external fun startContainer()
    external fun resetWindow(height: Int, width: Int)
    external fun createSurface(surface: Surface)
    external fun destroySurface()
    // pipe including Renderer, GPS & Sensor, input manager
    external fun initRuntime(width: Int, height: Int, xDpi: Int, yDpi: Int): Boolean
    external fun pushFingerUp(i: Int)
    external fun pushFingerDown(x: Int, y: Int, fingerId: Int)
    external fun pushFingerMotion(x: Int, y: Int, fingerId: Int)

    fun stopContainer() {
        Log.d("Anbox", "stopContainer")
        System.exit(0);
        Process.killProcess(Process.myPid());
    }

    override fun onTouch(v: View, e: MotionEvent): Boolean {
        when (e.action) {
            MotionEvent.ACTION_DOWN -> {
                pushFingerDown(e.x.toInt(), e.y.toInt(), 0)
            }

            MotionEvent.ACTION_UP -> {
                pushFingerUp(0)
            }

            MotionEvent.ACTION_MOVE -> {
                pushFingerMotion(e.x.toInt(), e.y.toInt(), 0)
            }
        }
        return true
    }
}