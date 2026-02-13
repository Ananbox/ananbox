package com.github.ananbox

import android.os.Build
import android.os.Parcel
import android.util.Log
import android.view.MotionEvent
import android.view.Surface
import android.view.View
import kotlin.system.exitProcess

object Anbox: View.OnTouchListener {
    init {
        System.loadLibrary("anbox")
    }

    external fun stringFromJNI(): String
    external fun setPath(path: String)
    external fun startRuntime()
    external fun destroyWindow()
    external fun stopRuntime()
    external fun startContainer(proot: String)
    external fun resetWindow(height: Int, width: Int)
    external fun createSurface(surface: Surface)
    external fun destroySurface()
    // pipe including Renderer, GPS & Sensor, input manager
    external fun initRuntime(width: Int, height: Int, dpi: Int): Boolean
    external fun pushFingerUp(i: Int)
    external fun pushFingerDown(x: Int, y: Int, fingerId: Int)
    external fun pushFingerMotion(x: Int, y: Int, fingerId: Int)
    external fun dumpParcel(parcel: Parcel, path: String)

    fun stopContainer() {
        Log.d("Anbox", "stopContainer")
        // TODO: better way to stop the container
        Runtime.getRuntime().exec("killall init")
        exitProcess(0)
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