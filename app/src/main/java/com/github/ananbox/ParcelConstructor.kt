package com.github.ananbox

import android.content.ComponentName
import android.content.Intent
import android.os.Binder
import android.os.Build
import android.os.Bundle
import android.os.Parcel
import android.util.Log
import androidx.annotation.RequiresApi

object ParcelConstructor {
    final val TAG: String = "BinderTest"
    final val PATH: String = "/data/data/com.github.ananbox/files/rootfs/"

    @RequiresApi(Build.VERSION_CODES.Q)
    fun getBroadcastIntent(binderName: String) {
        val intent: Intent = Intent()
        val bundle1: Bundle = Bundle()
        val binder1: Binder = Binder()
        bundle1.putBinder(binderName, binder1)
        intent.setComponent(ComponentName("com.github.ananbox", "com.github.ananbox.BinderReceiver"))
        intent.putExtras(bundle1)

        val _data: Parcel = Parcel.obtain()
        var bundle: Bundle = Bundle()
        _data.writeInterfaceToken("android.app.IActivityManager");
        Log.d(TAG, "interface token size: " + _data.dataSize())
        _data.writeStrongInterface(null);
        Log.d(TAG, "interface size: " + _data.dataSize())
        _data.writeTypedObject(intent, 0);
        Log.d(TAG, "intent size: " + _data.dataSize())
        _data.writeString(null);
        _data.writeStrongInterface(null);
        Log.d(TAG, "interface2 size: " + _data.dataSize())
        _data.writeInt(0);
        _data.writeString(null);
        Log.d(TAG, "null string size: " + _data.dataSize());
        _data.writeTypedObject(null, 0);
        Log.d(TAG, "null bundle size: " + _data.dataSize());
        _data.writeStringArray(null);
        Log.d(TAG, "null array size: " + _data.dataSize());
        _data.writeInt(0);
        _data.writeTypedObject(bundle, 0);
        _data.writeBoolean(true);
        _data.writeBoolean(false);
        _data.writeInt(0);
        Anbox.dumpParcel(_data, PATH + binderName + "BroadcastIntent")
        _data.recycle()
    }

//    fun getBroadcastIntent(binderName: String) {
//        val bundle1: Bundle = Bundle()
//        val binder1: Binder = Binder()
//        // binder
//        bundle1.putBinder(binderName, binder1)
//        val parcel: Parcel = Parcel.obtain()
//        val intent: Intent = Intent()
//        intent.setComponent(ComponentName("com.github.ananbox", "com.github.ananbox.BinderReceiver"))
//        intent.putExtras(bundle1)
//        intent.writeToParcel(parcel, 0)
//        Anbox.dumpParcel(parcel, PATH + binderName + "BroadcastIntent")
//        parcel.recycle()
//    }

    fun getEmptyBundle() {
        val bundle1: Bundle = Bundle()
        val parcel2: Parcel = Parcel.obtain()
        bundle1.writeToParcel(parcel2, 0)
        Anbox.dumpParcel(parcel2, PATH + "emptyBundle")
        parcel2.recycle()
    }

    fun getNullBinder() {
        val parcel2: Parcel = Parcel.obtain()
        parcel2.writeStrongBinder(null)
        Anbox.dumpParcel(parcel2, PATH + "nullBinder")
        parcel2.recycle()
    }

//    @RequiresApi(Build.VERSION_CODES.Q)
//    fun dumpBroadcastParcel() {
//        val parcel: Parcel = Parcel.obtain()
//        Log.d(TAG, "bundle size: " + parcel2.readInt())
//        val intent: Intent = Intent()
//        intent.setComponent(ComponentName("com.github.ananbox", "com.github.ananbox.BinderReceiver"))
//        intent.putExtras(bundle1)
//        intent.writeToParcel(parcel, 0)
//        Log.d(TAG, "intent size " + parcel.dataSize())
//        Log.d(TAG, "parcel start")
//
//        val _data: Parcel = Parcel.obtain()
//        var bundle: Bundle = Bundle()
//        _data.writeInterfaceToken("android.app.IActivityManager");
//        Log.d(TAG, "interface token size: " + _data.dataSize())
//        _data.writeStrongInterface(null);
//        Log.d(TAG, "interface size: " + _data.dataSize())
//        _data.writeTypedObject(intent, 0);
//        Log.d(TAG, "broadcastbundle size: " + _data.dataSize())
//        _data.writeString(null);
//        _data.writeStrongInterface(null);
//        Log.d(TAG, "interface2 size: " + _data.dataSize())
//        _data.writeInt(0);
//        _data.writeString(null);
//        Log.d(TAG, "null string size: " + _data.dataSize());
//        val _prev_position: Int = _data.dataSize();
//        _data.writeTypedObject(null, 0);
//        Log.d(TAG, "null bundle size: " + _data.dataSize());
//        _data.writeStringArray(null);
//        Log.d(TAG, "null array size: " + _data.dataSize());
//        _data.writeInt(0);
//        _data.writeTypedObject(bundle, 0);
//        _data.writeBoolean(true);
//        _data.writeBoolean(false);
//        _data.writeInt(0);
//        Log.e(TAG, "total parcel size: " + _data.dataSize())
//    }

}