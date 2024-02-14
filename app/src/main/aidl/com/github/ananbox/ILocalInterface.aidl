// ILocalInterface.aidl
package com.github.ananbox;
import android.os.IBinder;

// Declare any non-default types here with import statements

interface ILocalInterface {
    int getPid();
    void onReceiveBinder(IBinder binder);
}