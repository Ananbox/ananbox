package com.github.ananbox

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.os.Bundle
import android.util.DisplayMetrics
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.ViewConfiguration
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.github.ananbox.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private val TAG = "MainActivity"
    private lateinit var mSurfaceView: SurfaceView
    private lateinit var anbox: Anbox
    private val mSurfaceCallback: SurfaceHolder.Callback = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            val surface = holder.surface
            val windowManager = windowManager
            val defaultDisplay = windowManager.defaultDisplay
            val displayMetrics = DisplayMetrics()
            defaultDisplay.getRealMetrics(displayMetrics)
            val xdpi = displayMetrics.xdpi
            val ydpi = displayMetrics.ydpi
            Log.i(TAG, "Runtime initializing..")
            if(anbox.initRuntime(mSurfaceView.width, mSurfaceView.height, xdpi.toInt(), ydpi.toInt())) {
                anbox.createSurface(surface)
                anbox.startRuntime()
                anbox.startContainer()
            }
            else {
                anbox.createSurface(surface)
            }
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            Log.i(
                TAG,
                "surfaceChanged: " + mSurfaceView.width + "x" + mSurfaceView.height
            )
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
//            Renderer.removeWindow(holder.surface)
            anbox.destroySurface()
            Log.i(TAG, "surfaceDestroyed!")
        }
    }

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val windowInsetsController =
            WindowCompat.getInsetsController(window, window.decorView)
        windowInsetsController.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars())

        anbox = Anbox(applicationContext);

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        mSurfaceView = SurfaceView(this)
        mSurfaceView.getHolder().addCallback(mSurfaceCallback)
        binding.root.addView(mSurfaceView, 0)

    }

    override fun onResume() {
        super.onResume()
        mSurfaceView.setOnTouchListener(anbox)
    }

    override fun onDestroy() {
        super.onDestroy()
        anbox.stopRuntime()
        anbox.stopContainer()
    }
}