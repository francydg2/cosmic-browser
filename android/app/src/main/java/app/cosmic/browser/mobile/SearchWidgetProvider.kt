package app.cosmic.browser.mobile

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews

internal const val ACTION_WIDGET_SEARCH = "app.cosmic.browser.mobile.action.WIDGET_SEARCH"

class SearchWidgetProvider : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, widgetIds: IntArray) {
        val intent = Intent(context, CosmicComposeActivity::class.java).apply {
            action = ACTION_WIDGET_SEARCH
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        for (widgetId in widgetIds) {
            val views = RemoteViews(context.packageName, R.layout.search_widget)
            views.setOnClickPendingIntent(R.id.search_widget, pendingIntent)
            manager.updateAppWidget(widgetId, views)
        }
    }
}
