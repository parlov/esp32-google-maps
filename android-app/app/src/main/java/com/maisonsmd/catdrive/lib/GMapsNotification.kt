package com.maisonsmd.catdrive.lib

import android.app.Notification
import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.BitmapDrawable
import android.service.notification.StatusBarNotification
import android.text.Spanned
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.RemoteViews
import android.widget.TextView
import androidx.core.view.children
import org.json.JSONObject
import timber.log.Timber

const val GMAPS_PACKAGE = "com.google.android.apps.maps"

enum class ContentViewType {
    NORMAL,
    BIG,
    BEST,
}

internal class GMapsNotification(cx: Context, sbn: StatusBarNotification) : NavigationNotification(cx, sbn) {
    init {
        val normalContent = getContentView(ContentViewType.NORMAL)
        if (normalContent != null)
            parseRemoteView(getRemoteViewGroup(normalContent))

        val bestContentView = getContentView(ContentViewType.BEST)
        if (bestContentView != normalContent)
            parseRemoteView(getRemoteViewGroup(bestContentView))
    }

    private fun getContentView(type: ContentViewType = ContentViewType.BEST): RemoteViews? {
        val builder = Notification.Builder.recoverBuilder(mContext, mNotification)
        if (type == ContentViewType.BIG || type == ContentViewType.BEST) {
            val remoteViews = builder.createBigContentView()
            if (remoteViews != null || type == ContentViewType.BIG)
                return remoteViews
        }
        return builder.createContentView()
    }

    private fun getRemoteViewGroup(remoteViews: RemoteViews?): ViewGroup {
        if (remoteViews == null) {
            throw Exception("Impossible to create notification view")
        }

        val layoutInflater = mAppSourceContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        val viewGroup = layoutInflater.inflate(remoteViews.layoutId, null) as ViewGroup?
            ?: throw Exception("Impossible to inflate viewGroup")

        remoteViews.reapply(mAppSourceContext, viewGroup)
        return viewGroup
    }

    private fun getEntryName(item: View): String {
        return try {
            if (item.id > 0) mAppSourceContext.resources.getResourceEntryName(item.id) else ""
        } catch (e: Exception) { "" }
    }

    private fun findChildByNames(group: ViewGroup, vararg names: String): View? {
        for (child in group.children) {
            val entryName = getEntryName(child)
            if (names.contains(entryName)) return child
            if (child is ViewGroup) {
                val c = findChildByNames(child, *names)
                if (c != null) return c
            }
        }
        return null
    }

    private fun parseRemoteView(group: ViewGroup): NavigationData {
        val data = navigationData

        // Maps uses "text", "title", and "header_text". Sometimes they swap them.
        val directionText = findChildByNames(group, "text", "msg") as TextView?
        val etaText = findChildByNames(group, "header_text", "info") as TextView?
        val titleText = findChildByNames(group, "title", "title_text") as TextView?
        val rightIcon = findChildByNames(group, "right_icon", "icon") as ImageView?

        // Parse ETA, ETE & Total Distance (usually separated by dots)
        if (etaText != null) {
            val text = etaText.text.toString()
            val parts = if (text.contains("·")) text.split("·") else text.split("-")
            if (parts.size >= 3) {
                data.eta = NavigationEta(
                    parts[2].removeSuffix("ETA").trim(), 
                    parts[0].trim(), 
                    parts[1].trim()
                )
            }
        }

        var nextDistance = ""
        var nextRoad = ""
        var nextRoadDesc = ""

        val rawTitle = titleText?.text?.toString()?.trim() ?: ""
        val rawDir = directionText?.text

        // Logic to distinguish distance from road name
        // If title is short (e.g., "500 m"), it's likely the distance.
        if (rawTitle.length < 10 && rawTitle.any { it.isDigit() }) {
            nextDistance = rawTitle
        } else {
            nextRoad = rawTitle
        }

        if (rawDir is Spanned) {
            val directionList = ParserHelper.splitByStyleSpan(rawDir, Typeface.NORMAL, 2)
            if (directionList.isNotEmpty()) {
                if (nextRoad.isEmpty()) nextRoad = directionList.first().text
                else nextRoadDesc = directionList.first().text
                
                if (directionList.size > 1) {
                    nextRoadDesc = directionList.drop(1).joinToString(" ") { it.text }
                }
            }
        } else if (rawDir != null) {
            val dirStr = rawDir.toString()
            if (nextRoad.isEmpty()) nextRoad = dirStr else nextRoadDesc = dirStr
        }

        data.nextDirection = NavigationDirection(nextRoad, nextRoadDesc, nextDistance)

        (rightIcon?.drawable as BitmapDrawable?)?.bitmap?.also {
            data.actionIcon = NavigationIcon(it.copy(it.config ?: android.graphics.Bitmap.Config.ARGB_8888, false))
        }

        return data
    }
}
