package app.cosmic.browser.mobile

import android.Manifest
import android.app.DownloadManager
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.webkit.CookieManager
import android.webkit.DownloadListener
import android.webkit.URLUtil
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebStorage
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.cosmic.browser.mobile.privacy.RequestProtection
import com.google.zxing.WriterException
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

private val CosmicBackground = Color(0xFF111319)
private val CosmicSurface = Color(0xFF151820)
private val CosmicCard = Color(0xFF1A1E27)
private val CosmicField = Color(0xFF20242D)
private val CosmicBorder = Color(0xFF343B48)
private val CosmicText = Color(0xFFE8E9EE)
private val CosmicMuted = Color(0xFF98A2B2)
private val CosmicAccent = Color(0xFF7C6CF0)

private data class DownloadInfo(
    val url: String,
    val userAgent: String?,
    val contentDisposition: String?,
    val mimeType: String?
)

private enum class BrowserScreen { BROWSER, TABS, BOOKMARKS, HISTORY, SETTINGS, READER }

private enum class SettingsCategory(val title: String, val description: String) {
    GENERAL("Generali", "Avvio e sessione"),
    SEARCH("Motore di ricerca", "Ricerca dalla barra degli indirizzi"),
    PRIVACY("Privacy", "Protezione e permessi dei siti"),
    APPEARANCE("Aspetto", "Colori e disposizione"),
    PASSWORDS("Password", "Vault e credenziali"),
    USERS("Utenti", "Profili e dati di navigazione"),
    ADVANCED("Avanzate", "Connessioni e dati"),
    UPDATES("Aggiornamenti", "Versione Android")
}

private data class SearchEngine(val id: String, val name: String, val queryUrl: String) {
    fun search(query: String): String = queryUrl.replace("%s", Uri.encode(query))
}

private val searchEngines = listOf(
    SearchEngine("duckduckgo", "DuckDuckGo", "https://duckduckgo.com/?q=%s"),
    SearchEngine("google", "Google", "https://www.google.com/search?q=%s"),
    SearchEngine("bing", "Bing", "https://www.bing.com/search?q=%s"),
    SearchEngine("startpage", "Startpage", "https://www.startpage.com/sp/search?query=%s"),
    SearchEngine("brave", "Brave Search", "https://search.brave.com/search?q=%s"),
    SearchEngine("mojeek", "Mojeek", "https://www.mojeek.com/search?q=%s")
)

private val translationLanguages = listOf(
    "Italiano" to "it", "Inglese" to "en", "Francese" to "fr",
    "Tedesco" to "de", "Spagnolo" to "es", "Portoghese" to "pt",
    "Giapponese" to "ja"
)

private data class SavedPage(val title: String, val url: String)
private data class HistoryEntry(val title: String, val url: String, val visitedAt: Long)

private class BrowserTab(val id: Long, initialUrl: String = "") {
    var url by mutableStateOf(initialUrl)
    var title by mutableStateOf(if (initialUrl.isBlank()) "Nuova scheda" else initialUrl)
    var progress by mutableIntStateOf(0)
    var webView: WebView? = null
}

class CosmicComposeActivity : ComponentActivity() {
    private var pendingDownload: DownloadInfo? = null
    private var browser: BrowserController? = null

    private val storagePermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        val download = pendingDownload
        pendingDownload = null
        if (granted && download != null) {
            enqueueDownload(download)
        } else if (download != null) {
            Toast.makeText(this, "Permesso di archiviazione negato", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.statusBarColor = android.graphics.Color.rgb(17, 19, 25)
        window.navigationBarColor = android.graphics.Color.rgb(17, 19, 25)
        browser = BrowserController(this, ::requestDownload)
        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = CosmicAccent,
                    secondary = Color(0xFFB3A8FF),
                    background = CosmicBackground,
                    surface = CosmicSurface,
                    onPrimary = Color.White,
                    onBackground = CosmicText,
                    onSurface = CosmicText
                )
            ) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = CosmicBackground
                ) {
                    browser?.let { BrowserApp(it, onExit = ::finish) }
                }
            }
        }
    }

    override fun onPause() {
        browser?.pause()
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        browser?.resume()
    }

    override fun onStop() {
        browser?.save()
        super.onStop()
    }

    override fun onDestroy() {
        browser?.dispose()
        browser = null
        super.onDestroy()
    }

    private fun requestDownload(info: DownloadInfo) {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P &&
            checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) !=
            PackageManager.PERMISSION_GRANTED
        ) {
            pendingDownload = info
            storagePermission.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
            return
        }
        enqueueDownload(info)
    }

    private fun enqueueDownload(info: DownloadInfo) {
        val uri = Uri.parse(info.url)
        if (uri.scheme != "http" && uri.scheme != "https") {
            Toast.makeText(this, "Indirizzo di download non supportato", Toast.LENGTH_SHORT).show()
            return
        }
        try {
            val request = DownloadManager.Request(uri)
                .setTitle(URLUtil.guessFileName(info.url, info.contentDisposition, info.mimeType))
                .setDescription("Download avviato da Cosmic")
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setDestinationInExternalPublicDir(
                    Environment.DIRECTORY_DOWNLOADS,
                    safeDownloadName(URLUtil.guessFileName(info.url, info.contentDisposition, info.mimeType))
                )
            if (!info.mimeType.isNullOrBlank()) request.setMimeType(info.mimeType)
            if (!info.userAgent.isNullOrBlank()) request.addRequestHeader("User-Agent", info.userAgent)
            CookieManager.getInstance().getCookie(info.url)?.let {
                request.addRequestHeader("Cookie", it)
            }
            val manager = getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
            manager.enqueue(request)
            Toast.makeText(this, "Download avviato · cartella Download", Toast.LENGTH_SHORT).show()
        } catch (_: Exception) {
            Toast.makeText(this, "Impossibile avviare il download", Toast.LENGTH_LONG).show()
        }
    }

    private fun safeDownloadName(name: String): String =
        name.replace(Regex("[\\\\/:*?\"<>|\\u0000-\\u001F]"), "_").take(180)
            .ifBlank { "download" }
}

private class BrowserController(
    private val activity: ComponentActivity,
    private val onDownload: (DownloadInfo) -> Unit
) {
    private val preferences = activity.getSharedPreferences("cosmic_mobile", Context.MODE_PRIVATE)
    private val requestProtection = RequestProtection(activity.applicationContext)
    var blockKnownHosts by mutableStateOf(requestProtection.enabled)
        private set
    val tabs = mutableStateListOf<BrowserTab>()
    val bookmarks = mutableStateListOf<SavedPage>()
    val history = mutableStateListOf<HistoryEntry>()
    var activeTabId by mutableStateOf(0L)
        private set
    var screen by mutableStateOf(BrowserScreen.BROWSER)
    var searchEngineId by mutableStateOf("duckduckgo")
        private set
    var customSearchTemplate by mutableStateOf("")
        private set
    var restoreSession by mutableStateOf(preferences.getBoolean("restoreSession", true))
        private set
    var settingsCategory by mutableStateOf<SettingsCategory?>(null)
    val versionName: String = activity.packageManager
        .getPackageInfo(activity.packageName, 0).versionName ?: "non disponibile"
    var showClearDialog by mutableStateOf(false)
    var readerTitle by mutableStateOf("")
        private set
    var readerText by mutableStateOf("")
        private set
    var translationUrl by mutableStateOf<String?>(null)
    var qrBitmap by mutableStateOf<Bitmap?>(null)
        private set

    init {
        restoreBookmarks()
        restoreHistory()
        searchEngineId = preferences.getString("searchEngine", "duckduckgo") ?: "duckduckgo"
        customSearchTemplate = preferences.getString("customSearchTemplate", "") ?: ""
        restoreTabs()
    }

    val activeTab: BrowserTab
        get() = tabs.firstOrNull { it.id == activeTabId } ?: tabs.first()

    private fun restoreTabs() {
        val restored = mutableListOf<String>()
        try {
            val array = JSONArray(if (restoreSession) preferences.getString("tabs", "[]") else "[]")
            for (index in 0 until minOf(array.length(), 20)) {
                restored += array.optString(index)
            }
        } catch (_: Exception) {
            preferences.edit().remove("tabs").apply()
        }
        if (restored.isEmpty()) restored += ""
        restored.forEach { url -> tabs += newTabModel(url) }
        val activeIndex = preferences.getInt("activeTab", 0).coerceIn(0, tabs.lastIndex)
        activeTabId = tabs[activeIndex].id
        tabs.forEach { tab ->
            val webView = createWebView(tab)
            if (tab.url.isNotBlank()) webView.loadUrl(tab.url)
        }
    }

    private fun newTabModel(url: String): BrowserTab =
        BrowserTab(System.nanoTime(), url)

    fun createTab(url: String = "") {
        if (tabs.size >= 20) {
            Toast.makeText(activity, "Limite di 20 schede raggiunta", Toast.LENGTH_SHORT).show()
            return
        }
        val tab = newTabModel(url)
        tabs += tab
        activeTabId = tab.id
        screen = BrowserScreen.BROWSER
        val view = createWebView(tab)
        if (url.isNotBlank()) view.loadUrl(url)
        save()
    }

    fun selectTab(tab: BrowserTab) {
        activeTabId = tab.id
        screen = BrowserScreen.BROWSER
    }

    fun closeTab(tab: BrowserTab) {
        val index = tabs.indexOf(tab)
        if (index < 0) return
        destroyWebView(tab)
        tabs.remove(tab)
        if (tabs.isEmpty()) {
            val fresh = newTabModel("")
            tabs += fresh
            fresh.webView = createWebView(fresh)
            activeTabId = fresh.id
        } else if (activeTabId == tab.id) {
            activeTabId = tabs[(index - 1).coerceAtLeast(0)].id
        }
        save()
    }

    fun navigate(input: String) {
        val value = input.trim()
        if (value.isEmpty()) return
        val destination = resolveDestination(value)
        if (destination == null) {
            Toast.makeText(activity, "Sono supportati solo indirizzi web e ricerche", Toast.LENGTH_SHORT).show()
            return
        }
        hideKeyboard()
        val tab = activeTab
        tab.url = destination
        tab.title = domain(destination)
        tab.webView?.loadUrl(destination) ?: createWebView(tab).loadUrl(destination)
        screen = BrowserScreen.BROWSER
        save()
    }

    private fun resolveDestination(input: String): String? {
        val lower = input.lowercase(Locale.ROOT)
        if (lower.startsWith("http://") || lower.startsWith("https://")) return input
        if (input.contains("://")) return null
        val looksLikeHost = !input.any(Char::isWhitespace) &&
            (input.contains('.') || lower.startsWith("localhost:") || lower == "localhost")
        if (looksLikeHost) return "https://$input"
        if (searchEngineId == "custom") {
            if (!validSearchTemplate(customSearchTemplate)) return null
            return customSearchTemplate.replace("%1", Uri.encode(input))
        }
        val engine = searchEngines.firstOrNull { it.id == searchEngineId } ?: searchEngines.first()
        return engine.search(input)
    }

    fun chooseSearchEngine(id: String) {
        if (searchEngines.none { it.id == id }) return
        searchEngineId = id
        preferences.edit().putString("searchEngine", id).apply()
    }

    fun chooseCustomSearch(template: String): Boolean {
        val value = template.trim()
        if (!validSearchTemplate(value)) return false
        customSearchTemplate = value
        searchEngineId = "custom"
        preferences.edit().putString("customSearchTemplate", value).putString("searchEngine", "custom").apply()
        return true
    }

    fun updateRestoreSession(enabled: Boolean) {
        restoreSession = enabled
        preferences.edit().putBoolean("restoreSession", enabled).apply()
        save()
    }

    fun updateBlockKnownHosts(enabled: Boolean) {
        requestProtection.setEnabled(enabled)
        blockKnownHosts = enabled
    }

    fun toggleBookmark() {
        val tab = activeTab
        if (tab.url.isBlank()) {
            Toast.makeText(activity, "Apri una pagina per salvarla", Toast.LENGTH_SHORT).show()
            return
        }
        val existing = bookmarks.indexOfFirst { it.url == tab.url }
        if (existing >= 0) {
            bookmarks.removeAt(existing)
        } else {
            bookmarks.add(0, SavedPage(tab.title.ifBlank { tab.url }, tab.url))
        }
        persistBookmarks()
    }

    fun openBookmark(page: SavedPage) = navigate(page.url)

    fun removeBookmark(page: SavedPage) {
        bookmarks.remove(page)
        persistBookmarks()
    }

    fun openHistory(entry: HistoryEntry) = navigate(entry.url)

    fun clearHistory() {
        history.clear()
        preferences.edit().remove("history").apply()
    }

    fun reload() {
        activeTab.webView?.reload()
    }

    private fun currentWebAddress(): String? {
        val uri = Uri.parse(activeTab.url)
        if (uri.scheme?.lowercase(Locale.ROOT) !in setOf("http", "https") || uri.host.isNullOrBlank()) {
            Toast.makeText(activity, "Apri una pagina web per usare questa funzione", Toast.LENGTH_SHORT).show()
            return null
        }
        return uri.buildUpon()
            .encodedAuthority(uri.encodedAuthority?.substringAfterLast('@'))
            .build().toString()
    }

    fun showReader() {
        currentWebAddress() ?: return
        val tab = activeTab
        val originalUrl = tab.url
        val view = tab.webView ?: return
        val title = tab.title
        view.evaluateJavascript("""
            (() => {
                const root = document.querySelector('article, main, [role="main"]') || document.body;
                if (!root) return '';
                const parts = [];
                let size = 0;
                for (const node of root.querySelectorAll('h1,h2,h3,h4,p,li,blockquote,pre')) {
                    if (node.closest('nav,aside,footer,header,script,style') ||
                        node.parentElement?.closest('li,blockquote,pre')) continue;
                    const text = node.innerText.trim();
                    if (!text) continue;
                    parts.push(text.slice(0, 300000 - size));
                    size += text.length;
                    if (size >= 300000) break;
                }
                return parts.join('\n\n');
            })()
        """.trimIndent()) { result ->
            if (activeTabId != tab.id || tab.url != originalUrl || screen != BrowserScreen.BROWSER) return@evaluateJavascript
            val text = try {
                JSONArray("[$result]").optString(0, "").trim()
            } catch (_: org.json.JSONException) {
                ""
            }
            if (text.isBlank() || text == "null") {
                Toast.makeText(activity, "Nessun testo leggibile trovato nella pagina", Toast.LENGTH_SHORT).show()
            } else {
                readerTitle = title
                readerText = text
                screen = BrowserScreen.READER
            }
        }
    }

    fun requestTranslation() {
        translationUrl = currentWebAddress()
    }

    fun translate(url: String, language: String) {
        translationUrl = null
        val target = Uri.parse("https://translate.google.com/translate").buildUpon()
            .appendQueryParameter("sl", "auto")
            .appendQueryParameter("tl", language)
            .appendQueryParameter("u", url)
            .build().toString()
        createTab(target)
    }

    fun showQrCode() {
        val url = currentWebAddress() ?: return
        try {
            val matrix = pageQrCode(url)
            val pixels = IntArray(matrix.width * matrix.height) { index ->
                if (matrix[index % matrix.width, index / matrix.width]) android.graphics.Color.BLACK
                else android.graphics.Color.WHITE
            }
            qrBitmap = Bitmap.createBitmap(pixels, matrix.width, matrix.height, Bitmap.Config.ARGB_8888)
        } catch (_: WriterException) {
            Toast.makeText(activity, "Indirizzo troppo lungo per il codice QR", Toast.LENGTH_SHORT).show()
        } catch (_: IllegalArgumentException) {
            Toast.makeText(activity, "Impossibile creare il codice QR", Toast.LENGTH_SHORT).show()
        }
    }

    fun closeQrCode() {
        qrBitmap = null
    }

    fun webViewFor(tab: BrowserTab): WebView = createWebView(tab)

    fun goBack() {
        val webView = activeTab.webView
        if (webView?.canGoBack() == true) {
            webView.goBack()
        } else if (activeTab.url.isNotBlank()) {
            goHome()
        }
    }

    fun goForward() {
        activeTab.webView?.takeIf { it.canGoForward() }?.goForward()
    }

    fun goHome() {
        val tab = activeTab
        tab.url = ""
        tab.title = "Nuova scheda"
        tab.progress = 0
        tab.webView?.apply {
            stopLoading()
            clearHistory()
            loadUrl("about:blank")
        }
        screen = BrowserScreen.BROWSER
        save()
    }

    fun handleBack(onExit: () -> Unit) {
        if (screen == BrowserScreen.SETTINGS && settingsCategory != null) {
            settingsCategory = null
        } else if (screen != BrowserScreen.BROWSER) {
            screen = BrowserScreen.BROWSER
        } else if (activeTab.webView?.canGoBack() == true) {
            goBack()
        } else if (activeTab.url.isNotBlank()) {
            goHome()
        } else {
            onExit()
        }
    }

    fun clearSiteData() {
        tabs.forEach { tab ->
            tab.webView?.apply {
                clearCache(true)
                clearHistory()
                clearFormData()
            }
        }
        CookieManager.getInstance().removeAllCookies(null)
        CookieManager.getInstance().flush()
        WebStorage.getInstance().deleteAllData()
        clearHistory()
        showClearDialog = false
        Toast.makeText(activity, "Dati di navigazione cancellati", Toast.LENGTH_SHORT).show()
    }

    fun save() {
        if (!restoreSession) {
            preferences.edit().remove("tabs").remove("activeTab").apply()
            return
        }
        val urls = JSONArray()
        tabs.forEach { urls.put(it.url) }
        preferences.edit()
            .putString("tabs", urls.toString())
            .putInt("activeTab", tabs.indexOfFirst { it.id == activeTabId }.coerceAtLeast(0))
            .apply()
    }

    fun pause() {
        tabs.forEach { it.webView?.onPause() }
    }

    fun resume() {
        tabs.forEach { it.webView?.onResume() }
    }

    fun dispose() {
        tabs.toList().forEach(::destroyWebView)
    }

    private fun createWebView(tab: BrowserTab): WebView {
        tab.webView?.let { return it }
        val webView = WebView(activity)
        webView.setBackgroundColor(android.graphics.Color.rgb(17, 19, 25))
        webView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            javaScriptCanOpenWindowsAutomatically = false
            setSupportMultipleWindows(false)
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) safeBrowsingEnabled = true
        }
        CookieManager.getInstance().setAcceptThirdPartyCookies(webView, false)
        webView.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(
                view: WebView,
                request: WebResourceRequest
            ): WebResourceResponse? = requestProtection.intercept(request.url)

            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean =
                handleNavigation(request.url)

            @Suppress("DEPRECATION")
            override fun shouldOverrideUrlLoading(view: WebView, url: String): Boolean =
                handleNavigation(Uri.parse(url))

            override fun onPageStarted(view: WebView, url: String, favicon: android.graphics.Bitmap?) {
                if (url == "about:blank") return
                tab.url = url
                tab.progress = 5
                save()
            }

            override fun onPageFinished(view: WebView, url: String) {
                if (url == "about:blank") return
                tab.url = url
                view.title?.takeIf(String::isNotBlank)?.let { tab.title = it }
                tab.progress = 100
                addHistory(tab.title, url)
                save()
            }
        }
        webView.webChromeClient = object : WebChromeClient() {
            override fun onReceivedTitle(view: WebView, title: String?) {
                if (!title.isNullOrBlank()) tab.title = title
            }

            override fun onProgressChanged(view: WebView, newProgress: Int) {
                tab.progress = newProgress
            }
        }
        webView.setDownloadListener(
            DownloadListener { url, userAgent, contentDisposition, mimeType, _ ->
                onDownload(DownloadInfo(url, userAgent, contentDisposition, mimeType))
            }
        )
        tab.webView = webView
        return webView
    }

    private fun handleNavigation(uri: Uri): Boolean {
        val scheme = uri.scheme?.lowercase(Locale.ROOT)
        if (scheme == "http" || scheme == "https" || scheme == "about") return false
        if (scheme in setOf("tel", "mailto", "sms", "geo")) {
            try {
                activity.startActivity(android.content.Intent(android.content.Intent.ACTION_VIEW, uri))
            } catch (_: Exception) {
                Toast.makeText(activity, "Nessuna app disponibile per questo link", Toast.LENGTH_SHORT).show()
            }
        } else {
            Toast.makeText(activity, "Tipo di link non supportato", Toast.LENGTH_SHORT).show()
        }
        return true
    }

    private fun destroyWebView(tab: BrowserTab) {
        val webView = tab.webView ?: return
        (webView.parent as? ViewGroup)?.removeView(webView)
        webView.stopLoading()
        webView.destroy()
        tab.webView = null
    }

    private fun addHistory(title: String, url: String) {
        if (url.isBlank() || url == "about:blank") return
        val current = history.firstOrNull()
        if (current?.url == url) {
            history[0] = HistoryEntry(title.ifBlank { url }, url, System.currentTimeMillis())
        } else {
            history.add(0, HistoryEntry(title.ifBlank { url }, url, System.currentTimeMillis()))
            while (history.size > 500) history.removeAt(history.lastIndex)
        }
        val array = JSONArray()
        history.forEach { entry ->
            array.put(JSONObject().put("title", entry.title).put("url", entry.url).put("visitedAt", entry.visitedAt))
        }
        preferences.edit().putString("history", array.toString()).apply()
    }

    private fun restoreBookmarks() {
        try {
            val array = JSONArray(preferences.getString("bookmarks", "[]"))
            for (i in 0 until array.length()) {
                val value = array.getJSONObject(i)
                val url = value.optString("url")
                if (url.startsWith("http://") || url.startsWith("https://")) {
                    bookmarks += SavedPage(value.optString("title", url), url)
                }
            }
        } catch (_: Exception) {
            preferences.edit().remove("bookmarks").apply()
        }
    }

    private fun restoreHistory() {
        try {
            val array = JSONArray(preferences.getString("history", "[]"))
            for (i in 0 until minOf(array.length(), 500)) {
                val value = array.getJSONObject(i)
                val url = value.optString("url")
                if (url.startsWith("http://") || url.startsWith("https://")) {
                    history += HistoryEntry(
                        value.optString("title", url),
                        url,
                        value.optLong("visitedAt")
                    )
                }
            }
        } catch (_: Exception) {
            preferences.edit().remove("history").apply()
        }
    }

    private fun persistBookmarks() {
        val array = JSONArray()
        bookmarks.forEach { array.put(JSONObject().put("title", it.title).put("url", it.url)) }
        preferences.edit().putString("bookmarks", array.toString()).apply()
    }

    private fun hideKeyboard() {
        val imm = activity.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.hideSoftInputFromWindow(activity.window.decorView.windowToken, 0)
    }

    private fun domain(url: String): String = Uri.parse(url).host?.lowercase(Locale.ROOT) ?: url
}

@Composable
private fun BrowserApp(controller: BrowserController, onExit: () -> Unit) {
    val activeTab = controller.activeTab
    BackHandler {
        controller.handleBack(onExit)
    }
    DisposableEffect(controller) {
        onDispose { controller.save() }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(CosmicBackground)
    ) {
        Header(controller)
        when (controller.screen) {
            BrowserScreen.BROWSER -> {
                AddressBar(controller, activeTab)
                BrowserContent(controller, activeTab, Modifier.weight(1f))
            }
            BrowserScreen.TABS -> TabsScreen(controller, Modifier.weight(1f))
            BrowserScreen.BOOKMARKS -> BookmarksScreen(controller, Modifier.weight(1f))
            BrowserScreen.HISTORY -> HistoryScreen(controller, Modifier.weight(1f))
            BrowserScreen.SETTINGS -> SettingsScreen(controller, Modifier.weight(1f))
            BrowserScreen.READER -> ReaderScreen(controller, Modifier.weight(1f))
        }
        NavigationBar(controller)
    }

    if (controller.showClearDialog) {
        AlertDialog(
            onDismissRequest = { controller.showClearDialog = false },
            title = { Text("Cancellare i dati di navigazione?") },
            text = { Text("Verranno rimossi cookie, cache, dati dei siti e cronologia locale.") },
            confirmButton = {
                TextButton(onClick = controller::clearSiteData) { Text("Cancella") }
            },
            dismissButton = {
                TextButton(onClick = { controller.showClearDialog = false }) { Text("Annulla") }
            },
            containerColor = CosmicSurface,
            titleContentColor = CosmicText,
            textContentColor = CosmicMuted
        )
    }
    controller.translationUrl?.let { url ->
        var language by remember(url) { mutableStateOf("it") }
        AlertDialog(
            onDismissRequest = { controller.translationUrl = null },
            title = { Text("Traduci pagina") },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    Text("Google Translate riceverà l'indirizzo della pagina, inclusi i parametri. Le pagine con accesso potrebbero non funzionare.")
                    translationLanguages.forEach { (name, code) ->
                        Row(
                            modifier = Modifier.fillMaxWidth().clickable { language = code },
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(selected = language == code, onClick = { language = code })
                            Text(name)
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { controller.translate(url, language) }) { Text("Traduci") }
            },
            dismissButton = {
                TextButton(onClick = { controller.translationUrl = null }) { Text("Annulla") }
            },
            containerColor = CosmicSurface
        )
    }
    controller.qrBitmap?.let { bitmap ->
        AlertDialog(
            onDismissRequest = controller::closeQrCode,
            title = { Text("Codice QR della pagina") },
            text = {
                Column {
                    Text("Il codice contiene l'indirizzo e i suoi eventuali parametri.")
                    Spacer(Modifier.height(8.dp))
                    Image(
                        bitmap = bitmap.asImageBitmap(),
                        contentDescription = "Codice QR dell'indirizzo della pagina",
                        modifier = Modifier.fillMaxWidth(),
                        filterQuality = FilterQuality.None
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = controller::closeQrCode) { Text("Chiudi") }
            },
            containerColor = CosmicSurface
        )
    }
}

@Composable
private fun Header(controller: BrowserController) {
    var menuExpanded by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(CosmicBackground)
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (controller.screen == BrowserScreen.BROWSER) {
            Text(
                "COSMIC",
                modifier = Modifier.weight(1f),
                color = CosmicText,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 2.sp
            )
            TextButton(onClick = controller::toggleBookmark) {
                Text(if (controller.bookmarks.any { it.url == controller.activeTab.url }) "★" else "☆")
            }
            Box {
                TextButton(onClick = { menuExpanded = true }) { Text("Menu") }
                DropdownMenu(
                    expanded = menuExpanded,
                    onDismissRequest = { menuExpanded = false },
                    containerColor = CosmicSurface
                ) {
                    DropdownMenuItem(
                        text = { Text("Nuova scheda") },
                        onClick = { menuExpanded = false; controller.createTab() }
                    )
                    DropdownMenuItem(
                        text = { Text("Preferiti") },
                        onClick = { menuExpanded = false; controller.screen = BrowserScreen.BOOKMARKS }
                    )
                    DropdownMenuItem(
                        text = { Text("Cronologia") },
                        onClick = { menuExpanded = false; controller.screen = BrowserScreen.HISTORY }
                    )
                    DropdownMenuItem(
                        text = { Text("Ricarica pagina") },
                        onClick = { menuExpanded = false; controller.reload() }
                    )
                    DropdownMenuItem(
                        text = { Text("Cerca schede") },
                        onClick = { menuExpanded = false; controller.screen = BrowserScreen.TABS }
                    )
                    DropdownMenuItem(
                        text = { Text("Modalità lettura") },
                        onClick = { menuExpanded = false; controller.showReader() }
                    )
                    DropdownMenuItem(
                        text = { Text("Traduci pagina") },
                        onClick = { menuExpanded = false; controller.requestTranslation() }
                    )
                    DropdownMenuItem(
                        text = { Text("Codice QR") },
                        onClick = { menuExpanded = false; controller.showQrCode() }
                    )
                }
            }
        } else {
            TextButton(onClick = {
                if (controller.screen == BrowserScreen.SETTINGS && controller.settingsCategory != null) {
                    controller.settingsCategory = null
                } else {
                    controller.screen = BrowserScreen.BROWSER
                }
            }) { Text("‹ Indietro") }
            Text(
                when (controller.screen) {
                    BrowserScreen.TABS -> "${controller.tabs.size} schede"
                    BrowserScreen.BOOKMARKS -> "Preferiti"
                    BrowserScreen.HISTORY -> "Cronologia"
                    BrowserScreen.SETTINGS -> controller.settingsCategory?.title ?: "Impostazioni"
                    BrowserScreen.READER -> "Modalità lettura"
                    BrowserScreen.BROWSER -> "COSMIC"
                },
                modifier = Modifier.weight(1f),
                color = CosmicText,
                fontSize = 18.sp,
                fontWeight = FontWeight.SemiBold
            )
            if (controller.screen == BrowserScreen.TABS) {
                TextButton(onClick = {
                    controller.tabs.toList().forEach(controller::closeTab)
                }) { Text("Chiudi tutte") }
            }
        }
    }
}

@Composable
private fun AddressBar(controller: BrowserController, tab: BrowserTab) {
    var text by remember(tab.id) { mutableStateOf(tab.url) }
    LaunchedEffect(tab.id, tab.url) {
        text = tab.url
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(CosmicBackground)
            .padding(horizontal = 12.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        val secure = tab.url.startsWith("https://", ignoreCase = true)
        Text(
            if (secure) "●" else "◎",
            modifier = Modifier.padding(horizontal = 8.dp),
            color = if (secure) Color(0xFF87D6AE) else CosmicMuted
        )
        OutlinedTextField(
            value = text,
            onValueChange = { text = it },
            modifier = Modifier.weight(1f),
            singleLine = true,
            placeholder = { Text("Cerca o inserisci un indirizzo", color = CosmicMuted) },
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Go),
            keyboardActions = KeyboardActions(onGo = { controller.navigate(text) }),
            colors = OutlinedTextFieldDefaults.colors(
                focusedTextColor = CosmicText,
                unfocusedTextColor = CosmicText,
                focusedBorderColor = CosmicAccent,
                unfocusedBorderColor = CosmicBorder,
                cursorColor = CosmicAccent,
                focusedContainerColor = CosmicField,
                unfocusedContainerColor = CosmicField
            ),
            shape = RoundedCornerShape(24.dp)
        )
        TextButton(onClick = { controller.navigate(text) }) { Text("Vai") }
    }
}

@Composable
private fun BrowserContent(controller: BrowserController, tab: BrowserTab, modifier: Modifier) {
    if (tab.url.isBlank()) {
        HomeScreen(controller, modifier)
    } else {
        if (tab.progress in 1..99) {
            LinearProgressIndicator(
                progress = { tab.progress / 100f },
                modifier = Modifier.fillMaxWidth(),
                color = CosmicAccent,
                trackColor = CosmicSurface
            )
        }
        val webView = remember(tab.id) { controller.webViewFor(tab) }
        androidx.compose.ui.viewinterop.AndroidView(
            factory = {
                val current = tab.webView ?: webView.also { tab.webView = it }
                (current.parent as? ViewGroup)?.removeView(current)
                current
            },
            modifier = modifier,
            update = { }
        )
    }
}

@Composable
private fun HomeScreen(controller: BrowserController, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(CosmicBackground)
            .padding(horizontal = 24.dp, vertical = 36.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("◉", color = CosmicAccent, fontSize = 54.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(12.dp))
        Text("Il web, al tuo ritmo.", color = CosmicText, fontSize = 24.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(8.dp))
        Text("Cerca qualcosa o apri un nuovo spazio.", color = CosmicMuted)
        Spacer(Modifier.height(28.dp))
        if (controller.bookmarks.isEmpty()) {
            Text("I tuoi siti salvati compariranno qui.", color = CosmicMuted)
        } else {
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(controller.bookmarks.take(8), key = { it.url }) { page ->
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { controller.openBookmark(page) },
                        color = CosmicCard,
                        shape = RoundedCornerShape(14.dp)
                    ) {
                        Column(Modifier.padding(14.dp)) {
                            Text(page.title, color = CosmicText, fontWeight = FontWeight.SemiBold)
                            Text(Uri.parse(page.url).host ?: page.url, color = CosmicMuted, fontSize = 12.sp)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun TabsScreen(controller: BrowserController, modifier: Modifier) {
    var query by remember { mutableStateOf("") }
    val filteredTabs = controller.tabs.filter {
        matchesTab(it.title, it.url, query)
    }
    Column(modifier) {
        OutlinedTextField(
            value = query,
            onValueChange = { query = it },
            modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp),
            singleLine = true,
            label = { Text("Cerca schede") }
        )
        if (filteredTabs.isEmpty()) {
            EmptyMessage("Nessuna scheda trovata.")
        }
    LazyColumn(
        modifier = Modifier.weight(1f),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        items(filteredTabs, key = { it.id }) { tab ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { controller.selectTab(tab) },
                colors = CardDefaults.cardColors(
                    containerColor = if (tab.id == controller.activeTabId) Color(0xFF26233C) else CosmicCard
                )
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(tab.title, color = CosmicText, fontWeight = FontWeight.SemiBold, maxLines = 1)
                        Text(tab.url.ifBlank { "Nuova scheda" }, color = CosmicMuted, fontSize = 12.sp, maxLines = 1)
                    }
                    TextButton(onClick = { controller.closeTab(tab) }) { Text("Chiudi") }
                }
            }
        }
    }
    }
}

@Composable
private fun ReaderScreen(controller: BrowserController, modifier: Modifier) {
    Column(
        modifier = modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(20.dp)
    ) {
        Text(controller.readerTitle, color = CosmicText, fontSize = 24.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(20.dp))
        Text(controller.readerText, color = CosmicText, fontSize = 18.sp, lineHeight = 28.sp)
    }
}

@Composable
private fun BookmarksScreen(controller: BrowserController, modifier: Modifier) {
    Column(modifier) {
        Button(
            onClick = {
                controller.screen = BrowserScreen.BROWSER
                controller.toggleBookmark()
                controller.screen = BrowserScreen.BOOKMARKS
            },
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 10.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF26233C))
        ) { Text("＋  Salva pagina corrente") }
        if (controller.bookmarks.isEmpty()) {
            EmptyMessage("Non hai ancora salvato preferiti.")
        } else {
            LazyColumn(
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp, vertical = 4.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(controller.bookmarks.toList(), key = { it.url }) { page ->
                    PageRow(
                        title = page.title,
                        subtitle = page.url,
                        onOpen = { controller.openBookmark(page) },
                        onRemove = { controller.removeBookmark(page) }
                    )
                }
            }
        }
    }
}

@Composable
private fun HistoryScreen(controller: BrowserController, modifier: Modifier) {
    Column(modifier) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Pagine visitate", modifier = Modifier.weight(1f), color = CosmicMuted)
            TextButton(onClick = controller::clearHistory) { Text("Cancella cronologia") }
        }
        if (controller.history.isEmpty()) {
            EmptyMessage("La cronologia è vuota.")
        } else {
            LazyColumn(
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp, vertical = 4.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(controller.history.toList(), key = { "${it.visitedAt}:${it.url}" }) { entry ->
                    PageRow(entry.title, entry.url, onOpen = { controller.openHistory(entry) })
                }
            }
        }
    }
}

@Composable
private fun SettingsScreen(controller: BrowserController, modifier: Modifier) {
    val category = controller.settingsCategory
    LazyColumn(
        modifier = modifier,
        contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        if (category == null) {
            items(SettingsCategory.entries, key = { it.name }) { entry ->
                Surface(
                    modifier = Modifier.fillMaxWidth().clickable { controller.settingsCategory = entry },
                    color = CosmicCard,
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Text(entry.title, color = CosmicText, fontWeight = FontWeight.SemiBold)
                        Text(entry.description, color = CosmicMuted, fontSize = 12.sp)
                    }
                }
            }
        } else when (category) {
            SettingsCategory.GENERAL -> item {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text("Ripristina la sessione precedente", color = CosmicText)
                        Text("Riapre le schede all'avvio. Se disattivato, le schede salvate vengono rimosse.",
                            color = CosmicMuted, fontSize = 12.sp)
                    }
                    Switch(checked = controller.restoreSession, onCheckedChange = controller::updateRestoreSession)
                }
            }
            SettingsCategory.SEARCH -> {
                item { Text("Motore di ricerca predefinito", color = CosmicText) }
                items(searchEngines, key = { it.id }) { engine ->
                    Surface(
                        modifier = Modifier.fillMaxWidth().clickable { controller.chooseSearchEngine(engine.id) },
                        color = CosmicCard,
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(selected = controller.searchEngineId == engine.id,
                                onClick = { controller.chooseSearchEngine(engine.id) })
                            Text(engine.name, color = CosmicText)
                        }
                    }
                }
                item { CustomSearchSetting(controller) }
            }
            SettingsCategory.PRIVACY -> item {
                Column {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("Blocca domini noti", color = CosmicText)
                            Text("Usa la lista inclusa; non blocca tutti gli annunci o tracker.",
                                color = CosmicMuted, fontSize = 12.sp)
                        }
                        Switch(checked = controller.blockKnownHosts, onCheckedChange = controller::updateBlockKnownHosts)
                    }
                    SettingsNotice("Do Not Track, cookie solo per la sessione e gestione dei permessi per sito non sono ancora configurabili su Android.")
                }
            }
            SettingsCategory.APPEARANCE -> item {
                SettingsNotice("Colori personalizzati, posizione delle schede e personalizzazione della barra non sono ancora disponibili su Android.")
            }
            SettingsCategory.PASSWORDS -> item {
                SettingsNotice("Il vault delle password non è ancora integrato nella versione Android.")
            }
            SettingsCategory.USERS -> item {
                SettingsNotice("I profili isolati non sono ancora disponibili su Android.")
            }
            SettingsCategory.ADVANCED -> item {
                Column {
                    SettingsNotice("L'aggiornamento automatico delle pagine HTTP a HTTPS e il ripristino delle impostazioni non sono ancora disponibili. I contenuti misti sono bloccati.")
                    Button(
                        onClick = { controller.showClearDialog = true },
                        modifier = Modifier.fillMaxWidth().padding(top = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = CosmicCard)
                    ) { Text("Cancella cookie, cache e dati dei siti", color = CosmicText) }
                }
            }
            SettingsCategory.UPDATES -> item {
                SettingsNotice("Cosmic Android ${controller.versionName}. La verifica e l'installazione automatica degli aggiornamenti non sono ancora disponibili.")
            }
        }
    }
}

@Composable
private fun SettingsNotice(message: String) {
    Text(message, modifier = Modifier.fillMaxWidth().padding(top = 8.dp)
        .background(CosmicCard, RoundedCornerShape(12.dp)).padding(14.dp), color = CosmicMuted)
}

@Composable
private fun CustomSearchSetting(controller: BrowserController) {
    var template by remember { mutableStateOf(controller.customSearchTemplate) }
    var error by remember { mutableStateOf(false) }
    Column {
        Spacer(Modifier.height(12.dp))
        Text(if (controller.searchEngineId == "custom") "Motore personalizzato attivo" else "Motore personalizzato",
            color = CosmicText, fontWeight = FontWeight.SemiBold)
        Text("Inserisci un indirizzo HTTP o HTTPS con %1 al posto dei termini di ricerca.",
            color = CosmicMuted, fontSize = 12.sp)
        OutlinedTextField(
            value = template,
            onValueChange = { template = it; error = false },
            modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
            label = { Text("URL di ricerca") },
            placeholder = { Text("https://example.com/search?q=%1") },
            singleLine = true,
            isError = error,
            supportingText = if (error) ({ Text("Inserisci un URL valido contenente %1.") }) else null
        )
        Button(onClick = { error = !controller.chooseCustomSearch(template) }) {
            Text("Salva e usa")
        }
    }
}

@Composable
private fun NavigationBar(controller: BrowserController) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(CosmicSurface)
            .padding(horizontal = 6.dp, vertical = 4.dp)
            .padding(bottom = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically
    ) {
        NavButton("←", "Indietro", controller::goBack)
        NavButton("→", "Avanti", controller::goForward)
        NavButton("⌂", "Home", controller::goHome)
        NavButton("${controller.tabs.size}", "Schede") { controller.screen = BrowserScreen.TABS }
        NavButton("☆", "Preferiti") { controller.screen = BrowserScreen.BOOKMARKS }
        NavButton("⚙", "Impostazioni") {
            controller.settingsCategory = null
            controller.screen = BrowserScreen.SETTINGS
        }
    }
}

@Composable
private fun NavButton(symbol: String, description: String, onClick: () -> Unit) {
    Column(
        modifier = Modifier
            .size(width = 56.dp, height = 48.dp)
            .clickable(onClick = onClick)
            .padding(2.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(symbol, color = CosmicText, fontSize = 18.sp, lineHeight = 20.sp)
        Text(description, color = CosmicMuted, fontSize = 9.sp, lineHeight = 11.sp, maxLines = 1)
    }
}

@Composable
private fun PageRow(
    title: String,
    subtitle: String,
    onOpen: () -> Unit,
    onRemove: (() -> Unit)? = null
) {
    Card(colors = CardDefaults.cardColors(containerColor = CosmicCard)) {
        Row(
            modifier = Modifier.fillMaxWidth().clickable(onClick = onOpen).padding(start = 14.dp, top = 12.dp, bottom = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(Modifier.weight(1f)) {
                Text(title, color = CosmicText, fontWeight = FontWeight.SemiBold, maxLines = 1)
                Text(subtitle, color = CosmicMuted, fontSize = 12.sp, maxLines = 1)
            }
            onRemove?.let {
                TextButton(onClick = it) { Text("Rimuovi") }
            }
        }
    }
}

@Composable
private fun EmptyMessage(message: String) {
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Text(message, color = CosmicMuted)
    }
}
