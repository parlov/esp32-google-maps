package com.maisonsmd.catdrive.service

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import com.maisonsmd.catdrive.lib.GMAPS_PACKAGE
import com.maisonsmd.catdrive.lib.GMapsNotification
import com.maisonsmd.catdrive.lib.NavigationData
import com.maisonsmd.catdrive.lib.NavigationNotification
import kotlinx.coroutines.*
import timber.log.Timber

@OptIn(DelicateCoroutinesApi::class)
open class NavigationListener : NotificationListenerService() {
    private var mNotificationParserCoroutine: Job? = null
    private lateinit var mLastNotification: StatusBarNotification

    private var mCurrentNotification: NavigationNotification? = null
    private var mEnabled = false

    protected var enabled: Boolean
        get() = mEnabled
        set(value) {
            if (value == mEnabled)
                return
            if (value.also { mEnabled = it })
                checkActiveNotifications()
            else {
                mCurrentNotification = null
            }
        }

    val lastNavigationData: NavigationData? get() = mCurrentNotification?.navigationData

    override fun onListenerConnected() {
        super.onListenerConnected()
        checkActiveNotifications()
    }

    private fun checkActiveNotifications() {
        try {
            Timber.d("Checking for active Navigation notifications")
            this.activeNotifications.forEach { statusBarNotification ->
                onNotificationPosted(statusBarNotification)
            }
        } catch (e: Throwable) {
            Timber.e("Failed to check for active notifications: $e")
        }
    }

    private fun isGoogleMapsNotification(sbn: StatusBarNotification?): Boolean {
        if (!enabled || sbn == null) return false

        // Check if it's from Google Maps
        if (GMAPS_PACKAGE !in sbn.packageName) return false

        val notification = sbn.notification
        
        // Navigation notifications are usually ongoing
        val isOngoing = (notification.flags and Notification.FLAG_ONGOING_EVENT) != 0
        
        // They also usually have the CATEGORY_NAVIGATION
        val isNavigationCategory = notification.category == Notification.CATEGORY_NAVIGATION

        // We accept it if it's ongoing AND (is in navigation category OR has a specific ID)
        // This makes it much more robust against Google Maps updates
        return isOngoing && (isNavigationCategory || sbn.id == 1)
    }

    protected open fun onNavigationNotificationAdded(navNotification: NavigationNotification) {}

    protected open fun onNavigationNotificationUpdated(navNotification: NavigationNotification) {}

    protected open fun onNavigationNotificationRemoved(navNotification: NavigationNotification) {}

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        if (isGoogleMapsNotification(sbn)) {
            handleGoogleNotification(sbn!!)
        }
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification?) {
        if (isGoogleMapsNotification(sbn)) {
            mNotificationParserCoroutine?.cancel()

            onNavigationNotificationRemoved(
                if (mCurrentNotification != null) mCurrentNotification!!
                else NavigationNotification(applicationContext, sbn!!)
            )

            mCurrentNotification = null
        }
    }

    private fun handleGoogleNotification(statusBarNotification: StatusBarNotification) {
        mLastNotification = statusBarNotification
        if (mNotificationParserCoroutine != null && mNotificationParserCoroutine!!.isActive)
            return

        mNotificationParserCoroutine = GlobalScope.launch(Dispatchers.Main) {
            val worker = GlobalScope.async(Dispatchers.Default) {
                return@async try {
                    GMapsNotification(this@NavigationListener.applicationContext, mLastNotification)
                } catch (e: Exception) {
                    Timber.e("Error creating GMapsNotification: $e")
                    null
                }
            }

            try {
                val mapNotification = worker.await() ?: return@launch
                val lastNotification = mCurrentNotification

                val updated: Boolean = if (lastNotification == null) {
                    onNavigationNotificationAdded(mapNotification)
                    true
                } else {
                    lastNotification.navigationData != mapNotification.navigationData
                }

                if (updated) {
                    mCurrentNotification = mapNotification
                    onNavigationNotificationUpdated(mCurrentNotification!!)
                }
            } catch (error: Exception) {
                if (!mNotificationParserCoroutine!!.isCancelled)
                    Timber.e("Got an error while parsing: $error")
            }
        }
    }
}
