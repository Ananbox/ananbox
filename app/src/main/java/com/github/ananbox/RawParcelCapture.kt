package com.github.ananbox

import android.content.Intent
import android.os.Binder
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.os.Parcel
import android.util.Log
import androidx.annotation.RequiresApi
import java.lang.reflect.Method

/**
 * Builds a raw `IActivityManager.broadcastIntent` transaction parcel on the host side and
 * dumps it for the guest to replay.
 *
 * The guest has no way to introspect the host, so anything version-dependent has to be
 * resolved here. Instead of hard-coding the argument list for one AOSP release, the list is
 * driven by the parameter types of the host's own `IActivityManager` stub, so a downstream ROM
 * that inserts, removes or reorders `broadcastIntent` arguments still produces a valid parcel.
 *
 * The kernel-visible part of the parcel (the flat_binder_object that lets the two sides
 * exchange their binder) is still patched on the guest side; see AnanboxFile.h.
 */
@RequiresApi(Build.VERSION_CODES.Q)
object RawParcelCapture {
    private const val TAG = "RawParcelCapture"
    private const val INTERFACE = "android.app.IActivityManager"
    private const val BROADCAST_ACTION = "com.github.ananbox.BINDER"

    /** Placeholder binder serialized into the Intent extras; the guest replaces it. */
    private class MarkerBinder : Binder()

    /**
     * Captures the parcel for [key] ("local" or "binder") into [path].
     *
     * @return true on success. On failure nothing is written and the caller should fall back
     *         to the legacy [ParcelConstructor] path (or skip the handshake).
     */
    fun capture(path: String, key: String): Boolean {
        val method = findBroadcastIntentMethod()
        if (method == null) {
            Log.e(TAG, "IActivityManager.Stub has no broadcastIntent method")
            return false
        }

        val unsupported = method.parameterTypes.filter { !isSupported(it.name) }
        if (unsupported.isNotEmpty()) {
            Log.e(TAG, "unsupported broadcastIntent parameter types: " +
                    unsupported.joinToString { it.name })
            return false
        }

        val code = Anbox.getBroadcastIntentTransactionCode()
        if (code == 0) {
            Log.e(TAG, "failed to probe broadcastIntent transaction code")
            return false
        }

        val intent = Intent(BROADCAST_ACTION).apply {
            putExtras(Bundle().apply { putBinder(key, MarkerBinder()) })
        }

        val data = Parcel.obtain()
        try {
            data.writeInterfaceToken(INTERFACE)

            var binderIndex = -1
            for (type in method.parameterTypes) {
                if (type.name == Intent::class.java.name) {
                    // Remember the object-table index of the marker binder before writing the
                    // Intent. putBinder() adds exactly one flat_binder_object, so the marker
                    // ends up at this index.
                    binderIndex = Anbox.getParcelObjectsCount(data)
                    data.writeTypedObject(intent, 0)
                } else {
                    writeDefault(data, type.name)
                }
            }

            if (binderIndex < 0) {
                Log.e(TAG, "broadcastIntent has no Intent parameter")
                return false
            }

            val dumped = Anbox.dumpRawParcel(data, path, binderIndex, code)
            if (!dumped) {
                Log.e(TAG, "dumpRawParcel($path) failed")
            }
            return dumped
        } catch (e: Exception) {
            Log.e(TAG, "capture($path) failed: $e")
            return false
        } finally {
            data.recycle()
        }
    }

    private fun findBroadcastIntentMethod(): Method? = try {
        val stub = Class.forName("$INTERFACE\$Stub")
        stub.methods.firstOrNull { it.name == "broadcastIntent" }
    } catch (e: Throwable) {
        Log.e(TAG, "cannot resolve $INTERFACE\$Stub: $e")
        null
    }

    /**
     * Types we know how to encode as a harmless placeholder value.
     *
     * Note: Class.getName() uses the JVM descriptor form for arrays ("[Ljava.lang.String;"),
     * not "java.lang.String[]", so array types must be compared against Array<String>::class
     * rather than written out as source-style names.
     */
    private val STRING_ARRAY = Array<String>::class.java.name

    private fun isSupported(name: String): Boolean = when (name) {
        "android.app.IApplicationThread",
        "android.content.IIntentReceiver",
        IBinder::class.java.name,
        Intent::class.java.name,
        Bundle::class.java.name,
        "android.os.Parcelable",
        String::class.java.name,
        STRING_ARRAY,
        "int", "boolean", "long", "float", "double", "byte" -> true
        else -> false
    }

    private fun writeDefault(parcel: Parcel, name: String) = when (name) {
        // capture() intercepts the Intent before calling this; reaching here is a bug.
        Intent::class.java.name -> throw IllegalStateException("Intent must be written by capture()")
        "android.app.IApplicationThread",
        "android.content.IIntentReceiver",
        IBinder::class.java.name -> parcel.writeStrongBinder(null)
        Bundle::class.java.name,
        "android.os.Parcelable" -> parcel.writeTypedObject(null as Bundle?, 0)
        String::class.java.name -> parcel.writeString(null)
        STRING_ARRAY -> parcel.writeStringArray(null)
        "int" -> parcel.writeInt(0)
        "boolean" -> parcel.writeBoolean(false)
        "long" -> parcel.writeLong(0L)
        "float" -> parcel.writeFloat(0f)
        "double" -> parcel.writeDouble(0.0)
        "byte" -> parcel.writeByte(0)
        // isSupported() has already rejected anything else.
        else -> throw IllegalStateException("unsupported parameter type $name")
    }
}
